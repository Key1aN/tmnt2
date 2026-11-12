#include "DebugToolsProcess.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/ProcessList.hpp"
#include "Game/Component/CombatLab/CombatLab.hpp"
#include "Game/Component/GameData/GamePlayResult.hpp"
#include "Game/Sequence/Test/DebugUtils.hpp"
#include "Game/Component/Enemy/Enemy.hpp"
#include "Game/Component/Enemy/EnemyCharacter.hpp"
#include "Game/Component/Enemy/CharacterCompositor.hpp"
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
#include "Game/System/2d/GameFont.hpp"
#include "System/Common/Process/ProcessMail.hpp"
#include "System/Common/Process/Sequence.hpp"
#include "System/Common/Camera.hpp"
#include "System/Common/Screen.hpp"
#include "System/Common/Sprite.hpp"
#include "System/Common/System2D.hpp"
#include "System/PC/PCFrameStats.hpp"
#include "System/PC/PCGraphicsDevice.hpp"
#include "System/PC/PCCrashReporter.hpp"
#include "System/PC/PCPhysicalController.hpp"
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
        PAGE_GRAPHICS,
        PAGE_HITBOXES,
        PAGE_TELEMETRY,
        PAGE_COMBATLAB,

        PAGE_NUM,
    };


    static const char* s_apszPageName[PAGE_NUM] =
    {
        "Difficulty",
        "Player",
        "Enemy/AI",
        "Stage",
        "Camera",
        "Graphics",
        "Hitboxes",
        "Telemetry",
        "COMBAT LAB",
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
        case PROCLABEL_SEQ_COMBATLAB:
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


    static const int32 s_anMSAASamples[] = { 0, 2, 4, 8 };


    static int32 GetMSAAIndex(int32 samples)
    {
        if (samples >= 8)
            return 3;
        if (samples >= 4)
            return 2;
        if (samples >= 2)
            return 1;
        return 0;
    };


    static const char* GetMSAALabel(int32 samples)
    {
        static const char* s_apszMSAALabel[] = { "Off", "2X", "4X", "8X" };
        return s_apszMSAALabel[GetMSAAIndex(samples)];
    };


    // ---- Combat Lab tab ----

    enum LABITEM
    {
        LABITEM_ENTER_EXIT = 0,
        LABITEM_RESET,
        LABITEM_ARENA,
        LABITEM_PLAYER,
        LABITEM_CRYSTALS,
        LABITEM_RELOAD,
        LABITEM_ENEMY_TYPE,
        LABITEM_ENEMY_COUNT,
        LABITEM_SPAWN,
        LABITEM_CLEAR,
        LABITEM_INF_PLAYER_HP,
        LABITEM_INF_ENEMY_HP,
        LABITEM_AI_MODE,
        LABITEM_AI_EDIT,
        LABITEM_AI_STORE,
        LABITEM_AI_RECALL,
        LABITEM_OVERLAY,
        LABITEM_STATS_RESET,
        LABITEM_LOG,
        LABITEM_LOG_SUMMARY,
        LABITEM_HITBOXES,
        LABITEM_LABELS,
        LABITEM_RULESET,
        LABITEM_RESET_CLEARS_STATS,
        LABITEM_DEFAULTS,

        LABITEM_NUM,
    };

    static const int32 MENU_VISIBLE_ITEMS = 14;


    static bool IsCombatLabShortcut(void)
    {
        // Ctrl + Shift + L. L, Ctrl and the Shift+Ctrl chord have no retail
        // binding; left Shift alone is the default "character change" key,
        // which has no effect with the lab's single character.
        if (!CPCSpecific::IsKeyTrigger(DIK_L))
            return false;

        bool bCtrl = (CPCSpecific::IsKeyDown(DIK_LCONTROL) || CPCSpecific::IsKeyDown(DIK_RCONTROL));
        bool bShift = (CPCSpecific::IsKeyDown(DIK_LSHIFT) || CPCSpecific::IsKeyDown(DIK_RSHIFT));
        return (bCtrl && bShift);
    };


    // ---- v1.2.5 performance and move telemetry helpers ----

    static const char* s_apszEnemyStatusName[] =
    {
        "HIDE", "QUIT", "DEATH", "APPEAR", "RETREAT", "AERIAL", "TOUCHDOWN", "THINKING",
        "IDLE", "TURN", "WALK", "WALK_RIGHT", "WALK_LEFT", "WALK_BACK", "RUN", "DASH",
        "GETUP", "JUMP_READY", "JUMP", "JUMP_2ND", "ATTACK_A", "ATTACK_AA", "ATTACK_AAA",
        "ATTACK_B", "ATTACK_C", "ATTACK_RUN", "FIRE_A", "FIRE_B", "FIRE_C", "FIRE_AA",
        "FIRE_BB", "FIRE_CC", "THROW", "JUMP_ATTACK", "KNOCK", "KNOCK_FRONT", "KNOCK_BACK",
        "FLYAWAY_FRONT", "FLYAWAY_BACK", "FLYAWAY_BOUND_FRONT", "FLYAWAY_BOUND_BACK",
        "CRASHWALL_FRONT", "CRASHWALL_BACK", "CRASHWALL_FALL_FRONT", "CRASHWALL_FALL_BACK",
        "CRASHWALL_TOUCHDOWN_FRONT", "CRASHWALL_TOUCHDOWN_BACK", "DINDLE", "STUN",
        "COUNTERACT", "SLEEP", "FREEZE", "BIND", "CAUGHT", "LIFTED", "LIFTED_WALK",
        "THROWN_FRONT", "THROWN_BACK", "GUARD",
    };

    static_assert(COUNT_OF(s_apszEnemyStatusName) == ENEMYTYPES::STATUS_SPECIAL,
                  "Enemy status name table must match ENEMYTYPES::STATUS.");


    static void FormatEnemyStatus(int32 status, char* buffer, size_t capacity)
    {
        if ((status >= 0) && (status < ENEMYTYPES::STATUS_SPECIAL))
            std::snprintf(buffer, capacity, "%s", s_apszEnemyStatusName[status]);
        else if ((status >= ENEMYTYPES::STATUS_SPECIAL) && (status < ENEMYTYPES::STATUS_MAX))
            std::snprintf(buffer, capacity, "SPECIAL+%d", status - ENEMYTYPES::STATUS_SPECIAL);
        else
            std::snprintf(buffer, capacity, "STATUS_%d", status);
    };


    static bool IsBossEnemy(ENEMYID::VALUE id)
    {
        return ((id >= ENEMYID::BOSSBEGIN) && (id < ENEMYID::BOSSEND));
    };


    static void FormatEnemyName(ENEMYID::VALUE id, char* buffer, size_t capacity)
    {
        const char* pszName = ENEMYID::GetName(id);
        if (pszName && pszName[0])
            std::snprintf(buffer, capacity, "%s", pszName);
        else
            std::snprintf(buffer, capacity, "enemy%03d", static_cast<int32>(id));
    };


    static const char* SafeMotionName(const char* pszMotion)
    {
        return ((pszMotion && pszMotion[0]) ? pszMotion : "<none>");
    };


    static const char* GetEnemyMotionName(CEnemy* pEnemy)
    {
        if (!pEnemy)
            return nullptr;

        return pEnemy->Character().Compositor().GetMotionName();
    };


    static bool ProjectToNormalized(const RwV3d& world, float& normalizedX, float& normalizedY)
    {
        RwMatrix matrixView;
        RwMatrixSetIdentity(&matrixView);
        CGameProperty::GetCameraViewMatrix(&matrixView);

        RwV3d projected = Math::VECTOR3_ZERO;
        RwV3dTransformPoint(&projected, &world, &matrixView);
        if ((projected.z <= 1.0f) || (projected.z > 100.0f))
            return false;

        normalizedX = projected.x / projected.z;
        normalizedY = projected.y / projected.z;
        return ((normalizedX >= -0.05f) && (normalizedX <= 1.05f) &&
                (normalizedY >= -0.05f) && (normalizedY <= 1.05f));
    };


    struct IM2DBATCH
    {
        enum { VERTEX_MAX = 1800 };

        RwIm2DVertex m_aVertex[VERTEX_MAX];
        int32 m_nVertex;
        float m_fZ;
        float m_fRhw;
    };


    static void Im2DQuad(IM2DBATCH& batch, float x0, float y0, float x1, float y1, const RwRGBA& color)
    {
        if ((batch.m_nVertex + 6) > IM2DBATCH::VERTEX_MAX)
            return;

        const float ax[6] = { x0, x1, x0, x0, x1, x1 };
        const float ay[6] = { y0, y0, y1, y1, y0, y1 };
        for (int32 i = 0; i < 6; ++i)
        {
            RwIm2DVertex* pVertex = &batch.m_aVertex[batch.m_nVertex++];
            RwIm2DVertexSetScreenX(pVertex, ax[i]);
            RwIm2DVertexSetScreenY(pVertex, ay[i]);
            RwIm2DVertexSetScreenZ(pVertex, batch.m_fZ);
            RwIm2DVertexSetIntRGBA(pVertex, color.red, color.green, color.blue, color.alpha);
            RwIm2DVertexSetU(pVertex, 0.0f, batch.m_fRhw);
            RwIm2DVertexSetV(pVertex, 0.0f, batch.m_fRhw);
            RwIm2DVertexSetRecipCameraZ(pVertex, batch.m_fRhw);
        };
    };


    // Gameplay frames accumulated for the periodic SUMMARY line (every 10 s),
    // so steady slowdowns below the drop threshold are also recorded.
    struct PERFSUMMARY
    {
        float m_fSpanMs;
        int32 m_nTicks;
        int32 m_nShown;
        float m_fUpdateMs;
        float m_fRenderMs;
        float m_fPresentMs;
        float m_fWorstMs;
        int32 m_nOver17;
        int32 m_nOver20;
        int32 m_nOver33;
    };

    static const float PERF_SUMMARY_SPAN_MS = 10000.0f;


    struct FRAMEWINDOW
    {
        float m_fSpanMs;
        int32 m_nFrames;
        int32 m_nDrawn;
        float m_fDisplayFps;
        float m_fTickRate;
        float m_fAvgFrameMs;
        float m_fWorstDisplayMs;
        float m_fAvgUpdateMs;
        float m_fAvgRenderMs;
        float m_fAvgPresentMs;
    };


    // Summarizes roughly the last second of real frames.
    static FRAMEWINDOW ComputeFrameWindow(float spanMs)
    {
        FRAMEWINDOW window = {};
        float pendingMs = 0.0f;
        float updateSum = 0.0f;
        float renderSum = 0.0f;
        float presentSum = 0.0f;
        int32 count = CPCFrameStats::GetHistoryCount();

        for (int32 ago = 0; ago < count; ++ago)
        {
            const CPCFrameStats::SAMPLE& sample = CPCFrameStats::GetSample(ago);
            if (sample.m_fFrameMs > 1000.0f)
                break;

            window.m_fSpanMs += sample.m_fFrameMs;
            ++window.m_nFrames;
            updateSum += sample.m_fUpdateMs;
            renderSum += sample.m_fRenderMs;
            presentSum += sample.m_fPresentMs;
            if (sample.m_bDrawn)
                ++window.m_nDrawn;

            if (window.m_fSpanMs >= spanMs)
                break;
        };

        // Worst interval between frames that actually reached the screen.
        for (int32 ago = window.m_nFrames - 1; ago >= 0; --ago)
        {
            const CPCFrameStats::SAMPLE& sample = CPCFrameStats::GetSample(ago);
            pendingMs += sample.m_fFrameMs;
            if (sample.m_bDrawn)
            {
                if (pendingMs > window.m_fWorstDisplayMs)
                    window.m_fWorstDisplayMs = pendingMs;
                pendingMs = 0.0f;
            };
        };

        if ((window.m_nFrames > 0) && (window.m_fSpanMs > 0.0f))
        {
            window.m_fDisplayFps = (static_cast<float>(window.m_nDrawn) * 1000.0f) / window.m_fSpanMs;
            window.m_fTickRate = (static_cast<float>(window.m_nFrames) * 1000.0f) / window.m_fSpanMs;
            window.m_fAvgFrameMs = window.m_fSpanMs / static_cast<float>(window.m_nFrames);
            window.m_fAvgUpdateMs = updateSum / static_cast<float>(window.m_nFrames);
            window.m_fAvgRenderMs = renderSum / static_cast<float>(window.m_nFrames);
            window.m_fAvgPresentMs = presentSum / static_cast<float>(window.m_nFrames);
        };

        return window;
    };


    static float GameSpeedPercent(const FRAMEWINDOW& window)
    {
        float nominal = CScreen::Framerate();
        if (nominal <= 0.0f)
            return 0.0f;

        float speed = (window.m_fTickRate / nominal) * 100.0f;
        return (speed > 100.0f ? 100.0f : speed);
    };


    static inline float MaxF(float a, float b) { return (a > b ? a : b); };
    static inline float MinF(float a, float b) { return (a < b ? a : b); };
    static inline int32 MaxI(int32 a, int32 b) { return (a > b ? a : b); };
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
    , m_bShowcaseCamera(false)
    , m_bMSAAApplyAttempted(false)
    , m_bMSAAApplySucceeded(false)
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
    , m_bFrameGraph(true)
    , m_bDropLog(true)
    , m_bMoveLog(true)
    , m_iLabelMode(LABELMODE_BOSSES)
    , m_fLabelSize(16.0f)
    , m_fLabelLift(0.10f)
    , m_fDropThresholdMs(20.0f)
    , m_uPerfProcessed(CPCFrameStats::GetFrameIndex())
    , m_iPerfSequence(-1)
    , m_iPerfWarmup(PERF_WARMUP_FRAMES)
    , m_fPendingDisplayMs(0.0f)
    , m_iPendingSkips(0)
    , m_bDropActive(false)
    , m_iDropFrames(0)
    , m_iDropSkips(0)
    , m_iDropRecover(0)
    , m_fDropWorstMs(0.0f)
    , m_fDropTotalMs(0.0f)
    , m_uDropStartTick(0)
    , m_iSessionDropEpisodes(0)
    , m_iSessionSlowFrames(0)
    , m_iSessionSkippedFrames(0)
    , m_bPerfHeaderWritten(false)
    , m_iDisplayWidth(0)
    , m_iDisplayHeight(0)
    , m_summary()
    , m_perfLog("TMNT2_perf.log")
    , m_moveLog("TMNT2_moves.log")
    , m_aTracked()
    , m_aMoveEvent()
    , m_iMoveEventHead(0)
    , m_szPlayerMotion()
    , m_bLabEnterArmed(false)
    , m_bLabAIPresetStored(false)
    , m_labAIPreset()
    {
        std::memset(m_aiSelection, 0x00, sizeof(m_aiSelection));
        std::memset(m_aTracked, 0x00, sizeof(m_aTracked));
        std::memset(m_aMoveEvent, 0x00, sizeof(m_aMoveEvent));
        m_szPlayerMotion[0] = '\0';
        m_moveLog.Write("SESSION  TMNT2 Debug Tools v1.2.7 move telemetry (bosses always; all enemies when labels = ALL)");
        m_moveLog.Flush();

        // v1.2.7: the session header (with GPU and display mode) is written at
        // startup, not at the first drop, so every run is visible in the log.
        WritePerfSessionHeader();
        m_perfLog.Flush();
        CPCCrashReporter::Breadcrumb("DEBUGTOOLS attach perf_log=TMNT2_perf.log moves_log=TMNT2_moves.log");
    };


    ~CImpl(void)
    {
        EndDropEpisode("session end");
        m_perfLog.Flush();
        m_moveLog.Flush();
        CGameStageDebug::CAMERA_MENU_CONTROL = false;
        SetEnemyAIPaused(false);
        SetGodMode(false);
    };


    void Move(CDebugToolsProcess& owner)
    {
        UpdatePerformance();
        UpdateMoveTracking();
        UpdateStep(owner);
        UpdatePersistentTools();
        UpdateScreenshot();

        int32 currentLabel = CSequence::GetCurrently();
        bool bAvailable = IsGameplaySequence(currentLabel);
        uint32 controllerTrigger = ControllerDigitalTrigger();
        bool bControllerToggle =
            ((controllerTrigger & CController::DIGITAL_SELECT) != 0);

        if (m_bOpen && !bAvailable)
        {
            Close(owner);
            return;
        };

        if (IsCombatLabShortcut())
        {
            // One dedicated shortcut: opens the existing menu directly on the
            // COMBAT LAB tab, or closes it when that tab is already showing.
            if (m_bOpen && (m_page == PAGE_COMBATLAB))
            {
                Close(owner);
            }
            else
            {
                if (!m_bOpen && bAvailable)
                    Open(owner, currentLabel);
                if (m_bOpen)
                    m_page = PAGE_COMBATLAB;
            };
        }
        else if (CPCSpecific::IsKeyTrigger(DIK_F4) || bControllerToggle)
        {
            if (m_bOpen)
                Close(owner);
            else if (bAvailable)
                Open(owner, currentLabel);
        };

        UpdateShowcaseCamera();

        if (!m_bOpen)
            return;

        uint32 controllerNavigation = controllerTrigger | ControllerDigitalRepeat();

        if (controllerTrigger & CController::DIGITAL_START)
        {
            Close(owner);
            CGameStage* pStage = CGameStage::GetCurrent();
            if (pStage && !pStage->IsPaused())
                pStage->StartPause(CGameStage::PAUSETYPE_MENU, nullptr);
            return;
        };

        if (CPCSpecific::IsKeyTrigger(DIK_ESCAPE) ||
            (controllerTrigger & CController::DIGITAL_RLEFT))
        {
            Close(owner);
            return;
        };

        // The retail PC DirectInput table exposes the physical shoulder buttons
        // as L1 (LB) and L2 (RB). Use those raw identities here so the labels
        // match the buttons the player actually presses.
        if (CPCSpecific::IsKeyTrigger(DIK_Q) ||
            (controllerTrigger & CController::DIGITAL_L1))
            ChangePage(-1);
        else if (CPCSpecific::IsKeyTrigger(DIK_E) ||
                 (controllerTrigger & CController::DIGITAL_L2))
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

        DrawEnemyLabels();
        if (m_bFrameGraph)
            DrawFrameGraph();

        if (m_bOpen)
        {
            DrawMenu();
            DrawTelemetryCompact(12, 416);
        }
        else
        {
            DrawHint();
            if (m_bTelemetryEnabled)
                DrawTelemetry(12, 30);
        };

        // Right-hand panel; with the menu open it is shown only on the
        // COMBAT LAB tab and only when the screen is wide enough not to
        // overlap the menu text.
        if (CCombatLab::IsPlaying() && CCombatLab::IsOverlayEnabled() &&
            (!m_bOpen || ((m_page == PAGE_COMBATLAB) && (CScreen::Width() >= 1280))))
        {
            DrawCombatLabOverlay(CScreen::Width() - 344, 30);
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
        m_bLabEnterArmed = false;
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
        m_bLabEnterArmed = false;
        int32 page = InvClamp(static_cast<int32>(m_page) + direction, 0, PAGE_NUM - 1);
        m_page = static_cast<PAGE>(page);

        int32 itemCount = GetItemCount(m_page);
        if (m_aiSelection[m_page] >= itemCount)
            m_aiSelection[m_page] = (itemCount - 1);
    };


    void ChangeSelection(int32 direction)
    {
        m_bLabEnterArmed = false;
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

        case PAGE_GRAPHICS:
            {
                int32 currentIndex = GetMSAAIndex(CPCSpecific::GetMultiSamplingSamples());
                int32 nextIndex = Clamp(
                    currentIndex + direction * multiplier,
                    0,
                    COUNT_OF(s_anMSAASamples) - 1
                );
                ApplyMSAA(s_anMSAASamples[nextIndex]);
            };
            break;

        case PAGE_HITBOXES:
            ToggleHitbox(selected);
            break;

        case PAGE_TELEMETRY:
            if (selected == 0)
                m_bTelemetryEnabled = !m_bTelemetryEnabled;
            else
                AdjustTelemetryItem(selected, direction * multiplier);
            break;

        case PAGE_COMBATLAB:
            AdjustLabItem(selected, direction, multiplier);
            break;

        default:
            break;
        };
    };


    void AdjustTelemetryItem(int32 selected, int32 step)
    {
        switch (selected)
        {
        case 2: m_bFrameGraph = !m_bFrameGraph; break;
        case 3:
            if (m_bDropLog)
                EndDropEpisode("drop log disabled");
            m_bDropLog = !m_bDropLog;
            break;
        case 4:
            m_fDropThresholdMs = Clamp(m_fDropThresholdMs + (static_cast<float>(step) * 0.5f), 17.0f, 100.0f);
            break;
        case 5:
            m_iLabelMode = (m_iLabelMode + (step < 0 ? LABELMODE_NUM - 1 : 1)) % LABELMODE_NUM;
            break;
        case 6: m_bMoveLog = !m_bMoveLog; break;
        case 7:
            m_fLabelSize = Clamp(m_fLabelSize + static_cast<float>(step), 10.0f, 32.0f);
            break;
        case 8:
            m_fLabelLift = Clamp(m_fLabelLift + (static_cast<float>(step) * 0.05f), -1.0f, 1.5f);
            break;
        default: break;
        };
    };


    void ResetTelemetryItem(int32 selected)
    {
        switch (selected)
        {
        case 0: m_bTelemetryEnabled = false; break;
        case 2: m_bFrameGraph = true; break;
        case 3: m_bDropLog = true; break;
        case 4: m_fDropThresholdMs = 20.0f; break;
        case 5: m_iLabelMode = LABELMODE_BOSSES; break;
        case 6: m_bMoveLog = true; break;
        case 7: m_fLabelSize = 16.0f; break;
        case 8: m_fLabelLift = 0.10f; break;
        default: break;
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

        case PAGE_GRAPHICS:
            ApplyMSAA(CPCSpecific::GetMultiSamplingSamples());
            break;

        case PAGE_HITBOXES:
            ToggleHitbox(selected);
            break;

        case PAGE_TELEMETRY:
            if (selected == 0)
                m_bTelemetryEnabled = !m_bTelemetryEnabled;
            else if (selected == 1)
                m_iScreenshotDelay = 3;
            else if ((selected != 4) && (selected != 7) && (selected != 8))
                AdjustTelemetryItem(selected, 1);
            break;

        case PAGE_COMBATLAB:
            ActivateLabItem(owner, selected);
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

        case PAGE_GRAPHICS:
            ApplyMSAA(0);
            break;

        case PAGE_HITBOXES:
            SetHitbox(selected, false);
            break;

        case PAGE_TELEMETRY:
            ResetTelemetryItem(selected);
            break;

        case PAGE_COMBATLAB:
            ResetLabItem(selected);
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

        case PAGE_GRAPHICS:
            ApplyMSAA(0);
            break;

        case PAGE_HITBOXES:
            CHitDebug::SHOW_HIT_ATTACK = false;
            CHitDebug::SHOW_HIT_CATCH = false;
            CHitDebug::SHOW_HIT_BODY = false;
            break;

        case PAGE_TELEMETRY:
            for (int32 i = 0; i < GetItemCount(PAGE_TELEMETRY); ++i)
                ResetTelemetryItem(i);
            break;

        case PAGE_COMBATLAB:
            CCombatLab::RestoreDefaults();
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


    void ApplyMSAA(int32 samples)
    {
        m_bMSAAApplyAttempted = true;
        m_bMSAAApplySucceeded = CPCSpecific::ApplyDisplaySettings(
            CPCSpecific::GetVideomodeCur(),
            samples,
            false
        );
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

        int16 rightX = CPCPhysicalController::GetDebugRightStickHorizontal();
        int16 rightY = ControllerAnalog(CController::ANALOG_RSTICK_X);
        if (IsAnalogCameraInput(rightX) || IsAnalogCameraInput(rightY))
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
        case PAGE_GRAPHICS:   return 1;
        case PAGE_HITBOXES:   return 3;
        case PAGE_TELEMETRY:  return 9;
        case PAGE_COMBATLAB:  return LABITEM_NUM;
        default:              return 0;
        };
    };


    void DrawHint(void) const
    {
        m_font.Background({ 0x00, 0x00, 0x00, 0xB0 });
        m_font.Color({ 0xC0, 0xE8, 0xFF, 0xFF });
        m_font.Position(12, 10);
        m_font.Print("F4 / BACK  DEBUG TOOLS  |  CTRL+SHIFT+L  COMBAT LAB  |  START  PAUSE");
    };


    void DrawMenu(void) const
    {
        m_font.Background({ 0x08, 0x0C, 0x12, 0xE8 });
        m_font.Color({ 0x78, 0xD8, 0xFF, 0xFF });
        m_font.Position(18, 14);
        m_font.Print("TMNT2 DEBUG TOOLS  |  F4 or Back menu  |  Ctrl+Shift+L Combat Lab  |  Start game pause  |  Esc close");

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

        // Pages longer than the space above the compact telemetry scroll;
        // every pre-existing page fits, so their layout is unchanged.
        int32 visibleCount = (itemCount < MENU_VISIBLE_ITEMS ? itemCount : MENU_VISIBLE_ITEMS);
        int32 firstItem = Clamp(selected - (MENU_VISIBLE_ITEMS / 2), 0, MaxI(0, itemCount - visibleCount));
        for (int32 row = 0; row < visibleCount; ++row)
        {
            int32 i = firstItem + row;
            char text[160];
            FormatItemText(i, text, sizeof(text));

            if (i == selected)
                m_font.Color({ 0xFF, 0xB0, 0x30, 0xFF });
            else
                m_font.Color({ 0xE8, 0xEE, 0xF8, 0xFF });

            m_font.Position(28, 66 + (row * 18));
            m_font.Print("%c %s", (i == selected ? '>' : ' '), text);
        };

        const char* scope = "";
        const char* description = "";
        GetItemHelp(selected, scope, description);

        int32 helpY = 66 + (visibleCount * 18) + 12;
        m_font.Color({ 0x78, 0xD8, 0xFF, 0xFF });
        m_font.Position(18, helpY);
        if (itemCount > visibleCount)
            m_font.Print("%s   [%d-%d of %d]", scope, firstItem + 1, firstItem + visibleCount, itemCount);
        else
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
        m_font.Print("Camera: RS orbit  LT/RT distance  R3 reset  |  Back menu  Start game pause");
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
            }
            else
                std::snprintf(buffer, capacity, "Reset to default game camera");
            break;

        case PAGE_GRAPHICS:
            std::snprintf(
                buffer,
                capacity,
                "MSAA (fullscreen)              %s%s",
                GetMSAALabel(CPCSpecific::GetMultiSamplingSamples()),
                (m_bMSAAApplyAttempted ?
                    (m_bMSAAApplySucceeded ? "  APPLIED" : "  FAILED") : "")
            );
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
            else if (index == 1)
                std::snprintf(buffer, capacity, "Copy game screenshot to clipboard");
            else if (index == 2)
                std::snprintf(buffer, capacity, "Frame-time graph (top)        %s", m_bFrameGraph ? "ON" : "OFF");
            else if (index == 3)
                std::snprintf(buffer, capacity, "Log frame drops               %s  (%d this session)", m_bDropLog ? "ON" : "OFF", m_iSessionDropEpisodes);
            else if (index == 4)
                std::snprintf(buffer, capacity, "Frame drop threshold          %.1f ms (%.0f FPS)", m_fDropThresholdMs, 1000.0f / m_fDropThresholdMs);
            else if (index == 5)
                std::snprintf(buffer, capacity, "Move labels over enemies      %s", LabelModeName(m_iLabelMode));
            else if (index == 6)
                std::snprintf(buffer, capacity, "Log enemy/player moves        %s", m_bMoveLog ? "ON" : "OFF");
            else if (index == 7)
                std::snprintf(buffer, capacity, "Move label size               %.0f", m_fLabelSize);
            else
                std::snprintf(buffer, capacity, "Move label height above head  %+.2f", m_fLabelLift);
            break;

        case PAGE_COMBATLAB:
            FormatLabItem(index, buffer, capacity);
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

        case PAGE_GRAPHICS:
            scope = "LIVE GRAPHICS DEVICE";
            description = "Changes MSAA Off/2X/4X/8X immediately; fullscreen and driver support are required.";
            break;

        case PAGE_HITBOXES:
            scope = "LIVE VISUALIZATION";
            description = "Uses the recovered RenderWare debug-shape renderer; no collision data is modified.";
            break;

        case PAGE_TELEMETRY:
            switch (index)
            {
            case 0:
                scope = "DISPLAY TOGGLE";
                description = "Keeps the live player, enemy, stage, difficulty, and performance panel visible.";
                break;
            case 1:
                scope = "UTILITY";
                description = "Captures the game window after rendering and places the image on the clipboard.";
                break;
            case 2:
                scope = "DISPLAY TOGGLE";
                description = "Measured time between displayed frames. Green line = 60 FPS target, red line = drop threshold, purple = skipped draw.";
                break;
            case 3:
                scope = "LOG FILE";
                description = "Writes each slowdown (start, every 60 slow frames, end) with timing split and boss moves to TMNT2_perf.log.";
                break;
            case 4:
                scope = "LIVE SETTING";
                description = "A displayed frame slower than this counts as a drop. Left/Right 0.5 ms, PgUp/PgDn 2.5 ms.";
                break;
            case 5:
                scope = "DISPLAY MODE";
                description = "Shows the current animation (move), AI status, and HP above bosses or all enemies.";
                break;
            case 6:
                scope = "LOG FILE";
                description = "Writes boss (and with ALL labels, every enemy) move/status changes plus P1 moves to TMNT2_moves.log.";
                break;
            case 7:
                scope = "LIVE SETTING";
                description = "Text height of move labels (10-32, default 16). Left/Right adjust, PgUp/PgDn x5.";
                break;
            default:
                scope = "LIVE SETTING";
                description = "Distance between the top of the enemy and the label (default +0.10). Lower it if labels float too high.";
                break;
            };
            break;

        case PAGE_COMBATLAB:
            GetLabItemHelp(index, scope, description);
            break;

        default:
            break;
        };
    };


    // ---- Combat Lab tab (uses the existing menu, AI and hitbox systems) ----

    bool EnterCombatLabFromGameplay(CDebugToolsProcess& owner)
    {
        int32 currentLabel = CSequence::GetCurrently();
        if (currentLabel == PROCLABEL_SEQ_PLAYDEMO)
            return false;

        CGameStage* pStage = CGameStage::GetCurrent();
        if (!pStage)
            return false;

        // Same result the retail pause menu uses for "Quit to title"; the
        // main sequence then routes to the lab instead of the Title screen.
        if (!pStage->SetResult(CGameStage::RESULT_RET_TITLE))
            return false;

        CGameData::PlayResult().SetAreaResult(CGamePlayResult::AREARESULT_EXIT);
        CGameData::PlayResult().SetExitSub(CGamePlayResult::EXITSUB_TO_TITLE);
        CCombatLab::RequestEnterFromGameplay();
        Close(owner);
        return true;
    };


    int32 GetLabHitboxMode(void) const
    {
        bool a = CHitDebug::SHOW_HIT_ATTACK;
        bool b = CHitDebug::SHOW_HIT_BODY;
        bool c = CHitDebug::SHOW_HIT_CATCH;
        if (!a && !b && !c) return 0;
        if (a && !b && !c)  return 1;
        if (a && b && !c)   return 2;
        if (a && b && c)    return 3;
        return 4; // custom mix set on the Hitboxes tab
    };


    void SetLabHitboxMode(int32 mode)
    {
        CHitDebug::SHOW_HIT_ATTACK = (mode >= 1);
        CHitDebug::SHOW_HIT_BODY   = (mode >= 2);
        CHitDebug::SHOW_HIT_CATCH  = (mode >= 3);
    };


    void AdjustLabItem(int32 selected, int32 direction, int32 multiplier)
    {
        switch (selected)
        {
        case LABITEM_ARENA:
            CCombatLab::SelectArena(CCombatLab::GetSelectedArena() + direction);
            break;

        case LABITEM_PLAYER:
            CCombatLab::SelectPlayer(static_cast<PLAYERID::VALUE>(
                static_cast<int32>(CCombatLab::GetSelectedPlayer()) + direction));
            break;

        case LABITEM_CRYSTALS:
            CCombatLab::SetCrystalLevel(InvClamp(CCombatLab::GetCrystalLevel() + direction, -1, 3));
            break;

        case LABITEM_ENEMY_TYPE:
            CCombatLab::SelectEnemyType(CCombatLab::GetEnemyTypeIndex() + direction);
            break;

        case LABITEM_ENEMY_COUNT:
            CCombatLab::SetEnemyCount(CCombatLab::GetEnemyCount() + direction * multiplier);
            break;

        case LABITEM_INF_PLAYER_HP:
            CCombatLab::SetInfinitePlayerHP(!CCombatLab::IsInfinitePlayerHP());
            break;

        case LABITEM_INF_ENEMY_HP:
            CCombatLab::SetInfiniteEnemyHP(!CCombatLab::IsInfiniteEnemyHP());
            break;

        case LABITEM_AI_MODE:
            CCombatLab::SetAIModeValue(static_cast<CCombatLab::AIMODE>(
                InvClamp(static_cast<int32>(CCombatLab::GetAIMode()) + direction, 0,
                         static_cast<int32>(CCombatLab::AIMODE_NUM) - 1)));
            break;

        case LABITEM_OVERLAY:
            CCombatLab::SetOverlayEnabled(!CCombatLab::IsOverlayEnabled());
            break;

        case LABITEM_LOG:
            CCombatLab::SetLogEnabled(!CCombatLab::IsLogEnabled());
            break;

        case LABITEM_HITBOXES:
            {
                int32 mode = GetLabHitboxMode();
                if (mode > 3)
                    mode = 0;
                SetLabHitboxMode(InvClamp(mode + direction, 0, 3));
            }
            break;

        case LABITEM_LABELS:
            m_iLabelMode = (m_iLabelMode + (direction < 0 ? LABELMODE_NUM - 1 : 1)) % LABELMODE_NUM;
            break;

        case LABITEM_RULESET:
            CCombatLab::SetRuleset(CCombatLab::GetRuleset() == CCombatLab::RULESET_VANILLA ?
                                   CCombatLab::RULESET_EXPERIMENTAL :
                                   CCombatLab::RULESET_VANILLA);
            break;

        case LABITEM_RESET_CLEARS_STATS:
            CCombatLab::SetResetClearsStats(!CCombatLab::IsResetClearsStats());
            break;

        default:
            break;
        };
    };


    void ActivateLabItem(CDebugToolsProcess& owner, int32 selected)
    {
        bool bPlaying = CCombatLab::IsPlaying();

        if (selected != LABITEM_ENTER_EXIT)
            m_bLabEnterArmed = false;

        switch (selected)
        {
        case LABITEM_ENTER_EXIT:
            if (CCombatLab::IsActive())
            {
                CCombatLab::RequestExit();
                Close(owner);
            }
            else if (!m_bLabEnterArmed)
            {
                m_bLabEnterArmed = true;   // second press confirms leaving the stage
            }
            else
            {
                m_bLabEnterArmed = false;
                EnterCombatLabFromGameplay(owner);
            };
            break;

        case LABITEM_RESET:
            if (bPlaying)
            {
                CCombatLab::ResetScenario();
                ResetShowcaseCamera();
            };
            break;

        case LABITEM_RELOAD:
            if (CCombatLab::IsActive())
            {
                CCombatLab::RequestRestart();
                Close(owner);
            };
            break;

        case LABITEM_SPAWN:
            if (bPlaying)
                CCombatLab::SpawnEnemies();
            break;

        case LABITEM_CLEAR:
            if (bPlaying)
                CCombatLab::ClearEnemies();
            break;

        case LABITEM_AI_EDIT:
            // Reuse the existing Difficulty tab (Thinking rate, activity, ...)
            // instead of duplicating its sliders here.
            CCombatLab::SetAIModeValue(CCombatLab::AIMODE_CUSTOM);
            m_page = PAGE_DIFFICULTY;
            m_aiSelection[PAGE_DIFFICULTY] = 1 + CDebugDifficulty::SETTING_AI_THINKING;
            break;

        case LABITEM_AI_STORE:
            CDebugDifficulty::Snapshot(m_labAIPreset);
            m_bLabAIPresetStored = true;
            break;

        case LABITEM_AI_RECALL:
            if (m_bLabAIPresetStored)
            {
                CCombatLab::SetAIModeValue(CCombatLab::AIMODE_CUSTOM);
                CDebugDifficulty::Restore(m_labAIPreset);
            };
            break;

        case LABITEM_STATS_RESET:
            CCombatLab::ResetStats();
            break;

        case LABITEM_LOG_SUMMARY:
            CCombatLab::WriteSummaryToLog("manual");
            break;

        case LABITEM_DEFAULTS:
            CCombatLab::RestoreDefaults();
            break;

        default:
            AdjustLabItem(selected, 1, 1);
            break;
        };
    };


    void ResetLabItem(int32 selected)
    {
        switch (selected)
        {
        case LABITEM_ENEMY_COUNT:   CCombatLab::SetEnemyCount(3); break;
        case LABITEM_ENEMY_TYPE:    CCombatLab::SelectEnemyType(0); break;
        case LABITEM_ARENA:         CCombatLab::SelectArena(0); break;
        case LABITEM_PLAYER:        CCombatLab::SelectPlayer(PLAYERID::ID_LEO); break;
        case LABITEM_CRYSTALS:      CCombatLab::SetCrystalLevel(3); break;
        case LABITEM_INF_PLAYER_HP: CCombatLab::SetInfinitePlayerHP(true); break;
        case LABITEM_INF_ENEMY_HP:  CCombatLab::SetInfiniteEnemyHP(true); break;
        case LABITEM_AI_MODE:       CCombatLab::SetAIModeValue(CCombatLab::AIMODE_ORIGINAL); break;
        case LABITEM_OVERLAY:       CCombatLab::SetOverlayEnabled(true); break;
        case LABITEM_LOG:           CCombatLab::SetLogEnabled(true); break;
        case LABITEM_HITBOXES:      SetLabHitboxMode(0); break;
        case LABITEM_LABELS:        m_iLabelMode = LABELMODE_BOSSES; break;
        case LABITEM_RULESET:       CCombatLab::SetRuleset(CCombatLab::RULESET_VANILLA); break;
        case LABITEM_RESET_CLEARS_STATS: CCombatLab::SetResetClearsStats(true); break;
        case LABITEM_STATS_RESET:   CCombatLab::ResetStats(); break;
        default: break;
        };
    };


    void FormatLabItem(int32 index, char* buffer, size_t capacity) const
    {
        bool bActive = CCombatLab::IsActive();
        const char* pszLabOnly = (CCombatLab::IsPlaying() ? "" : "  (in lab)");

        switch (index)
        {
        case LABITEM_ENTER_EXIT:
            if (bActive)
                std::snprintf(buffer, capacity, "Exit Combat Lab (to Title)");
            else
                std::snprintf(buffer, capacity, "%s", m_bLabEnterArmed ?
                              "Enter Combat Lab  - press again to quit this stage" :
                              "Enter Combat Lab");
            break;

        case LABITEM_RESET:
            std::snprintf(buffer, capacity, "Reset scenario%s", pszLabOnly);
            break;

        case LABITEM_ARENA:
            std::snprintf(buffer, capacity, "Arena                         %s%s",
                          CCombatLab::GetArena(CCombatLab::GetSelectedArena()).m_pszName,
                          (CCombatLab::GetSelectedArena() != CCombatLab::GetLoadedArena() && bActive) ? " *RELOAD" : "");
            break;

        case LABITEM_PLAYER:
            std::snprintf(buffer, capacity, "Player character              %s%s",
                          PLAYERID::GetName(CCombatLab::GetSelectedPlayer()),
                          (CCombatLab::GetSelectedPlayer() != CCombatLab::GetLoadedPlayer() && bActive) ? " *RELOAD" : "");
            break;

        case LABITEM_CRYSTALS:
            if (CCombatLab::GetCrystalLevel() < 0)
                std::snprintf(buffer, capacity, "Crystal upgrades (move set)   Save file");
            else
                std::snprintf(buffer, capacity, "Crystal upgrades (move set)   Level %d%s",
                              CCombatLab::GetCrystalLevel(),
                              (CCombatLab::GetCrystalLevel() == 3 ? " (all combos)" : ""));
            break;

        case LABITEM_RELOAD:
            std::snprintf(buffer, capacity, "Reload lab with arena/player%s",
                          bActive ? (CCombatLab::IsReloadPending() ? "  (changes pending)" : "") : "  (in lab)");
            break;

        case LABITEM_ENEMY_TYPE:
            std::snprintf(buffer, capacity, "Enemy type                    %s  [%d/%d]",
                          ENEMYID::GetName(CCombatLab::GetEnemyType()),
                          CCombatLab::GetEnemyTypeIndex() + 1,
                          CCombatLab::GetEnemyTypeNum());
            break;

        case LABITEM_ENEMY_COUNT:
            std::snprintf(buffer, capacity, "Enemy count                   %d", CCombatLab::GetEnemyCount());
            break;

        case LABITEM_SPAWN:
            std::snprintf(buffer, capacity, "Spawn enemies%s   alive %d/%d", pszLabOnly,
                          CCombatLab::IsPlaying() ? CCombatLab::GetSpawnedAliveNum() : 0,
                          static_cast<int32>(CCombatLab::SPAWN_MAX));
            break;

        case LABITEM_CLEAR:
            std::snprintf(buffer, capacity, "Clear spawned enemies%s", pszLabOnly);
            break;

        case LABITEM_INF_PLAYER_HP:
            std::snprintf(buffer, capacity, "Infinite player HP            %s", CCombatLab::IsInfinitePlayerHP() ? "ON" : "OFF");
            break;

        case LABITEM_INF_ENEMY_HP:
            std::snprintf(buffer, capacity, "Infinite enemy HP             %s", CCombatLab::IsInfiniteEnemyHP() ? "ON" : "OFF");
            break;

        case LABITEM_AI_MODE:
            std::snprintf(buffer, capacity, "Enemy AI                      %s", CCombatLab::GetAIModeName(CCombatLab::GetAIMode()));
            break;

        case LABITEM_AI_EDIT:
            std::snprintf(buffer, capacity, "Edit AI values (Thinking rate...) -> Difficulty tab");
            break;

        case LABITEM_AI_STORE:
            std::snprintf(buffer, capacity, "Store current AI values as lab preset%s", m_bLabAIPresetStored ? "  (stored)" : "");
            break;

        case LABITEM_AI_RECALL:
            std::snprintf(buffer, capacity, "Recall lab AI preset          %s", m_bLabAIPresetStored ? "READY" : "EMPTY");
            break;

        case LABITEM_OVERLAY:
            std::snprintf(buffer, capacity, "Combat stats overlay          %s", CCombatLab::IsOverlayEnabled() ? "ON" : "OFF");
            break;

        case LABITEM_STATS_RESET:
            std::snprintf(buffer, capacity, "Reset statistics");
            break;

        case LABITEM_LOG:
            std::snprintf(buffer, capacity, "Combat log (TMNT2_combatlab.log) %s", CCombatLab::IsLogEnabled() ? "ON" : "OFF");
            break;

        case LABITEM_LOG_SUMMARY:
            std::snprintf(buffer, capacity, "Write stats summary to log");
            break;

        case LABITEM_HITBOXES:
            {
                static const char* s_apszMode[] = { "OFF", "Attack", "Attack+Body", "All", "Custom (Hitboxes tab)" };
                std::snprintf(buffer, capacity, "Hitbox view                   %s", s_apszMode[GetLabHitboxMode()]);
            }
            break;

        case LABITEM_LABELS:
            std::snprintf(buffer, capacity, "Enemy state labels            %s", LabelModeName(m_iLabelMode));
            break;

        case LABITEM_RULESET:
            std::snprintf(buffer, capacity, "Combat ruleset                %s", CCombatLab::GetRulesetName(CCombatLab::GetRuleset()));
            break;

        case LABITEM_RESET_CLEARS_STATS:
            std::snprintf(buffer, capacity, "Reset also clears statistics  %s", CCombatLab::IsResetClearsStats() ? "ON" : "OFF");
            break;

        case LABITEM_DEFAULTS:
            std::snprintf(buffer, capacity, "Restore default Combat Lab settings");
            break;

        default:
            break;
        };
    };


    void GetLabItemHelp(int32 index, const char*& scope, const char*& description) const
    {
        switch (index)
        {
        case LABITEM_ENTER_EXIT:
            scope = (CCombatLab::IsActive() ? "LAB EXIT" : "LAB ENTRY");
            description = (CCombatLab::IsActive() ?
                "Leaves the lab; record, game mode, option difficulty and AI overrides are restored." :
                "Quits the current stage like Pause > Quit to title, then loads the lab. Unsaved stage progress is lost.");
            break;
        case LABITEM_RESET:
            scope = "SCENARIO";
            description = "Player to the arena start, HP/shuriken full, enemies respawned at fixed spots, seed fixed, camera reset.";
            break;
        case LABITEM_ARENA:
            scope = "APPLIES ON RELOAD";
            description = "Retail Battle Nexus arena. Each choice loads a stage whose LPAC packs that enemy roster.";
            break;
        case LABITEM_PLAYER:
            scope = "APPLIES ON RELOAD";
            description = "Character loaded into the lab (default Leonardo).";
            break;
        case LABITEM_CRYSTALS:
            scope = "LIVE (SHURIKEN MAX ON RELOAD)";
            description = "Red lv gates AAC/AABB/power x2, green lv AABC. Forced levels never reach the save; Save file = your record.";
            break;
        case LABITEM_RELOAD:
            scope = "LAB RELOAD";
            description = "Reloads the lab sequence so a new arena or player takes effect; other lab settings are kept.";
            break;
        case LABITEM_ENEMY_TYPE:
            scope = "NEXT SPAWN";
            description = "Only enemies packed in the loaded arena are offered; spawning anything else is unsafe.";
            break;
        case LABITEM_ENEMY_COUNT:
            scope = "NEXT SPAWN / RESET";
            description = "Enemies created per spawn and per reset (1-6). Placed in a fixed arc in front of the start.";
            break;
        case LABITEM_SPAWN:
        case LABITEM_CLEAR:
            scope = "SCENARIO ACTION";
            description = "Adds or removes lab enemies with the same CEnemy calls the arena generators and Kill all use.";
            break;
        case LABITEM_INF_PLAYER_HP:
            scope = "LIVE";
            description = "ON: existing damage negation (God mode path). OFF: HP drains, a lethal hit refills instead of game over.";
            break;
        case LABITEM_INF_ENEMY_HP:
            scope = "LIVE";
            description = "Full retail damage is still sent; HP is refilled only before a hit that would be lethal.";
            break;
        case LABITEM_AI_MODE:
            scope = "LIVE - LAB ENEMIES";
            description = "Original = retail Normal. Passive = StopAI, reactions intact. Custom = Difficulty tab. Aggressive = preset.";
            break;
        case LABITEM_AI_EDIT:
            scope = "NAVIGATION";
            description = "Switches AI to Custom and opens the existing Difficulty tab at AI thinking rate.";
            break;
        case LABITEM_AI_STORE:
        case LABITEM_AI_RECALL:
            scope = "AI PRESET";
            description = "Stores/recalls every Difficulty-tab value for this session. Lab exit restores your pre-lab values.";
            break;
        case LABITEM_OVERLAY:
        case LABITEM_STATS_RESET:
            scope = "DIAGNOSTICS";
            description = "Hits, guards, theoretical damage, hit chain (gaps <= 1.0 s), KOs prevented, last hit, enemy AI status.";
            break;
        case LABITEM_LOG:
        case LABITEM_LOG_SUMMARY:
            scope = "LOG FILE";
            description = "Writes every hit, reset, spawn and summary line to TMNT2_combatlab.log in the game folder.";
            break;
        case LABITEM_HITBOXES:
            scope = "LIVE VISUALIZATION";
            description = "Shortcut to the existing Hitboxes tab flags; nothing about collision is modified.";
            break;
        case LABITEM_LABELS:
            scope = "LIVE VISUALIZATION";
            description = "Shortcut to the existing Telemetry move labels (motion, AI status, HP). Choose ALL for normal enemies.";
            break;
        case LABITEM_RULESET:
            scope = "KEPT ACROSS RESETS";
            description = "Vanilla = retail code. Experimental = enables future switchable changes; none are implemented yet.";
            break;
        case LABITEM_RESET_CLEARS_STATS:
            scope = "SCENARIO";
            description = "When ON, Reset scenario also zeroes statistics.";
            break;
        default:
            scope = "RESET";
            description = "Arena, player, enemies, HP toggles, AI mode, overlay. The combat ruleset is kept.";
            break;
        };
    };


    void DrawCombatLabOverlay(int32 x, int32 y) const
    {
        const CCombatLab::STATS& s = CCombatLab::Stats();
        int32 lineH = 16;

        m_font.Background({ 0x06, 0x0A, 0x10, 0xC8 });
        m_font.Color({ 0x78, 0xFF, 0xA8, 0xFF });
        m_font.Position(x, y);
        m_font.Print("COMBAT LAB  %s  |  AI %s",
                     CCombatLab::GetRulesetName(CCombatLab::GetRuleset()),
                     CCombatLab::GetAIModeName(CCombatLab::GetAIMode()));
        y += lineH;

        m_font.Color({ 0xE8, 0xEE, 0xF8, 0xFF });
        m_font.Position(x, y);
        m_font.Print("Hits %d  guarded %d  ineffective %d", s.m_nPlayerHits, s.m_nPlayerHitsGuarded, s.m_nPlayerHitsIneffective);
        y += lineH;
        m_font.Position(x, y);
        m_font.Print("Damage dealt %.0f  received %.0f (%d hits)", s.m_fDamageDealt, s.m_fDamageReceived, s.m_nEnemyHitsOnPlayer);
        y += lineH;
        m_font.Position(x, y);
        m_font.Print("Hit chain %d  max %d   KO prevented P%d E%d", s.m_nChain, s.m_nChainMax, s.m_nPlayerKOPrevented, s.m_nEnemyKOPrevented);
        y += lineH;
        m_font.Position(x, y);
        m_font.Print("Last: %s -> %s %s %d", s.m_szLastAttackMotion[0] ? s.m_szLastAttackMotion : "-",
                     s.m_szLastTarget[0] ? s.m_szLastTarget : "-",
                     s.m_szLastResult[0] ? s.m_szLastResult : "-",
                     s.m_iLastDamage);
        y += lineH;

        IGamePlayer* pPlayer = (CGameProperty::GetPlayerNum() > 0 ? CGameProperty::Player(0) : nullptr);
        CPlayerCharacter* pChr = (pPlayer && pPlayer->IsAlive() ? pPlayer->GetCurrentCharacter() : nullptr);
        if (pChr)
        {
            m_font.Color({ 0xFF, 0xD0, 0x58, 0xFF });
            m_font.Position(x, y);
            m_font.Print("P1 %s  st %d (%.2fs)  HP %d/%d",
                         SafeMotionName(pChr->GetMotionName()),
                         static_cast<int32>(pChr->GetStatus()),
                         pChr->GetStatusDuration(),
                         pPlayer->GetHP(), pPlayer->GetHPMax());
            y += lineH;
        };

        m_font.Color({ 0xC0, 0xE8, 0xFF, 0xFF });
        int32 enemyMax = CGameProperty::GetEnemyMax();
        int32 shown = 0;
        for (int32 i = 0; (i < enemyMax) && (shown < CCombatLab::SPAWN_MAX); ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!pEnemy)
                continue;

            char name[48];
            char status[32];
            FormatEnemyName(pEnemy->GetID(), name, sizeof(name));
            FormatEnemyStatus(static_cast<int32>(pEnemy->Character().GetStatus()), status, sizeof(status));

            m_font.Position(x, y);
            m_font.Print("%-18.18s %-14.14s %s HP %d/%d", name, status,
                         pEnemy->Character().IsRunningAI() ? "AI" : "--",
                         pEnemy->GetHP(), pEnemy->GetHPMax());
            y += lineH;
            ++shown;
        };
    };


    // ---- v1.2.5: real frame timing, drop log, move telemetry, labels ----

    enum LABELMODE
    {
        LABELMODE_OFF = 0,
        LABELMODE_BOSSES,
        LABELMODE_ALL,
        LABELMODE_NUM,
    };


    struct TRACKEDENEMY
    {
        uint32 m_uHandle;
        int32  m_iStatus;
        char   m_szMotion[48];
    };


    struct MOVEEVENT
    {
        char  m_szText[112];
        bool  m_bBoss;
    };


    enum
    {
        MOVEEVENT_NUM = 8,
        DROP_RECOVER_FRAMES = 30,
        DROP_CONTEXT_EVERY = 60,
        PERF_WARMUP_FRAMES = 45,
    };


    static const char* LabelModeName(int32 mode)
    {
        switch (mode)
        {
        case LABELMODE_OFF:    return "OFF";
        case LABELMODE_BOSSES: return "BOSSES";
        case LABELMODE_ALL:    return "ALL ENEMIES";
        default:               return "?";
        };
    };


    bool IsTrackedForMoves(ENEMYID::VALUE id) const
    {
        return (IsBossEnemy(id) || (m_iLabelMode == LABELMODE_ALL));
    };


    void UpdatePerformance(void)
    {
        int32 currentLabel = CSequence::GetCurrently();
        bool bGameplay = IsGameplaySequence(currentLabel);
        if (currentLabel != m_iPerfSequence)
        {
            m_iPerfSequence = currentLabel;
            m_iPerfWarmup = PERF_WARMUP_FRAMES;
            m_fPendingDisplayMs = 0.0f;
            m_iPendingSkips = 0;
        };

        uint32 frameIndex = CPCFrameStats::GetFrameIndex();
        uint32 available = frameIndex - m_uPerfProcessed;
        if (available > static_cast<uint32>(CPCFrameStats::HISTORY))
            available = static_cast<uint32>(CPCFrameStats::HISTORY);

        for (int32 ago = static_cast<int32>(available) - 1; ago >= 0; --ago)
            ProcessFrameSample(CPCFrameStats::GetSample(ago), bGameplay);

        m_uPerfProcessed = frameIndex;
        CheckDisplayChange();
        m_perfLog.FlushIfDue();
        m_moveLog.FlushIfDue();
    };


    void ProcessFrameSample(const CPCFrameStats::SAMPLE& sample, bool bGameplay)
    {
        // Focus loss, loading, and sequence changes produce long gaps that are not gameplay drops.
        if (!bGameplay || (sample.m_fFrameMs > 1000.0f))
        {
            if (m_bDropActive)
                EndDropEpisode("interrupted");
            std::memset(&m_summary, 0x00, sizeof(m_summary));
            m_iPerfWarmup = PERF_WARMUP_FRAMES;
            m_fPendingDisplayMs = 0.0f;
            m_iPendingSkips = 0;
            return;
        };

        m_fPendingDisplayMs += sample.m_fFrameMs;
        if (!sample.m_bDrawn)
        {
            ++m_iPendingSkips;
            ++m_iSessionSkippedFrames;
            return;
        };

        float displayMs = m_fPendingDisplayMs;
        int32 skips = m_iPendingSkips;
        m_fPendingDisplayMs = 0.0f;
        m_iPendingSkips = 0;

        if (m_iPerfWarmup > 0)
        {
            --m_iPerfWarmup;
            return;
        };

        AccumulateSummary(displayMs, sample, skips + 1);

        bool bSlow = (displayMs > m_fDropThresholdMs);
        if (bSlow)
        {
            ++m_iSessionSlowFrames;
            m_iDropRecover = 0;
            if (!m_bDropActive)
            {
                m_bDropActive = true;
                m_iDropFrames = 0;
                m_iDropSkips = 0;
                m_fDropWorstMs = 0.0f;
                m_fDropTotalMs = 0.0f;
                m_uDropStartTick = static_cast<uint32>(CGameStageDebug::STAGE_TICK);
                ++m_iSessionDropEpisodes;
                if (m_bDropLog)
                    WriteDropLine("DROP START", displayMs, skips, sample);
            }
            else if (m_bDropLog && ((m_iDropFrames % DROP_CONTEXT_EVERY) == 0))
            {
                WriteDropLine("DROP CONT ", displayMs, skips, sample);
            };

            ++m_iDropFrames;
            m_iDropSkips += skips;
            m_fDropTotalMs += displayMs;
            if (displayMs > m_fDropWorstMs)
                m_fDropWorstMs = displayMs;
        }
        else if (m_bDropActive)
        {
            m_iDropSkips += skips;
            if (++m_iDropRecover >= DROP_RECOVER_FRAMES)
                EndDropEpisode("recovered");
        };
    };


    void EndDropEpisode(const char* pszReason)
    {
        if (m_bDropActive && m_bDropLog)
        {
            m_perfLog.Write(
                "DROP END    %s  slow frames %d  worst %.1f ms  avg %.1f ms  skipped draws %d  ticks %u  threshold %.1f ms",
                pszReason,
                m_iDropFrames,
                m_fDropWorstMs,
                (m_iDropFrames ? (m_fDropTotalMs / static_cast<float>(m_iDropFrames)) : 0.0f),
                m_iDropSkips,
                static_cast<uint32>(CGameStageDebug::STAGE_TICK) - m_uDropStartTick,
                m_fDropThresholdMs
            );
            m_perfLog.Flush();
        };

        m_bDropActive = false;
        m_iDropRecover = 0;
    };


    void WritePerfSessionHeader(void)
    {
        if (m_bPerfHeaderWritten)
            return;

        m_bPerfHeaderWritten = true;
        m_perfLog.Write(
            "SESSION  TMNT2 Debug Tools v1.2.7 perf telemetry  %dx%d  nominal %.1f FPS  MSAA %s  threshold %.1f ms  summary every %.0f s",
            CScreen::Width(),
            CScreen::Height(),
            CScreen::Framerate(),
            GetMSAALabel(CPCSpecific::GetMultiSamplingSamples()),
            m_fDropThresholdMs,
            PERF_SUMMARY_SPAN_MS / 1000.0f
        );
        WriteDisplayLine("DISPLAY  start");
    };


    void WriteDisplayLine(const char* pszKind)
    {
        char szDisplay[384] = {};
        CPCGraphicsDevice::DescribeDisplay(szDisplay, sizeof(szDisplay));
        m_perfLog.Write("%s  %s", pszKind, szDisplay);
        m_iDisplayWidth = CScreen::Width();
        m_iDisplayHeight = CScreen::Height();
    };


    void CheckDisplayChange(void)
    {
        if ((CScreen::Width() == m_iDisplayWidth) && (CScreen::Height() == m_iDisplayHeight))
            return;

        WriteDisplayLine("DISPLAY  changed");
        m_perfLog.Flush();
    };


    void AccumulateSummary(float displayMs, const CPCFrameStats::SAMPLE& sample, int32 ticks)
    {
        m_summary.m_fSpanMs += displayMs;
        m_summary.m_nTicks += ticks;
        ++m_summary.m_nShown;
        m_summary.m_fUpdateMs += sample.m_fUpdateMs;
        m_summary.m_fRenderMs += sample.m_fRenderMs;
        m_summary.m_fPresentMs += sample.m_fPresentMs;
        if (displayMs > m_summary.m_fWorstMs)
            m_summary.m_fWorstMs = displayMs;
        if (displayMs > 17.5f) ++m_summary.m_nOver17;
        if (displayMs > 20.0f) ++m_summary.m_nOver20;
        if (displayMs > 33.4f) ++m_summary.m_nOver33;

        if (m_summary.m_fSpanMs < PERF_SUMMARY_SPAN_MS)
            return;

        float shown = static_cast<float>(m_summary.m_nShown);
        float fps = (shown * 1000.0f) / m_summary.m_fSpanMs;
        float speed = (static_cast<float>(m_summary.m_nTicks) * 1000.0f / m_summary.m_fSpanMs) / CScreen::Framerate() * 100.0f;
        m_perfLog.Write(
            "SUMMARY  %.1f s  shown %.1f FPS  speed %.0f%%  avg %.2f ms  worst %.1f ms  >17.5ms %d  >20ms %d  >33ms %d  |  avg update %.2f  render %.2f  present %.2f  |  seq %d stage %d  %dx%d",
            m_summary.m_fSpanMs / 1000.0f,
            fps,
            speed,
            m_summary.m_fSpanMs / shown,
            m_summary.m_fWorstMs,
            m_summary.m_nOver17,
            m_summary.m_nOver20,
            m_summary.m_nOver33,
            m_summary.m_fUpdateMs / shown,
            m_summary.m_fRenderMs / shown,
            m_summary.m_fPresentMs / shown,
            CSequence::GetCurrently(),
            static_cast<int32>(CGameData::PlayParam().GetStage()),
            CScreen::Width(),
            CScreen::Height());
        m_perfLog.Flush();
        std::memset(&m_summary, 0x00, sizeof(m_summary));
    };


    void WriteDropLine(const char* pszKind, float displayMs, int32 skips, const CPCFrameStats::SAMPLE& sample)
    {
        WritePerfSessionHeader();

        FRAMEWINDOW window = ComputeFrameWindow(1000.0f);
        int32 enemyCount = 0;
        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            if (CGameProperty::GetEnemy(i))
                ++enemyCount;
        };

        int32 charId = -1;
        const char* pszPlayerMotion = nullptr;
        if (CGameProperty::GetPlayerNum() > 0)
        {
            IGamePlayer* pPlayer = CGameProperty::Player(0);
            if (pPlayer && pPlayer->IsAlive())
            {
                charId = static_cast<int32>(pPlayer->GetCurrentCharacterID());
                CPlayerCharacter* pCharacter = pPlayer->GetCurrentCharacter();
                if (pCharacter)
                    pszPlayerMotion = pCharacter->GetMotionName();
            };
        };

        m_perfLog.Write(
            "%s %.1f ms (%.0f FPS)  update %.1f  render %.1f  present %.1f  skipped-before %d  |  1s: shown %.1f FPS  speed %.0f%%  worst %.1f ms  |  seq %d stage %d tick %d  enemies %d  P1 char %d motion %s",
            pszKind,
            displayMs,
            (displayMs > 0.0f ? 1000.0f / displayMs : 0.0f),
            sample.m_fUpdateMs,
            sample.m_fRenderMs,
            sample.m_fPresentMs,
            skips,
            window.m_fDisplayFps,
            GameSpeedPercent(window),
            window.m_fWorstDisplayMs,
            CSequence::GetCurrently(),
            static_cast<int32>(CGameData::PlayParam().GetStage()),
            CGameStageDebug::STAGE_TICK,
            enemyCount,
            charId,
            SafeMotionName(pszPlayerMotion)
        );

        for (int32 i = 0; i < enemyMax; ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!pEnemy || !IsBossEnemy(pEnemy->GetID()))
                continue;

            char name[48];
            char status[32];
            FormatEnemyName(pEnemy->GetID(), name, sizeof(name));
            FormatEnemyStatus(static_cast<int32>(pEnemy->Character().GetStatus()), status, sizeof(status));
            m_perfLog.Write(
                "           boss %s (id %d)  motion %s  status %s  HP %d/%d",
                name,
                static_cast<int32>(pEnemy->GetID()),
                SafeMotionName(GetEnemyMotionName(pEnemy)),
                status,
                pEnemy->GetHP(),
                pEnemy->GetHPMax()
            );
        };

        m_perfLog.Flush();
    };


    void PushMoveEvent(bool bBoss, const char* pszText)
    {
        MOVEEVENT& event = m_aMoveEvent[m_iMoveEventHead % MOVEEVENT_NUM];
        std::snprintf(event.m_szText, sizeof(event.m_szText), "%s", pszText);
        event.m_bBoss = bBoss;
        ++m_iMoveEventHead;
    };


    void UpdateMoveTracking(void)
    {
        if (!IsGameplaySequence(CSequence::GetCurrently()))
        {
            for (int32 i = 0; i < GAMETYPES::ENEMY_MAX; ++i)
                m_aTracked[i].m_uHandle = 0;
            m_szPlayerMotion[0] = '\0';
            return;
        };

        int32 enemyMax = CGameProperty::GetEnemyMax();
        if (enemyMax > GAMETYPES::ENEMY_MAX)
            enemyMax = GAMETYPES::ENEMY_MAX;

        for (int32 i = 0; i < enemyMax; ++i)
        {
            TRACKEDENEMY& tracked = m_aTracked[i];
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!pEnemy)
            {
                tracked.m_uHandle = 0;
                continue;
            };

            ENEMYID::VALUE id = pEnemy->GetID();
            int32 status = static_cast<int32>(pEnemy->Character().GetStatus());
            const char* pszMotion = SafeMotionName(GetEnemyMotionName(pEnemy));
            uint32 handle = pEnemy->GetHandle();
            bool bNew = (tracked.m_uHandle != handle);
            bool bMotionChanged = (bNew || (std::strcmp(tracked.m_szMotion, pszMotion) != 0));
            bool bStatusChanged = (bNew || (tracked.m_iStatus != status));

            if ((bMotionChanged || bStatusChanged) && IsTrackedForMoves(id))
            {
                char name[48];
                char statusText[32];
                FormatEnemyName(id, name, sizeof(name));
                FormatEnemyStatus(status, statusText, sizeof(statusText));

                char text[112];
                std::snprintf(text, sizeof(text), "%s%s: %s [%s]",
                              (IsBossEnemy(id) ? "BOSS " : ""), name, pszMotion, statusText);
                PushMoveEvent(IsBossEnemy(id), text);

                if (m_bMoveLog)
                {
                    m_moveLog.Write(
                        "tick %-7d %-5s %-24s slot %2d  %s %-28s status %-22s HP %d/%d",
                        CGameStageDebug::STAGE_TICK,
                        (IsBossEnemy(id) ? "BOSS" : "enemy"),
                        name,
                        i,
                        (bNew ? "SPAWN " : "motion"),
                        pszMotion,
                        statusText,
                        pEnemy->GetHP(),
                        pEnemy->GetHPMax()
                    );
                };
            };

            tracked.m_uHandle = handle;
            tracked.m_iStatus = status;
            std::snprintf(tracked.m_szMotion, sizeof(tracked.m_szMotion), "%s", pszMotion);
        };

        if (m_bMoveLog && (CGameProperty::GetPlayerNum() > 0))
        {
            IGamePlayer* pPlayer = CGameProperty::Player(0);
            CPlayerCharacter* pCharacter = ((pPlayer && pPlayer->IsAlive()) ? pPlayer->GetCurrentCharacter() : nullptr);
            if (pCharacter)
            {
                const char* pszMotion = SafeMotionName(pCharacter->GetMotionName());
                if (std::strcmp(m_szPlayerMotion, pszMotion) != 0)
                {
                    m_moveLog.Write(
                        "tick %-7d P1    char %-19d          motion %-28s HP %d/%d",
                        CGameStageDebug::STAGE_TICK,
                        static_cast<int32>(pPlayer->GetCurrentCharacterID()),
                        pszMotion,
                        pPlayer->GetHP(),
                        pPlayer->GetHPMax()
                    );
                    std::snprintf(m_szPlayerMotion, sizeof(m_szPlayerMotion), "%s", pszMotion);
                };
            };
        };

        m_moveLog.FlushIfDue();
    };


    void DrawEnemyLabels(void) const
    {
        if ((m_iLabelMode == LABELMODE_OFF) || !CGameFont::IsOpen())
            return;

        // Uses the game's scalable font in the same virtual-screen space as the
        // engine's own world labels (640x480 virtual units, centered origin).
        const float mainHeight = m_fLabelSize;
        const float infoHeight = m_fLabelSize * 0.68f;
        const float shadow = MaxF(0.6f, m_fLabelSize * 0.06f);

        bool bStatePushed = false;
        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!pEnemy)
                continue;

            ENEMYID::VALUE id = pEnemy->GetID();
            bool bBoss = IsBossEnemy(id);
            if (!bBoss && (m_iLabelMode != LABELMODE_ALL))
                continue;

            const CCharacterCompositor& compositor = pEnemy->Character().Compositor();
            RwV3d head = Math::VECTOR3_ZERO;
            pEnemy->GetPosition(&head);
            head.y += compositor.GetCollisionParameter().m_fHeight + m_fLabelLift;

            float nx = 0.0f;
            float ny = 0.0f;
            if (!ProjectToNormalized(head, nx, ny))
                continue;

            char name[48];
            char status[32];
            FormatEnemyName(id, name, sizeof(name));
            FormatEnemyStatus(static_cast<int32>(pEnemy->Character().GetStatus()), status, sizeof(status));

            char line1[96];
            char line2[96];
            std::snprintf(line1, sizeof(line1), "%s", SafeMotionName(compositor.GetMotionName()));
            std::snprintf(line2, sizeof(line2), "%s  %s  HP %d/%d", name, status, pEnemy->GetHP(), pEnemy->GetHPMax());

            float vx = CSprite::m_fVirtualScreenX + (nx * CSprite::m_fVirtualScreenW);
            float vy = CSprite::m_fVirtualScreenY + (ny * CSprite::m_fVirtualScreenH);

            // The info line sits directly above the head point; the move name above it.
            float y2 = vy - (infoHeight * 1.1f);
            float y1 = y2 - (mainHeight * 1.02f);
            float x1 = vx - (CGameFont::GetStringWidth(line1, mainHeight) * 0.5f);
            float x2 = vx - (CGameFont::GetStringWidth(line2, infoHeight) * 0.5f);

            if (!bStatePushed)
            {
                CSystem2D::PushRenderState();
                bStatePushed = true;
            };

            CGameFont::SetHeight(mainHeight);
            CGameFont::SetRGBA(0x00, 0x00, 0x00, 0xE0);
            CGameFont::Show(line1, x1 + shadow, y1 + shadow);
            if (bBoss)
                CGameFont::SetRGBA(0xFF, 0xD0, 0x40, 0xFF);
            else
                CGameFont::SetRGBA(0xF0, 0xF4, 0xFF, 0xFF);
            CGameFont::Show(line1, x1, y1);

            CGameFont::SetHeight(infoHeight);
            CGameFont::SetRGBA(0x00, 0x00, 0x00, 0xD0);
            CGameFont::Show(line2, x2 + shadow, y2 + shadow);
            CGameFont::SetRGBA(0xC8, 0xD8, 0xE8, 0xFF);
            CGameFont::Show(line2, x2, y2);
        };

        if (bStatePushed)
            CSystem2D::PopRenderState();
    };


    void DrawFrameGraph(void) const
    {
        RwCamera* pCamera = CCamera::CameraCurrent();
        if (!pCamera)
            return;

        // Build display-frame intervals: time between frames that reached the screen.
        enum { BARS = 120 };
        float aBar[BARS] = {};
        bool aSkipped[BARS] = {};
        int32 nBar = 0;
        {
            float pendingMs = 0.0f;
            bool bPendingSkip = false;
            int32 count = CPCFrameStats::GetHistoryCount();
            float aAll[CPCFrameStats::HISTORY] = {};
            bool aAllSkipped[CPCFrameStats::HISTORY] = {};
            int32 nAll = 0;
            for (int32 ago = count - 1; ago >= 0; --ago)
            {
                const CPCFrameStats::SAMPLE& sample = CPCFrameStats::GetSample(ago);
                if (sample.m_fFrameMs > 1000.0f)
                {
                    nAll = 0;
                    pendingMs = 0.0f;
                    bPendingSkip = false;
                    continue;
                };

                pendingMs += sample.m_fFrameMs;
                if (!sample.m_bDrawn)
                {
                    bPendingSkip = true;
                    continue;
                };

                aAll[nAll] = pendingMs;
                aAllSkipped[nAll] = bPendingSkip;
                ++nAll;
                pendingMs = 0.0f;
                bPendingSkip = false;
            };

            int32 first = (nAll > BARS ? nAll - BARS : 0);
            for (int32 i = first; i < nAll; ++i)
            {
                aBar[nBar] = aAll[i];
                aSkipped[nBar] = aAllSkipped[i];
                ++nBar;
            };
        };

        float scale = MaxF(1.0f, static_cast<float>(CScreen::Height()) / 480.0f);
        float barW = 2.0f * scale;
        float graphW = barW * static_cast<float>(BARS);
        float graphH = 56.0f * scale;
        float x0 = (static_cast<float>(CScreen::Width()) - graphW) * 0.5f;
        if (m_bOpen && (CScreen::Width() < 1400))
            x0 = static_cast<float>(CScreen::Width()) - graphW - (12.0f * scale);
        float y0 = 6.0f * scale;
        const float MAX_MS = 50.0f;

        static IM2DBATCH s_batch;
        s_batch.m_nVertex = 0;
        s_batch.m_fZ = RwIm2DGetNearScreenZ();
        s_batch.m_fRhw = 1.0f / RwCameraGetNearClipPlane(pCamera);

        Im2DQuad(s_batch, x0 - 4.0f, y0 - 3.0f, x0 + graphW + 4.0f, y0 + graphH + 3.0f, { 0x00, 0x00, 0x00, 0xA8 });

        for (int32 i = 0; i < nBar; ++i)
        {
            float ms = MinF(aBar[i], MAX_MS);
            float h = MaxF(1.0f, (ms / MAX_MS) * graphH);
            RwRGBA color = { 0x40, 0xE0, 0x70, 0xE0 };
            if (aBar[i] > m_fDropThresholdMs)
                color = { 0xFF, 0x48, 0x48, 0xF0 };
            else if (aBar[i] > 17.6f)
                color = { 0xFF, 0xC8, 0x40, 0xE8 };

            float bx = x0 + (static_cast<float>(i) * barW);
            Im2DQuad(s_batch, bx, y0 + graphH - h, bx + barW - 0.5f, y0 + graphH, color);
            if (aSkipped[i])
                Im2DQuad(s_batch, bx, y0, bx + barW - 0.5f, y0 + (3.0f * scale), { 0xD0, 0x60, 0xFF, 0xFF });
        };

        float target = y0 + graphH - ((1000.0f / MaxF(1.0f, CScreen::Framerate())) / MAX_MS) * graphH;
        float limit = y0 + graphH - (MinF(m_fDropThresholdMs, MAX_MS) / MAX_MS) * graphH;
        Im2DQuad(s_batch, x0, target, x0 + graphW, target + 1.0f, { 0x60, 0xFF, 0x90, 0x90 });
        Im2DQuad(s_batch, x0, limit, x0 + graphW, limit + 1.0f, { 0xFF, 0x50, 0x50, 0xA0 });

        CSystem2D::PushRenderState();
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, s_batch.m_aVertex, s_batch.m_nVertex);
        CSystem2D::PopRenderState();

        FRAMEWINDOW window = ComputeFrameWindow(1000.0f);
        float latest = (nBar > 0 ? aBar[nBar - 1] : 0.0f);
        m_font.Background({ 0x00, 0x00, 0x00, 0xA8 });
        m_font.Color(window.m_fWorstDisplayMs > m_fDropThresholdMs ? RwRGBA{ 0xFF, 0x90, 0x80, 0xFF } : RwRGBA{ 0xC8, 0xFF, 0xD8, 0xFF });
        m_font.Position(static_cast<int32>(x0), static_cast<int32>(y0 + graphH + (5.0f * scale)));
        m_font.Print(
            "FPS %.1f  %.1fms  worst %.1fms  speed %.0f%%  drops %d",
            window.m_fDisplayFps,
            latest,
            window.m_fWorstDisplayMs,
            GameSpeedPercent(window),
            m_iSessionDropEpisodes
        );
    };


    void DrawTelemetryCompact(int32 x, int32 y) const
    {
        m_font.Background({ 0x00, 0x00, 0x00, 0xD8 });
        m_font.Color({ 0xD8, 0xF0, 0xFF, 0xFF });
        m_font.Position(x, y);
        m_font.SetAutoStep(0, 16);

        GAMETYPES::DIFFICULTY difficulty = CGameData::Option().Play().GetDifficulty();
        FRAMEWINDOW window = ComputeFrameWindow(1000.0f);
        m_font.Print(
            "LIVE  FPS %.1f %.2fms  Speed %.0f%%  Diff %s  Preset %s  MSAA %s",
            window.m_fDisplayFps,
            window.m_fAvgFrameMs,
            GameSpeedPercent(window),
            DifficultyName(difficulty),
            CDebugDifficulty::GetPresetName(),
            GetMSAALabel(CPCSpecific::GetMultiSamplingSamples())
        );

        int32 enemyCount = 0;
        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            if (CGameProperty::GetEnemy(i))
                ++enemyCount;
        };

        IGamePlayer* pPlayer = nullptr;
        if (CGameProperty::GetPlayerNum() > 0)
            pPlayer = CGameProperty::Player(0);

        if (!pPlayer || !pPlayer->IsAlive())
        {
            m_font.Print(
                "Seq %d  Tick %d  Enemies %d  P1 unavailable",
                CSequence::GetCurrently(),
                CGameStageDebug::STAGE_TICK,
                enemyCount
            );
            m_font.SetAutoStep(0, 0);
            return;
        };

        m_font.Print(
            "Seq %d  Tick %d  Enemies %d  P1 HP %d/%d",
            CSequence::GetCurrently(),
            CGameStageDebug::STAGE_TICK,
            enemyCount,
            pPlayer->GetHP(),
            pPlayer->GetHPMax()
        );

        RwV3d playerPosition = Math::VECTOR3_ZERO;
        pPlayer->GetPosition(&playerPosition);
        m_font.Print(
            "P1 XYZ %.1f %.1f %.1f  Rot %.2f  Status %d  Char %d",
            playerPosition.x,
            playerPosition.y,
            playerPosition.z,
            pPlayer->GetRotY(),
            static_cast<int32>(pPlayer->GetStatus()),
            static_cast<int32>(pPlayer->GetCurrentCharacterID())
        );

        const char* pszMotionName = "<none>";
        CPlayerCharacter* pCharacter = pPlayer->GetCurrentCharacter();
        if (pCharacter && pCharacter->GetMotionName())
            pszMotionName = pCharacter->GetMotionName();

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
                "Motion %.20s  Near %d HP %d/%d Dist %.1f",
                pszMotionName,
                static_cast<int32>(pNearest->GetID()),
                pNearest->GetHP(),
                pNearest->GetHPMax(),
                Math::Sqrt(nearestDistanceSq)
            );
        }
        else
        {
            m_font.Print("Motion %.20s  Near none", pszMotionName);
        };

        m_font.SetAutoStep(0, 0);
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
        FRAMEWINDOW window = ComputeFrameWindow(1000.0f);
        m_font.Print(
            "FPS %.1f shown  %.1f ticks/s  speed %.0f%%  worst %.1f ms",
            window.m_fDisplayFps,
            window.m_fTickRate,
            GameSpeedPercent(window),
            window.m_fWorstDisplayMs
        );
        m_font.Print(
            "Frame %.2f ms = update %.2f + render %.2f + present %.2f",
            window.m_fAvgFrameMs,
            window.m_fAvgUpdateMs,
            window.m_fAvgRenderMs,
            window.m_fAvgPresentMs
        );
        m_font.Print(
            "Drops %d (>%.1f ms)  slow frames %d  skipped draws %d",
            m_iSessionDropEpisodes,
            m_fDropThresholdMs,
            m_iSessionSlowFrames,
            m_iSessionSkippedFrames
        );
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

        int32 eventCount = (m_iMoveEventHead < MOVEEVENT_NUM ? m_iMoveEventHead : MOVEEVENT_NUM);
        if (eventCount > 0)
        {
            m_font.Color({ 0x78, 0xD8, 0xFF, 0xFF });
            m_font.Print("Recent moves (newest first)");
            for (int32 i = 0; i < eventCount; ++i)
            {
                const MOVEEVENT& event = m_aMoveEvent[(m_iMoveEventHead - 1 - i) % MOVEEVENT_NUM];
                m_font.Color(event.m_bBoss ? RwRGBA{ 0xFF, 0xD0, 0x40, 0xFF } : RwRGBA{ 0xE8, 0xEE, 0xF8, 0xFF });
                m_font.Print("%s", event.m_szText);
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
    bool m_bShowcaseCamera;
    bool m_bMSAAApplyAttempted;
    bool m_bMSAAApplySucceeded;
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
    bool m_bFrameGraph;
    bool m_bDropLog;
    bool m_bMoveLog;
    int32 m_iLabelMode;
    float m_fLabelSize;
    float m_fLabelLift;
    float m_fDropThresholdMs;
    uint32 m_uPerfProcessed;
    int32 m_iPerfSequence;
    int32 m_iPerfWarmup;
    float m_fPendingDisplayMs;
    int32 m_iPendingSkips;
    bool m_bDropActive;
    int32 m_iDropFrames;
    int32 m_iDropSkips;
    int32 m_iDropRecover;
    float m_fDropWorstMs;
    float m_fDropTotalMs;
    uint32 m_uDropStartTick;
    int32 m_iSessionDropEpisodes;
    int32 m_iSessionSlowFrames;
    int32 m_iSessionSkippedFrames;
    bool m_bPerfHeaderWritten;
    int32 m_iDisplayWidth;
    int32 m_iDisplayHeight;
    PERFSUMMARY m_summary;
    CPCDebugLogFile m_perfLog;
    CPCDebugLogFile m_moveLog;
    TRACKEDENEMY m_aTracked[GAMETYPES::ENEMY_MAX];
    MOVEEVENT m_aMoveEvent[MOVEEVENT_NUM];
    int32 m_iMoveEventHead;
    char m_szPlayerMotion[48];
    bool m_bLabEnterArmed;
    bool m_bLabAIPresetStored;
    CDebugDifficulty::SNAPSHOT m_labAIPreset;
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
