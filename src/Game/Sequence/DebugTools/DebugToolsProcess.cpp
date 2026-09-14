#include "DebugToolsProcess.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/ProcessList.hpp"
#include "Game/Sequence/Test/DebugUtils.hpp"
#include "Game/Component/Enemy/Enemy.hpp"
#include "Game/Component/Enemy/EnemyCharacter.hpp"
#include "Game/Component/GameData/GameData.hpp"
#include "Game/Component/GameMain/DebugDifficulty.hpp"
#include "Game/Component/GameMain/GamePlayer.hpp"
#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/GameMain/GameStage.hpp"
#include "Game/Component/GameMain/GameStageDebug.hpp"
#include "Game/Component/Player/PlayerCharacter.hpp"
#include "Game/System/GameObject/GameObject.hpp"
#include "Game/System/GameObject/GameObjectManager.hpp"
#include "Game/System/Hit/HitDebug.hpp"
#include "Game/System/Map/MapCamera.hpp"
#include "Game/System/Misc/ControllerMisc.hpp"
#include "Game/System/Misc/Gamepad.hpp"
#include "System/Common/Process/ProcessMail.hpp"
#include "System/Common/Process/Sequence.hpp"
#include "System/Common/Screen.hpp"
#include "System/PC/PCPhysicalControllerKey.hpp"
#include "System/PC/PCSpecific.hpp"

#ifdef GetObject
#undef GetObject
#endif /* GetObject */

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <vector>


namespace
{
    enum PAGE
    {
        PAGE_DIFFICULTY = 0,
        PAGE_PLAYER,
        PAGE_ENEMY_AI,
        PAGE_STAGE,
        PAGE_CAMERA,
        PAGE_HITBOXES,
        PAGE_TELEMETRY,

        PAGE_NUM,
    };


    static const char* s_apszPageName[PAGE_NUM] =
    {
        "Difficulty",
        "Player",
        "Enemy/AI",
        "Stage",
        "Camera",
        "Hitboxes",
        "Telemetry",
    };


    static const char* s_apszDifficultySettingName[CDebugDifficulty::SETTING_NUM] =
    {
        "Enemy damage received",
        "Enemy HP",
        "Player damage received",
        "Attack frequency",
        "Guard frequency",
        "Projectile frequency",
        "Projectile range",
        "AI thinking rate",
        "AI activity",
        "AI awareness",
        "Attack interval",
        "Knockback threshold",
    };


    static const char* s_apszDifficultyDescription[CDebugDifficulty::SETTING_NUM] =
    {
        "Scales damage applied to enemies from all incoming character attacks.",
        "Scales maximum HP when an enemy is created; living enemy HP is preserved.",
        "Scales final damage received by players after guard and defence modifiers.",
        "Scales standard-enemy attack decision frequencies.",
        "Scales standard-enemy guard decision frequencies independently of attacks.",
        "Scales standard-enemy aiming, range-rate, and repeat-fire frequencies.",
        "Scales the standard-enemy projectile range parameter.",
        "Scales how quickly current and future enemies reconsider AI decisions.",
        "Scales the chance that current and future enemies act aggressively.",
        "Scales front and rear awareness for current and future enemies.",
        "Scales AI attack cooldowns when each enemy next assigns its interval.",
        "Scales damage required to knock back or stagger enemies.",
    };


    static const char* s_apszDifficultyScope[CDebugDifficulty::SETTING_NUM] =
    {
        "LIVE",
        "NEXT SPAWN",
        "LIVE",
        "LIVE - STANDARD ENEMIES",
        "LIVE - STANDARD ENEMIES",
        "LIVE - STANDARD ENEMIES",
        "LIVE - STANDARD ENEMIES",
        "LIVE",
        "LIVE",
        "LIVE",
        "NEXT AI INTERVAL",
        "LIVE",
    };


    static bool IsGameplaySequence(int32 label)
    {
        switch (label)
        {
        case PROCLABEL_SEQ_NORMALSTAGE:
        case PROCLABEL_SEQ_RIDESTAGE:
        case PROCLABEL_SEQ_NEXUSSTAGE:
        case PROCLABEL_SEQ_PLAYDEMO:
        case PROCLABEL_SEQ_HOMESTAGE:
            return true;

        default:
            return false;
        };
    };


    static const char* DifficultyName(GAMETYPES::DIFFICULTY difficulty)
    {
        switch (difficulty)
        {
        case GAMETYPES::DIFFICULTY_EASY:       return "Easy";
        case GAMETYPES::DIFFICULTY_NORMAL:     return "Normal";
        case GAMETYPES::DIFFICULTY_HARD:       return "Hard";
        case GAMETYPES::DIFFICULTY_VERY_HARD:  return "Very Hard";
        case GAMETYPES::DIFFICULTY_EXTREME:    return "Extreme";
        case GAMETYPES::DIFFICULTY_SOULS_LIKE: return "Souls Like";
        default:                               return "Unknown";
        };
    };


    static bool ContainsHandle(const std::vector<uint32>& handles, uint32 handle)
    {
        return (std::find(handles.begin(), handles.end(), handle) != handles.end());
    };


    static uint32 ControllerDigital(void)
    {
        return CController::GetDigital(CController::CONTROLLER_LOCKED_ON_VIRTUAL) |
               CController::GetDigital(CController::CONTROLLER_UNLOCKED_ON_VIRTUAL);
    };


    static uint32 ControllerDigitalTrigger(void)
    {
        return CController::GetDigitalTrigger(CController::CONTROLLER_LOCKED_ON_VIRTUAL) |
               CController::GetDigitalTrigger(CController::CONTROLLER_UNLOCKED_ON_VIRTUAL);
    };


    static uint32 ControllerDigitalRepeat(void)
    {
        return CController::GetDigitalRepeat(CController::CONTROLLER_LOCKED_ON_VIRTUAL) |
               CController::GetDigitalRepeat(CController::CONTROLLER_UNLOCKED_ON_VIRTUAL);
    };


    static int16 ControllerAnalog(CController::ANALOG analog)
    {
        int16 locked = CController::GetAnalog(CController::CONTROLLER_LOCKED_ON_VIRTUAL, analog);
        int16 unlocked = CController::GetAnalog(CController::CONTROLLER_UNLOCKED_ON_VIRTUAL, analog);
        int32 lockedMagnitude = (locked < 0 ? -static_cast<int32>(locked) : static_cast<int32>(locked));
        int32 unlockedMagnitude = (unlocked < 0 ? -static_cast<int32>(unlocked) : static_cast<int32>(unlocked));
        return (lockedMagnitude >= unlockedMagnitude ? locked : unlocked);
    };


    static bool IsAnalogCameraInput(int16 value)
    {
        const int32 DEADZONE = static_cast<int32>(TYPEDEF::SINT16_MAX * 0.30f);
        int32 magnitude = (value < 0 ? -static_cast<int32>(value) : static_cast<int32>(value));
        return (magnitude > DEADZONE);
    };
};


class CDebugToolsProcess::CImpl
{
public:
    CImpl(void)
    : m_font()
    , m_bOpen(false)
    , m_bGodMode(false)
    , m_bEnemyAIPaused(false)
    , m_bTelemetryEnabled(false)
    , m_bSavedPosition(false)
    , m_bControllerMenuComboDown(false)
    , m_bShowcaseCamera(false)
    , m_page(PAGE_DIFFICULTY)
    , m_aiSelection()
    , m_iPausedLabel(PROCESSTYPES::LABEL_EOL)
    , m_iStepFrames(0)
    , m_iScreenshotDelay(0)
    , m_pGodModeStage(nullptr)
    , m_pCameraStage(nullptr)
    , m_vSavedPosition(Math::VECTOR3_ZERO)
    , m_fSavedDirection(0.0f)
    , m_iCameraMode(CMapCamera::MODE_AUTOCHANGE)
    , m_aiResumeHandles()
    {
        std::memset(m_aiSelection, 0x00, sizeof(m_aiSelection));
    };


    ~CImpl(void)
    {
        CGameStageDebug::CAMERA_MENU_CONTROL = false;
        SetEnemyAIPaused(false);
        SetGodMode(false);
    };


    void Move(CDebugToolsProcess& owner)
    {
        UpdateStep(owner);
        UpdatePersistentTools();
        UpdateScreenshot();

        int32 currentLabel = CSequence::GetCurrently();
        bool bAvailable = IsGameplaySequence(currentLabel);
        uint32 controllerDigital = ControllerDigital();
        const uint32 controllerMenuCombo = CController::DIGITAL_SELECT |
                                           CController::DIGITAL_R2;
        bool bControllerMenuComboDown =
            ((controllerDigital & controllerMenuCombo) == controllerMenuCombo);
        bool bControllerToggle = bControllerMenuComboDown &&
                                 !m_bControllerMenuComboDown;
        m_bControllerMenuComboDown = bControllerMenuComboDown;

        if (m_bOpen && !bAvailable)
        {
            Close(owner);
            return;
        };

        if (CPCSpecific::IsKeyTrigger(DIK_F4) || bControllerToggle)
        {
            if (m_bOpen)
                Close(owner);
            else if (bAvailable)
                Open(owner, currentLabel);
        };

        UpdateShowcaseCamera();

        if (!m_bOpen)
            return;

        uint32 controllerTrigger = ControllerDigitalTrigger();
        uint32 controllerNavigation = controllerTrigger | ControllerDigitalRepeat();

        if (CPCSpecific::IsKeyTrigger(DIK_ESCAPE) ||
            (controllerTrigger & CController::DIGITAL_RLEFT))
        {
            Close(owner);
            return;
        };

        if (CPCSpecific::IsKeyTrigger(DIK_Q) ||
            (controllerTrigger & CController::DIGITAL_L1))
            ChangePage(-1);
        else if (CPCSpecific::IsKeyTrigger(DIK_E) ||
                 (controllerTrigger & CController::DIGITAL_R1))
            ChangePage(1);
        else if (CPCSpecific::IsKeyTrigger(DIK_UP) ||
                 (controllerNavigation & CController::DIGITAL_LUP))
            ChangeSelection(-1);
        else if (CPCSpecific::IsKeyTrigger(DIK_DOWN) ||
                 (controllerNavigation & CController::DIGITAL_LDOWN))
            ChangeSelection(1);
        else if (CPCSpecific::IsKeyTrigger(DIK_LEFT) ||
                 (controllerNavigation & CController::DIGITAL_LLEFT))
            AdjustSelected(-1, 1);
        else if (CPCSpecific::IsKeyTrigger(DIK_RIGHT) ||
                 (controllerNavigation & CController::DIGITAL_LRIGHT))
            AdjustSelected(1, 1);
        else if (CPCSpecific::IsKeyTrigger(DIK_PRIOR))
            AdjustSelected(1, 5);
        else if (CPCSpecific::IsKeyTrigger(DIK_NEXT))
            AdjustSelected(-1, 5);
        else if (CPCSpecific::IsKeyTrigger(DIK_RETURN) ||
                 (controllerTrigger & CController::DIGITAL_RDOWN))
            ActivateSelected(owner);
        else if (CPCSpecific::IsKeyTrigger(DIK_BACK) ||
                 (controllerTrigger & CController::DIGITAL_RRIGHT))
            ResetSelected();
        else if (CPCSpecific::IsKeyTrigger(DIK_DELETE) ||
                 (controllerTrigger & CController::DIGITAL_RUP))
            ResetPage();
    };


    void Draw(void) const
    {
        int32 currentLabel = CSequence::GetCurrently();
        if (!IsGameplaySequence(currentLabel))
            return;

        if (m_bOpen)
        {
            DrawMenu();
            DrawTelemetry(356, 80);
        }
        else
        {
            DrawHint();
            if (m_bTelemetryEnabled)
                DrawTelemetry(12, 30);
        };
    };

private:
    void Open(CDebugToolsProcess& owner, int32 label)
    {
        m_bOpen = true;
        m_iPausedLabel = label;
        m_iStepFrames = 0;
        owner.Mail().Send(m_iPausedLabel, PROCESSTYPES::MAIL::TYPE_MOVE_DISABLE);
        EnableStickToDirButton(true);
        CGameStageDebug::CAMERA_MENU_CONTROL = true;
    };


    void Close(CDebugToolsProcess& owner)
    {
        if (!m_bOpen)
            return;

        if (m_iPausedLabel != PROCESSTYPES::LABEL_EOL)
            owner.Mail().Send(m_iPausedLabel, PROCESSTYPES::MAIL::TYPE_MOVE_ENABLE);

        m_bOpen = false;
        m_iPausedLabel = PROCESSTYPES::LABEL_EOL;
        m_iStepFrames = 0;
        EnableStickToDirButton(false);
        CGameStageDebug::CAMERA_MENU_CONTROL = false;
    };


    void UpdateStep(CDebugToolsProcess& owner)
    {
        if (m_iStepFrames <= 0)
            return;

        --m_iStepFrames;
        if ((m_iStepFrames == 0) && m_bOpen &&
            (m_iPausedLabel != PROCESSTYPES::LABEL_EOL))
        {
            owner.Mail().Send(m_iPausedLabel, PROCESSTYPES::MAIL::TYPE_MOVE_DISABLE);
        };
    };


    void StartStep(CDebugToolsProcess& owner, int32 frames)
    {
        if (!m_bOpen || (m_iStepFrames > 0) ||
            (m_iPausedLabel == PROCESSTYPES::LABEL_EOL))
        {
            return;
        };

        m_iStepFrames = Clamp(frames, 1, 60);
        owner.Mail().Send(m_iPausedLabel, PROCESSTYPES::MAIL::TYPE_MOVE_ENABLE);
    };


    void ChangePage(int32 direction)
    {
        int32 page = InvClamp(static_cast<int32>(m_page) + direction, 0, PAGE_NUM - 1);
        m_page = static_cast<PAGE>(page);

        int32 itemCount = GetItemCount(m_page);
        if (m_aiSelection[m_page] >= itemCount)
            m_aiSelection[m_page] = (itemCount - 1);
    };


    void ChangeSelection(int32 direction)
    {
        int32 itemCount = GetItemCount(m_page);
        if (itemCount <= 0)
            return;

        m_aiSelection[m_page] = InvClamp(
            m_aiSelection[m_page] + direction,
            0,
            itemCount - 1
        );
    };


    void AdjustSelected(int32 direction, int32 multiplier)
    {
        int32 selected = m_aiSelection[m_page];
        GAMETYPES::DIFFICULTY difficulty = CGameData::Option().Play().GetDifficulty();

        switch (m_page)
        {
        case PAGE_DIFFICULTY:
            if (selected == 0)
            {
                int32 preset = static_cast<int32>(CDebugDifficulty::GetPreset());
                if (preset == CDebugDifficulty::PRESET_CUSTOM)
                    preset = (direction > 0 ? CDebugDifficulty::PRESET_GAME : CDebugDifficulty::PRESET_SOULS_LIKE);
                else
                    preset = InvClamp(preset + direction, 0, CDebugDifficulty::PRESET_NUM - 1);

                CDebugDifficulty::SetPreset(
                    static_cast<CDebugDifficulty::PRESET>(preset),
                    difficulty
                );
            }
            else if ((selected >= 1) && (selected <= CDebugDifficulty::SETTING_NUM))
            {
                CDebugDifficulty::SETTING setting =
                    static_cast<CDebugDifficulty::SETTING>(selected - 1);
                float delta = CDebugDifficulty::GetStep(setting) *
                              static_cast<float>(direction * multiplier);
                CDebugDifficulty::AdjustValue(setting, delta, difficulty);
            };
            break;

        case PAGE_PLAYER:
            if (selected == 0)
                SetGodMode(!m_bGodMode);
            break;

        case PAGE_ENEMY_AI:
            if (selected == 0)
                SetEnemyAIPaused(!m_bEnemyAIPaused);
            break;

        case PAGE_CAMERA:
            if (selected == 0)
            {
                float delta = 0.05f * static_cast<float>(direction * multiplier);
                CGameStageDebug::CAMERA_ZOOM_SCALE = Clamp(
                    CGameStageDebug::CAMERA_ZOOM_SCALE + delta,
                    0.25f,
                    3.00f
                );
            }
            else if (selected == 1)
            {
                m_iCameraMode = InvClamp(
                    m_iCameraMode + direction,
                    0,
                    CMapCamera::MODEMAX - 1
                );
                m_bShowcaseCamera = (m_iCameraMode == CMapCamera::MODE_MANUAL);
                ApplyCameraMode();
            };
            break;

        case PAGE_HITBOXES:
            ToggleHitbox(selected);
            break;

        case PAGE_TELEMETRY:
            if (selected == 0)
                m_bTelemetryEnabled = !m_bTelemetryEnabled;
            break;

        default:
            break;
        };
    };


    void ActivateSelected(CDebugToolsProcess& owner)
    {
        int32 selected = m_aiSelection[m_page];

        switch (m_page)
        {
        case PAGE_DIFFICULTY:
            if (selected == 0)
                AdjustSelected(1, 1);
            else if (selected == (CDebugDifficulty::SETTING_NUM + 1))
                CDebugDifficulty::ResetAll(CGameData::Option().Play().GetDifficulty());
            break;

        case PAGE_PLAYER:
            RunPlayerAction(selected);
            break;

        case PAGE_ENEMY_AI:
            if (selected == 0)
                SetEnemyAIPaused(!m_bEnemyAIPaused);
            else if (selected == 1)
                KillAllEnemies();
            break;

        case PAGE_STAGE:
            RunStageAction(owner, selected);
            break;

        case PAGE_CAMERA:
            if (selected == 1)
                ApplyCameraMode();
            else if (selected == 2)
                ResetShowcaseCamera();
            break;

        case PAGE_HITBOXES:
            ToggleHitbox(selected);
            break;

        case PAGE_TELEMETRY:
            if (selected == 0)
                m_bTelemetryEnabled = !m_bTelemetryEnabled;
            else if (selected == 1)
                m_iScreenshotDelay = 3;
            break;

        default:
            break;
        };
    };


    void ResetSelected(void)
    {
        int32 selected = m_aiSelection[m_page];
        GAMETYPES::DIFFICULTY difficulty = CGameData::Option().Play().GetDifficulty();

        switch (m_page)
        {
        case PAGE_DIFFICULTY:
            if (selected == 0)
                CDebugDifficulty::ResetAll(difficulty);
            else if ((selected >= 1) && (selected <= CDebugDifficulty::SETTING_NUM))
                CDebugDifficulty::ResetSetting(
                    static_cast<CDebugDifficulty::SETTING>(selected - 1),
                    difficulty
                );
            break;

        case PAGE_PLAYER:
            if (selected == 0)
                SetGodMode(false);
            break;

        case PAGE_ENEMY_AI:
            if (selected == 0)
                SetEnemyAIPaused(false);
            break;

        case PAGE_CAMERA:
            if (selected == 0)
                CGameStageDebug::CAMERA_ZOOM_SCALE = 1.00f;
            else
                ResetShowcaseCamera();
            break;

        case PAGE_HITBOXES:
            SetHitbox(selected, false);
            break;

        case PAGE_TELEMETRY:
            if (selected == 0)
                m_bTelemetryEnabled = false;
            break;

        default:
            break;
        };
    };


    void ResetPage(void)
    {
        switch (m_page)
        {
        case PAGE_DIFFICULTY:
            CDebugDifficulty::ResetAll(CGameData::Option().Play().GetDifficulty());
            break;

        case PAGE_PLAYER:
            SetGodMode(false);
            m_bSavedPosition = false;
            break;

        case PAGE_ENEMY_AI:
            SetEnemyAIPaused(false);
            break;

        case PAGE_CAMERA:
            ResetShowcaseCamera();
            break;

        case PAGE_HITBOXES:
            CHitDebug::SHOW_HIT_ATTACK = false;
            CHitDebug::SHOW_HIT_CATCH = false;
            CHitDebug::SHOW_HIT_BODY = false;
            break;

        case PAGE_TELEMETRY:
            m_bTelemetryEnabled = false;
            break;

        default:
            break;
        };
    };


    void RunPlayerAction(int32 selected)
    {
        if (selected == 0)
        {
            SetGodMode(!m_bGodMode);
            return;
        };

        if (CGameProperty::GetPlayerNum() <= 0)
            return;

        IGamePlayer* pPlayer = CGameProperty::Player(0);
        if (!pPlayer || !pPlayer->IsAlive())
            return;

        switch (selected)
        {
        case 1:
            pPlayer->AddHP(pPlayer->GetHPMax());
            break;

        case 2:
            pPlayer->AddHP(100);
            break;

        case 3:
            pPlayer->AddHP(-100);
            break;

        case 4:
            pPlayer->AddShurikenNum(pPlayer->GetShurikenMax());
            break;

        case 5:
            pPlayer->GetPosition(&m_vSavedPosition);
            m_fSavedDirection = pPlayer->GetRotY();
            m_bSavedPosition = true;
            break;

        case 6:
            if (m_bSavedPosition)
                pPlayer->Relocation(&m_vSavedPosition, m_fSavedDirection, false);
            break;

        case 7:
            {
                RwV3d position = Math::VECTOR3_ZERO;
                pPlayer->GetPosition(&position);
                float direction = pPlayer->GetRotY();
                int32 playerNum = CGameProperty::GetPlayerNum();
                for (int32 i = 0; i < playerNum; ++i)
                {
                    IGamePlayer* pOther = CGameProperty::Player(i);
                    if (pOther && pOther->IsAlive())
                        pOther->Relocation(&position, direction, false);
                };
            }
            break;

        default:
            break;
        };
    };


    void RunStageAction(CDebugToolsProcess& owner, int32 selected)
    {
        CGameStage* pStage = CGameStage::GetCurrent();
        if (!pStage)
            return;

        switch (selected)
        {
        case 0:
            StartStep(owner, 1);
            break;

        case 1:
            StartStep(owner, 10);
            break;

        case 2:
            pStage->NotifyGameClear(CGamePlayResult::CLEARSUB_A);
            break;

        case 3:
            pStage->NotifyGameClear(CGamePlayResult::CLEARSUB_B);
            break;

        case 4:
            pStage->NotifyGameOver();
            break;

        default:
            break;
        };
    };


    void SetGodMode(bool bEnabled)
    {
        if (m_bGodMode == bEnabled)
            return;

        m_bGodMode = bEnabled;
        CGameStage* pStage = CGameStage::GetCurrent();

        if (!m_bGodMode)
        {
            if (pStage && (m_pGodModeStage == pStage))
                pStage->EndPlayerNegateDamager();
            m_pGodModeStage = nullptr;
            CGameStageDebug::GODMODE = false;
        }
        else
        {
            ApplyGodModeToCurrentStage();
        };
    };


    void ApplyGodModeToCurrentStage(void)
    {
        CGameStage* pStage = CGameStage::GetCurrent();
        if (!pStage)
        {
            m_pGodModeStage = nullptr;
            return;
        };

        if (m_pGodModeStage != pStage)
        {
            pStage->BeginPlayerNegateDamage();
            m_pGodModeStage = pStage;
        };

        CGameStageDebug::GODMODE = true;
    };


    void SetEnemyAIPaused(bool bPaused)
    {
        if (m_bEnemyAIPaused == bPaused)
            return;

        m_bEnemyAIPaused = bPaused;
        if (m_bEnemyAIPaused)
            CaptureAndStopEnemyAI();
        else
            ResumeCapturedEnemyAI();
    };


    void CaptureAndStopEnemyAI(void)
    {
        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!pEnemy)
                continue;

            CEnemyCharacter& character = pEnemy->Character();
            if (!character.IsRunningAI())
                continue;

            uint32 handle = pEnemy->GetHandle();
            if (!ContainsHandle(m_aiResumeHandles, handle))
                m_aiResumeHandles.push_back(handle);

            pEnemy->StopAI();
        };
    };


    void ResumeCapturedEnemyAI(void)
    {
        if (!CGameStage::GetCurrent())
        {
            m_aiResumeHandles.clear();
            return;
        };

        for (uint32 handle : m_aiResumeHandles)
        {
            CGameObject* pObject = CGameObjectManager::GetObject(handle);
            if (pObject && (pObject->GetType() == GAMEOBJECTTYPE::ENEMY))
                static_cast<CEnemy*>(pObject)->StartAI();
        };

        m_aiResumeHandles.clear();
    };


    void UpdatePersistentTools(void)
    {
        if (m_bGodMode)
            ApplyGodModeToCurrentStage();
        else
            CGameStageDebug::GODMODE = false;

        if (m_bEnemyAIPaused)
            CaptureAndStopEnemyAI();
    };


    void KillAllEnemies(void)
    {
        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (pEnemy)
                pEnemy->Kill();
        };
    };


    void ApplyCameraMode(void)
    {
        CGameStage* pStage = CGameStage::GetCurrent();
        if (!pStage)
            return;

        CMapCamera* pCamera = pStage->GetMapCamera();
        if (pCamera)
        {
            if (m_iCameraMode == CMapCamera::MODE_MANUAL)
                pCamera->DebugBeginShowcase();
            else
                pCamera->SetCameraMode(static_cast<CMapCamera::MODE>(m_iCameraMode));
            m_pCameraStage = pStage;
        };
    };


    void ResetShowcaseCamera(void)
    {
        m_bShowcaseCamera = false;
        m_iCameraMode = CMapCamera::MODE_AUTOCHANGE;
        CGameStageDebug::CAMERA_ZOOM_SCALE = 1.00f;
        CGameStageDebug::CAMERA_SUPPRESS_SWITCH_TRIGGER = true;

        CGameStage* pStage = CGameStage::GetCurrent();
        m_pCameraStage = pStage;
        if (!pStage)
            return;

        CMapCamera* pCamera = pStage->GetMapCamera();
        if (!pCamera)
            return;

        pCamera->DebugResetShowcase(CGameProperty::GetPlayerNum() > 1 ?
            CMapCamera::PATHMODE_MULTIPLAYER :
            CMapCamera::PATHMODE_SINGLEPLAYER);

        if (m_bOpen && (m_iStepFrames <= 0))
            pStage->DebugUpdateCamera();
    };


    void UpdateShowcaseCamera(void)
    {
        CGameStage* pStage = CGameStage::GetCurrent();
        if (pStage != m_pCameraStage)
        {
            m_pCameraStage = pStage;
            m_bShowcaseCamera = false;
            m_iCameraMode = CMapCamera::MODE_AUTOCHANGE;
        };

        if (!pStage)
            return;

        uint32 controllerTrigger = ControllerDigitalTrigger();
        if (controllerTrigger & CController::DIGITAL_R3)
        {
            ResetShowcaseCamera();
            return;
        };

        int16 rightX = ControllerAnalog(CController::ANALOG_RSTICK_X);
        int16 rightY = ControllerAnalog(CController::ANALOG_RSTICK_Y);
        if (m_bOpen && (m_page == PAGE_CAMERA) &&
            (IsAnalogCameraInput(rightX) || IsAnalogCameraInput(rightY)))
        {
            m_bShowcaseCamera = true;
            m_iCameraMode = CMapCamera::MODE_MANUAL;
            ApplyCameraMode();
        };

        if (m_bOpen && m_bShowcaseCamera && (m_iStepFrames <= 0))
            pStage->DebugUpdateCamera();
    };


    void ToggleHitbox(int32 selected)
    {
        switch (selected)
        {
        case 0:
            CHitDebug::SHOW_HIT_ATTACK = !CHitDebug::SHOW_HIT_ATTACK;
            break;
        case 1:
            CHitDebug::SHOW_HIT_CATCH = !CHitDebug::SHOW_HIT_CATCH;
            break;
        case 2:
            CHitDebug::SHOW_HIT_BODY = !CHitDebug::SHOW_HIT_BODY;
            break;
        default:
            break;
        };
    };


    void SetHitbox(int32 selected, bool bEnabled)
    {
        switch (selected)
        {
        case 0: CHitDebug::SHOW_HIT_ATTACK = bEnabled; break;
        case 1: CHitDebug::SHOW_HIT_CATCH = bEnabled; break;
        case 2: CHitDebug::SHOW_HIT_BODY = bEnabled; break;
        default: break;
        };
    };


    void UpdateScreenshot(void)
    {
        if (m_iScreenshotDelay <= 0)
            return;

        --m_iScreenshotDelay;
        if (m_iScreenshotDelay == 0)
            CPCSpecific::MakeWindowScreenshotToClipboard();
    };


    int32 GetItemCount(PAGE page) const
    {
        switch (page)
        {
        case PAGE_DIFFICULTY: return CDebugDifficulty::SETTING_NUM + 2;
        case PAGE_PLAYER:     return 8;
        case PAGE_ENEMY_AI:   return 2;
        case PAGE_STAGE:      return 5;
        case PAGE_CAMERA:     return 3;
        case PAGE_HITBOXES:   return 3;
        case PAGE_TELEMETRY:  return 2;
        default:              return 0;
        };
    };


    void DrawHint(void) const
    {
        m_font.Background({ 0x00, 0x00, 0x00, 0xB0 });
        m_font.Color({ 0xC0, 0xE8, 0xFF, 0xFF });
        m_font.Position(12, 10);
        m_font.Print("F4 / BACK+RT  DEBUG TOOLS");
    };


    void DrawMenu(void) const
    {
        m_font.Background({ 0x08, 0x0C, 0x12, 0xE8 });
        m_font.Color({ 0x78, 0xD8, 0xFF, 0xFF });
        m_font.Position(18, 14);
        m_font.Print("TMNT2 DEBUG TOOLS  |  F4 or Back+RT menu  |  Esc close");

        char tabs[256];
        tabs[0] = '\0';
        for (int32 i = 0; i < PAGE_NUM; ++i)
        {
            char tab[40];
            std::snprintf(
                tab,
                sizeof(tab),
                (i == static_cast<int32>(m_page) ? "[%s] " : "%s "),
                s_apszPageName[i]
            );
            strcat_s(tabs, sizeof(tabs), tab);
        };

        m_font.Color({ 0xFF, 0xD0, 0x58, 0xFF });
        m_font.Position(18, 34);
        m_font.Print("%s", tabs);

        int32 itemCount = GetItemCount(m_page);
        int32 selected = m_aiSelection[m_page];
        for (int32 i = 0; i < itemCount; ++i)
        {
            char text[160];
            FormatItemText(i, text, sizeof(text));

            if (i == selected)
                m_font.Color({ 0xFF, 0xB0, 0x30, 0xFF });
            else
                m_font.Color({ 0xE8, 0xEE, 0xF8, 0xFF });

            m_font.Position(28, 66 + (i * 18));
            m_font.Print("%c %s", (i == selected ? '>' : ' '), text);
        };

        const char* scope = "";
        const char* description = "";
        GetItemHelp(selected, scope, description);

        int32 helpY = 66 + (itemCount * 18) + 12;
        m_font.Color({ 0x78, 0xD8, 0xFF, 0xFF });
        m_font.Position(18, helpY);
        m_font.Print("%s", scope);
        m_font.Color({ 0xD8, 0xE0, 0xEC, 0xFF });
        m_font.Position(18, helpY + 18);
        m_font.Print("%s", description);
        m_font.Color({ 0xA8, 0xB4, 0xC8, 0xFF });
        m_font.Position(18, helpY + 36);
        m_font.Print("Keys: Arrows adjust  PgUp/PgDn x5  Enter run  Backspace item reset  Delete page");
        m_font.Position(18, helpY + 54);
        m_font.Print("Pad: D-pad/LS navigate  LB/RB page  A run  B close  X reset  Y reset page");
        m_font.Position(18, helpY + 72);
        m_font.Print("Camera page: Right stick orbit  LT/RT zoom  R3 default camera");
    };


    void FormatItemText(int32 index, char* buffer, size_t capacity) const
    {
        buffer[0] = '\0';

        switch (m_page)
        {
        case PAGE_DIFFICULTY:
            if (index == 0)
            {
                std::snprintf(buffer, capacity, "Preset                         %s", CDebugDifficulty::GetPresetName());
            }
            else if ((index >= 1) && (index <= CDebugDifficulty::SETTING_NUM))
            {
                CDebugDifficulty::SETTING setting =
                    static_cast<CDebugDifficulty::SETTING>(index - 1);
                GAMETYPES::DIFFICULTY difficulty = CGameData::Option().Play().GetDifficulty();
                float value = CDebugDifficulty::GetValue(setting, difficulty);
                std::snprintf(
                    buffer,
                    capacity,
                    "%-30s %5.2fx%s",
                    s_apszDifficultySettingName[setting],
                    value,
                    (CDebugDifficulty::IsOverrideEnabled() ? " *" : "")
                );
            }
            else
            {
                std::snprintf(buffer, capacity, "Reset all to game difficulty");
            };
            break;

        case PAGE_PLAYER:
            switch (index)
            {
            case 0: std::snprintf(buffer, capacity, "God mode                      %s", m_bGodMode ? "ON" : "OFF"); break;
            case 1: std::snprintf(buffer, capacity, "Refill P1 HP"); break;
            case 2: std::snprintf(buffer, capacity, "Add 100 P1 HP"); break;
            case 3: std::snprintf(buffer, capacity, "Remove 100 P1 HP"); break;
            case 4: std::snprintf(buffer, capacity, "Refill P1 shuriken"); break;
            case 5: std::snprintf(buffer, capacity, "Save P1 position"); break;
            case 6: std::snprintf(buffer, capacity, "Restore P1 position          %s", m_bSavedPosition ? "READY" : "EMPTY"); break;
            case 7: std::snprintf(buffer, capacity, "Move all players to P1"); break;
            };
            break;

        case PAGE_ENEMY_AI:
            if (index == 0)
                std::snprintf(buffer, capacity, "Freeze enemy AI               %s", m_bEnemyAIPaused ? "ON" : "OFF");
            else
                std::snprintf(buffer, capacity, "Kill all active enemies");
            break;

        case PAGE_STAGE:
            switch (index)
            {
            case 0: std::snprintf(buffer, capacity, "Advance 1 frame"); break;
            case 1: std::snprintf(buffer, capacity, "Advance 10 frames"); break;
            case 2: std::snprintf(buffer, capacity, "Set stage clear A"); break;
            case 3: std::snprintf(buffer, capacity, "Set stage clear B"); break;
            case 4: std::snprintf(buffer, capacity, "Set game over"); break;
            };
            break;

        case PAGE_CAMERA:
            if (index == 0)
            {
                std::snprintf(buffer, capacity, "Camera zoom                   %.2fx", CGameStageDebug::CAMERA_ZOOM_SCALE);
            }
            else if (index == 1)
            {
                static const char* s_apszCameraMode[] = { "Manual", "Automatic", "Introduction" };
                std::snprintf(
                    buffer,
                    capacity,
                    "Camera mode                   %s%s",
                    s_apszCameraMode[m_iCameraMode],
                    (m_bShowcaseCamera ? " * SHOWCASE" : "")
                );
            };
            else
                std::snprintf(buffer, capacity, "Reset to default game camera");
            break;

        case PAGE_HITBOXES:
            if (index == 0)
                std::snprintf(buffer, capacity, "Attack hitboxes               %s", CHitDebug::SHOW_HIT_ATTACK ? "ON" : "OFF");
            else if (index == 1)
                std::snprintf(buffer, capacity, "Grab/catch hitboxes           %s", CHitDebug::SHOW_HIT_CATCH ? "ON" : "OFF");
            else
                std::snprintf(buffer, capacity, "Body collision spheres       %s", CHitDebug::SHOW_HIT_BODY ? "ON" : "OFF");
            break;

        case PAGE_TELEMETRY:
            if (index == 0)
                std::snprintf(buffer, capacity, "Show telemetry when closed    %s", m_bTelemetryEnabled ? "ON" : "OFF");
            else
                std::snprintf(buffer, capacity, "Copy game screenshot to clipboard");
            break;

        default:
            break;
        };
    };


    void GetItemHelp(int32 index, const char*& scope, const char*& description) const
    {
        scope = "ACTION";
        description = "";

        switch (m_page)
        {
        case PAGE_DIFFICULTY:
            if (index == 0)
            {
                scope = "RUNTIME PRESET";
                description = "Game uses the selected difficulty; other presets override every value below.";
            }
            else if ((index >= 1) && (index <= CDebugDifficulty::SETTING_NUM))
            {
                int32 setting = index - 1;
                scope = s_apszDifficultyScope[setting];
                description = s_apszDifficultyDescription[setting];
            }
            else
            {
                scope = "RESET";
                description = "Disables every runtime override and returns to the selected game difficulty.";
            };
            break;

        case PAGE_PLAYER:
            scope = (index == 0 ? "LIVE TOGGLE" : "PLAYER ACTION");
            description = "Safe player testing tools; saved positions remain in memory until the game closes.";
            break;

        case PAGE_ENEMY_AI:
            scope = (index == 0 ? "LIVE TOGGLE" : "ENEMY ACTION");
            description = (index == 0 ?
                "Stops only AI that was running and resumes the same surviving enemies." :
                "Requests normal death for every active enemy currently owned by the stage.");
            break;

        case PAGE_STAGE:
            scope = (index <= 1 ? "FRAME CONTROL" : "STAGE ACTION");
            description = (index <= 1 ?
                "Temporarily resumes the paused gameplay sequence for an exact number of frames." :
                "Uses the normal stage result path; close the menu to let the transition continue.");
            break;

        case PAGE_CAMERA:
            scope = "LIVE CAMERA";
            description = (index == 0 ?
                "Scales automatic gameplay camera zoom without changing widescreen projection." :
                (index == 1 ?
                    "Right stick enters showcase orbit; LT/RT zoom while this menu is open." :
                    "Returns to automatic game camera, normal zoom, and the correct player path mode."));
            break;

        case PAGE_HITBOXES:
            scope = "LIVE VISUALIZATION";
            description = "Uses the recovered RenderWare debug-shape renderer; no collision data is modified.";
            break;

        case PAGE_TELEMETRY:
            scope = (index == 0 ? "DISPLAY TOGGLE" : "UTILITY");
            description = (index == 0 ?
                "Keeps the live player, enemy, stage, difficulty, and performance panel visible." :
                "Captures the game window after rendering and places the image on the clipboard.");
            break;

        default:
            break;
        };
    };


    void DrawTelemetry(int32 x, int32 y) const
    {
        m_font.Background({ 0x00, 0x00, 0x00, 0xC8 });
        m_font.Color({ 0x78, 0xD8, 0xFF, 0xFF });
        m_font.Position(x, y);
        m_font.SetAutoStep(0, 16);
        m_font.Print("LIVE TELEMETRY");
        m_font.Color({ 0xE8, 0xEE, 0xF8, 0xFF });

        GAMETYPES::DIFFICULTY difficulty = CGameData::Option().Play().GetDifficulty();
        m_font.Print("FPS %.1f  Frame %.2f ms", CScreen::Framerate(), CScreen::TimerStride() * 1000.0f);
        m_font.Print("Difficulty %s", DifficultyName(difficulty));
        m_font.Print("Debug preset %s", CDebugDifficulty::GetPresetName());
        m_font.Print("Sequence %d  Tick %d", CSequence::GetCurrently(), CGameStageDebug::STAGE_TICK);

        int32 enemyCount = 0;
        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            if (CGameProperty::GetEnemy(i))
                ++enemyCount;
        };
        m_font.Print("Enemies %d", enemyCount);

        if (CGameProperty::GetPlayerNum() > 0)
        {
            IGamePlayer* pPlayer = CGameProperty::Player(0);
            if (pPlayer && pPlayer->IsAlive())
            {
                RwV3d playerPosition = Math::VECTOR3_ZERO;
                pPlayer->GetPosition(&playerPosition);
                m_font.Print("P1 HP %d/%d", pPlayer->GetHP(), pPlayer->GetHPMax());
                m_font.Print("P1 XYZ %.2f %.2f %.2f", playerPosition.x, playerPosition.y, playerPosition.z);
                m_font.Print("P1 RotY %.3f  Status %d", pPlayer->GetRotY(), static_cast<int32>(pPlayer->GetStatus()));

                CPlayerCharacter* pCharacter = pPlayer->GetCurrentCharacter();
                if (pCharacter)
                {
                    const char* pszMotionName = pCharacter->GetMotionName();
                    m_font.Print(
                        "P1 Char %d  Motion %s",
                        static_cast<int32>(pPlayer->GetCurrentCharacterID()),
                        (pszMotionName ? pszMotionName : "<none>")
                    );
                };

                CEnemy* pNearest = nullptr;
                float nearestDistanceSq = FLT_MAX;
                for (int32 i = 0; i < enemyMax; ++i)
                {
                    CEnemy* pEnemy = CGameProperty::GetEnemy(i);
                    if (!pEnemy)
                        continue;

                    RwV3d enemyPosition = Math::VECTOR3_ZERO;
                    pEnemy->GetPosition(&enemyPosition);
                    RwV3d delta;
                    Math::Vec3_Sub(&delta, &enemyPosition, &playerPosition);
                    float distanceSq = Math::Vec3_Dot(&delta, &delta);
                    if (distanceSq < nearestDistanceSq)
                    {
                        nearestDistanceSq = distanceSq;
                        pNearest = pEnemy;
                    };
                };

                if (pNearest)
                {
                    m_font.Print(
                        "Near enemy %d  HP %d/%d",
                        static_cast<int32>(pNearest->GetID()),
                        pNearest->GetHP(),
                        pNearest->GetHPMax()
                    );
                    m_font.Print("Near distance %.2f", Math::Sqrt(nearestDistanceSq));
                };
            };
        };

        m_font.SetAutoStep(0, 0);
    };

private:
    mutable CDebugFontCtrl m_font;
    bool m_bOpen;
    bool m_bGodMode;
    bool m_bEnemyAIPaused;
    bool m_bTelemetryEnabled;
    bool m_bSavedPosition;
    bool m_bControllerMenuComboDown;
    bool m_bShowcaseCamera;
    PAGE m_page;
    int32 m_aiSelection[PAGE_NUM];
    int32 m_iPausedLabel;
    int32 m_iStepFrames;
    int32 m_iScreenshotDelay;
    CGameStage* m_pGodModeStage;
    CGameStage* m_pCameraStage;
    RwV3d m_vSavedPosition;
    float m_fSavedDirection;
    int32 m_iCameraMode;
    std::vector<uint32> m_aiResumeHandles;
};


/*static*/ CProcess* CDebugToolsProcess::Instance(void)
{
    return new CDebugToolsProcess;
};


/*static*/ void CDebugToolsProcess::Initialize(CProcess* pSender)
{
    ASSERT(pSender);
    pSender->Mail().Send(PROCLABEL_DEBUGTOOLS, PROCESSTYPES::MAIL::TYPE_ATTACH);
};


/*static*/ void CDebugToolsProcess::Terminate(CProcess* pSender)
{
    ASSERT(pSender);
    pSender->Mail().Send(PROCLABEL_DEBUGTOOLS, PROCESSTYPES::MAIL::TYPE_DETACH);
};


CDebugToolsProcess::CDebugToolsProcess(void)
: m_pImpl(nullptr)
{
    ;
};


CDebugToolsProcess::~CDebugToolsProcess(void)
{
    ;
};


bool CDebugToolsProcess::Attach(void)
{
    CDebugDifficulty::Initialize();
    m_pImpl = new CImpl;
    return (m_pImpl != nullptr);
};


void CDebugToolsProcess::Detach(void)
{
    if (m_pImpl)
    {
        delete m_pImpl;
        m_pImpl = nullptr;
    };
};


void CDebugToolsProcess::Move(void)
{
    if (m_pImpl)
        m_pImpl->Move(*this);

    if (!Info().IsProcessExist(PROCLABEL_SEQ_GAMEMAIN))
        Mail().Send(Info().Label(), PROCESSTYPES::MAIL::TYPE_DETACH);
};


void CDebugToolsProcess::Draw(void) const
{
    if (m_pImpl)
        m_pImpl->Draw();
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
