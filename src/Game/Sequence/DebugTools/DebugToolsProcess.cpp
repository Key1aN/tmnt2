#include "DebugToolsProcess.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/ProcessList.hpp"
#include "Game/Component/CombatLab/CombatLab.hpp"
#include "Game/Component/CombatOverhaul/CombatOverhaul.hpp"
#include "Game/Component/Enemy/AITrace.hpp"
#include "Game/Component/Enemy/EnemyUtils.hpp"

#if defined(TMNT2_IMGUI)
#include "imgui.h"
#include "System/PC/PCDebugImGui.hpp"
#endif /* defined(TMNT2_IMGUI) */
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
        PAGE_AIVIEW,
        PAGE_OVERHAUL,

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
        "AI VIEW",
        "OVERHAUL",
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
        "Multiplies the final damage of every hit an enemy takes. A hit never does less than 1 damage.",
        "Multiplies maximum HP when an enemy spawns. Enemies already alive keep their HP.",
        "Multiplies the final damage of every hit a player takes, after guard and defence reductions. A hit never does less than 1.",
        "Chance, each time an enemy decides, to use A, AA, AAA, B, C or a run attack. The percent is what the live enemies have now. Stops at 99%.",
        "Chance to guard your A-chain, B, shuriken, jump attack, run attack and throw. A chance that is 0 in the game data stays 0. Stops at 99%.",
        "Shooters only (Foot Archer, Mobster...): aim convergence, mid-range and repeat-fire chances. Does nothing to melee Foot Ninjas. Stops at 99%.",
        "Shooters only: the distance they open fire from, in world units. Does nothing to melee Foot Ninjas. Stops at 255.",
        "How often an enemy re-decides; 100% = no wait. Foot Ninjas and other grunts read it only when they spawn (Reset scenario), and a higher rate also makes their idle waits longer.",
        "Chance, each time an enemy thinks, that it commits to a target instead of idling. Stops at 100%.",
        "How far away enemies notice and chase you: scales their action radius and their front/rear view (also widens the 36 deg attack and 90 deg guard cones). Behind them, a rear view of 0 stays 0. See AI VIEW > awareness zones.",
        "Bosses and monster-type enemies only: time between their attacks; lower = more attacks. Does nothing to Foot Ninjas and other grunts.",
        "Bosses and elites only: damage they absorb before getting temporary super-armour; lower = they stop flinching sooner. Does nothing to Foot Ninjas.",
    };


    static const char* s_apszDifficultyScope[CDebugDifficulty::SETTING_NUM] =
    {
        "LIVE - ALL ENEMIES - MULTIPLIER",
        "NEXT SPAWN - ALL ENEMIES - MULTIPLIER",
        "LIVE - ALL PLAYERS - MULTIPLIER",
        "LIVE - GRUNTS - PERCENT CHANCE",
        "LIVE - GRUNTS - PERCENT CHANCE",
        "LIVE - SHOOTERS ONLY - PERCENT CHANCE",
        "LIVE - SHOOTERS ONLY - DISTANCE",
        "GRUNTS: NEXT SPAWN / BOSSES: LIVE - PERCENT",
        "LIVE - ALL ENEMIES - PERCENT CHANCE",
        "LIVE - ALL ENEMIES - DETECTION REACH (UNITS)",
        "BOSSES AND MONSTERS ONLY - MULTIPLIER",
        "BOSSES AND ELITES ONLY - MULTIPLIER",
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


    enum AIITEM
    {
        AIITEM_ENABLE = 0,
        AIITEM_SIX_NINJAS,
        AIITEM_ZONES,
        AIITEM_ZONE_LABELS,
        AIITEM_RADAR_RANGE,
        AIITEM_TIMELINE,
        AIITEM_TIMELINE_SPAN,
        AIITEM_FREEZE,
        AIITEM_SELECT,
        AIITEM_DECISIONS,
        AIITEM_RECORD,
        AIITEM_SUMMARY,
        AIITEM_CLEAR,

        AIITEM_NUM,
    };


    enum AIZONES
    {
        AIZONES_OFF = 0,
        AIZONES_ALL,
        AIZONES_FOCUS,

        AIZONES_NUM,
    };


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


    // ---- v1.2.10 menu renderer (game font + flat panels) ----

    static const char* MENU_VERSION = "v1.2.21";

    enum MENUSIZE
    {
        MENUSIZE_CLASSIC = 0,   // previous fixed-size text renderer
        MENUSIZE_SMALL,
        MENUSIZE_NORMAL,
        MENUSIZE_LARGE,
        MENUSIZE_XLARGE,

        MENUSIZE_NUM,
    };

    static const char* s_apszMenuSizeName[MENUSIZE_NUM] = { "Classic", "Small", "Normal", "Large", "Extra large" };
    static const float s_afMenuSizeScale[MENUSIZE_NUM] = { 1.00f, 0.88f, 1.00f, 1.14f, 1.28f };


    enum MENUSTYLE
    {
        MENUSTYLE_IMGUI = 0,    // Dear ImGui window (mouse + keyboard + pad)
        MENUSTYLE_GAMEFONT,     // in-game font renderer (v1.2.10 - v1.2.13)

        MENUSTYLE_NUM,
    };

    static const char* s_apszMenuStyleName[MENUSTYLE_NUM] = { "ImGui", "Game font" };


    struct MENUTEXT
    {
        float  m_fX;
        float  m_fY;        // text baseline, virtual screen units
        float  m_fHeight;
        RwRGBA m_color;
        char   m_szText[200];
    };

    enum { MENUTEXT_MAX = 128 };


    // The game font has no glyphs for a few ASCII characters.
    static void MenuSanitize(char* pszText)
    {
        for (char* p = pszText; *p; ++p)
        {
            switch (*p)
            {
            case '[': case '{': case '<': *p = '('; break;
            case ']': case '}':           *p = ')'; break;
            case '|': case '\\':          *p = '/'; break;
            case '~': case '^':           *p = '-'; break;
            case '`':                     *p = '\''; break;
            case '$': case '#': case '@': *p = ' '; break;
            case ';':                     *p = ','; break;
            default: break;
            };
        };
    };


    static float MenuTextWidth(const char* pszText, float height)
    {
        if (!pszText[0])
            return 0.0f;
        return CGameFont::GetStringWidth(pszText, height) * 1.06f;
    };


    // Greedy word wrap by measured width. Returns the number of lines written.
    static int32 MenuWrap(const char* pszText, float height, float maxWidth, char aLine[][200], int32 maxLines)
    {
        int32 nLine = 0;
        char current[200];
        current[0] = '\0';

        const char* p = pszText;
        while (*p && (nLine < maxLines))
        {
            while (*p == ' ')
                ++p;
            if (!*p)
                break;

            char word[96];
            int32 n = 0;
            while (*p && (*p != ' ') && (n < static_cast<int32>(sizeof(word)) - 1))
                word[n++] = *p++;
            word[n] = '\0';

            char candidate[200];
            if (current[0])
                std::snprintf(candidate, sizeof(candidate), "%s %s", current, word);
            else
                std::snprintf(candidate, sizeof(candidate), "%s", word);

            if (current[0] && (MenuTextWidth(candidate, height) > maxWidth))
            {
                std::snprintf(aLine[nLine++], 200, "%s", current);
                std::snprintf(current, sizeof(current), "%s", word);
            }
            else
            {
                std::snprintf(current, sizeof(current), "%s", candidate);
            };
        };

        if (current[0] && (nLine < maxLines))
            std::snprintf(aLine[nLine++], 200, "%s", current);

        return nLine;
    };


    static const char* GetSectionTitle(int32 page, int32 index)
    {
        if (page == PAGE_DIFFICULTY)
        {
            switch (index)
            {
            case 1 + CDebugDifficulty::SETTING_PLAYER_DAMAGE_DEALT:   return "DAMAGE AND HEALTH";
            case 1 + CDebugDifficulty::SETTING_ATTACK_FREQUENCY:      return "MELEE DECISIONS";
            case 1 + CDebugDifficulty::SETTING_PROJECTILE_FREQUENCY:  return "SHOOTERS ONLY";
            case 1 + CDebugDifficulty::SETTING_AI_THINKING:           return "AI BEHAVIOUR";
            case 1 + CDebugDifficulty::SETTING_ATTACK_INTERVAL:       return "BOSSES AND MONSTERS ONLY";
            default: return nullptr;
            };
        };

        if (page == PAGE_COMBATLAB)
        {
            switch (index)
            {
            case LABITEM_ENTER_EXIT:    return "SCENARIO";
            case LABITEM_ENEMY_TYPE:    return "ENEMIES";
            case LABITEM_INF_PLAYER_HP: return "SURVIVABILITY";
            case LABITEM_AI_MODE:       return "ENEMY AI";
            case LABITEM_OVERLAY:       return "DIAGNOSTICS";
            case LABITEM_RULESET:       return "COMBAT RULES";
            default: return nullptr;
            };
        };

        if (page == PAGE_AIVIEW)
        {
            switch (index)
            {
            case AIITEM_ENABLE:      return "ENEMY AI VISUALIZER";
            case AIITEM_ZONES:       return "IN-WORLD AWARENESS ZONES";
            case AIITEM_RADAR_RANGE: return "DISPLAY";
            case AIITEM_SELECT:      return "DECISION HISTORY";
            case AIITEM_RECORD:      return "RECORDING";
            default: return nullptr;
            };
        };

        if (page == PAGE_OVERHAUL)
            return CCombatOverhaul::GetItemHeader(index);

        return nullptr;
    };


    static uint32 ControllerDigitalHeld(void)
    {
        return CController::GetDigital(CController::CONTROLLER_LOCKED_ON_VIRTUAL) |
               CController::GetDigital(CController::CONTROLLER_UNLOCKED_ON_VIRTUAL);
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
        if (!CGameProperty::IsCameraViewAvailable())
            return false;

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
    , m_uNavHeld(0)
    , m_iNavFrames(0)
    , m_iMenuSize(MENUSIZE_NORMAL)
    , m_iMenuStyle(MENUSTYLE_IMGUI)
    , m_nUIAction(0)
    , m_iUILastSelection(-1)
    , m_iUILastPage(-1)
    , m_aiScroll()
    , m_iAISelected(-1)
    , m_fAIRadarRange(8.0f)
    , m_fAITimelineSpan(12.0f)
    , m_bAITimeline(true)
    , m_bAIDecisionPanel(true)
    , m_iAIZones(AIZONES_OFF)
    , m_bAIZoneLabels(true)
    , m_iLastMenuBottom(0)
    , m_iLastMenuRight(0)
    , m_bImGuiMenuShown(false)
    , m_bLabEnterArmed(false)
    , m_bLabAIPresetStored(false)
    , m_labAIPreset()
    {
        std::memset(m_aiSelection, 0x00, sizeof(m_aiSelection));
        std::memset(m_aiScroll, 0x00, sizeof(m_aiScroll));
        std::memset(m_aTracked, 0x00, sizeof(m_aTracked));
        std::memset(m_aMoveEvent, 0x00, sizeof(m_aMoveEvent));
        m_szPlayerMotion[0] = '\0';
        m_moveLog.Write("SESSION  TMNT2 Debug Tools v1.2.19 move telemetry (bosses always; all enemies when labels = ALL)");
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
        CEnemyAITrace::SetEnabled(false);
    };


    void Move(CDebugToolsProcess& owner)
    {
        CEnemyAITrace::Update();
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

        if (bAvailable)
        {
            // Combat Overhaul: F6 switches BEFORE (retail) / AFTER (overhaul)
            // at any time; T toggles player 1's lock-on during play (the
            // controller uses R3, read by the player manipulator).
            if (CPCSpecific::IsKeyTrigger(DIK_F6))
                CCombatOverhaul::SetEnabled(!CCombatOverhaul::IsEnabled());

            if (!m_bOpen && CPCSpecific::IsKeyTrigger(DIK_T))
                CCombatOverhaul::RequestLockOnToggle(0);
        };

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
        {
            m_nUIAction = 0;
            return;
        };

        if (controllerTrigger & CController::DIGITAL_START)
        {
            Close(owner);
            CGameStage* pStage = CGameStage::GetCurrent();
            if (pStage && !pStage->IsPaused())
                pStage->StartPause(CGameStage::PAUSETYPE_MENU, nullptr);
            return;
        };

        if (CPCSpecific::IsKeyTrigger(DIK_ESCAPE) ||
            (controllerTrigger & CController::DIGITAL_RRIGHT))
        {
            Close(owner);
            return;
        };

        // Mouse actions from the ImGui menu (queued during Draw).
        if (!ApplyUIActions(owner))
            return;

        // Direction input with its own hold-repeat for keyboard, D-pad and
        // stick alike: first press acts at once, holding repeats, and a long
        // hold on a numeric item moves it in bigger jumps.
        enum { NAV_UP = 1, NAV_DOWN = 2, NAV_LEFT = 4, NAV_RIGHT = 8 };
        uint32 controllerHeld = ControllerDigitalHeld();
        uint32 navHeld = 0;
        if (CPCSpecific::IsKeyDown(DIK_UP)    || (controllerHeld & CController::DIGITAL_LUP))    navHeld |= NAV_UP;
        if (CPCSpecific::IsKeyDown(DIK_DOWN)  || (controllerHeld & CController::DIGITAL_LDOWN))  navHeld |= NAV_DOWN;
        if (CPCSpecific::IsKeyDown(DIK_LEFT)  || (controllerHeld & CController::DIGITAL_LLEFT))  navHeld |= NAV_LEFT;
        if (CPCSpecific::IsKeyDown(DIK_RIGHT) || (controllerHeld & CController::DIGITAL_LRIGHT)) navHeld |= NAV_RIGHT;

        bool bNavFire = false;
        bool bNavRepeat = false;
        if (navHeld != m_uNavHeld)
        {
            m_uNavHeld = navHeld;
            m_iNavFrames = 0;
            bNavFire = (navHeld != 0);
        }
        else if (navHeld != 0)
        {
            ++m_iNavFrames;
            int32 period = (m_iNavFrames > 80 ? 2 : 5);
            bNavRepeat = ((m_iNavFrames >= 20) && (((m_iNavFrames - 20) % period) == 0));
            bNavFire = bNavRepeat;
        };

        int32 navAccel = 1;
        if (m_iNavFrames > 200)
            navAccel = 8;
        else if (m_iNavFrames > 130)
            navAccel = 4;
        else if (m_iNavFrames > 70)
            navAccel = 2;

        // Since v1.2.17 the DirectInput table maps buttons by physical
        // position: LB = L1, RB = R1, A = RDOWN, B = RRIGHT, X = RLEFT, Y = RUP.
        if (CPCSpecific::IsKeyTrigger(DIK_Q) ||
            (controllerTrigger & CController::DIGITAL_L1))
            ChangePage(-1);
        else if (CPCSpecific::IsKeyTrigger(DIK_E) ||
                 (controllerTrigger & CController::DIGITAL_R1))
            ChangePage(1);
        else if (bNavFire && (navHeld & NAV_UP))
            ChangeSelection(-1);
        else if (bNavFire && (navHeld & NAV_DOWN))
            ChangeSelection(1);
        else if (bNavFire && (navHeld & (NAV_LEFT | NAV_RIGHT)))
        {
            // Toggles and actions only react to a fresh press, never to a hold.
            if (!bNavRepeat || IsNumericItem(m_page, m_aiSelection[m_page]))
                AdjustSelected(((navHeld & NAV_LEFT) ? -1 : 1), (bNavRepeat ? navAccel : 1));
        }
        else if (CPCSpecific::IsKeyTrigger(DIK_PRIOR))
            AdjustSelected(1, 5);
        else if (CPCSpecific::IsKeyTrigger(DIK_NEXT))
            AdjustSelected(-1, 5);
        else if (CPCSpecific::IsKeyTrigger(DIK_RETURN) ||
                 (controllerTrigger & CController::DIGITAL_RDOWN))
            ActivateSelected(owner);
        else if (CPCSpecific::IsKeyTrigger(DIK_BACK) ||
                 (controllerTrigger & CController::DIGITAL_RLEFT))
            ResetSelected();
        else if (CPCSpecific::IsKeyTrigger(DIK_DELETE) ||
                 (controllerTrigger & CController::DIGITAL_RUP))
            ResetPage();
    };


    static bool IsNumericItem(PAGE page, int32 selected)
    {
        switch (page)
        {
        case PAGE_DIFFICULTY: return ((selected >= 1) && (selected <= CDebugDifficulty::SETTING_NUM));
        case PAGE_CAMERA:     return (selected == 0);
        case PAGE_TELEMETRY:  return ((selected == 4) || (selected == 7) || (selected == 8));
        case PAGE_COMBATLAB:  return (selected == LABITEM_ENEMY_COUNT);
        case PAGE_AIVIEW:     return ((selected == AIITEM_RADAR_RANGE) || (selected == AIITEM_TIMELINE_SPAN));
        case PAGE_OVERHAUL:   return CCombatOverhaul::IsNumericItem(selected);
        default:              return false;
        };
    };


    void Draw(void) const
    {
        int32 currentLabel = CSequence::GetCurrently();
        if (!IsGameplaySequence(currentLabel))
        {
#if defined(TMNT2_IMGUI)
            CPCDebugImGui::MarkHidden();
#endif
            return;
        };

        DrawAwarenessZones();
        DrawEnemyLabels();
        DrawOverhaulHud();
        if (m_bFrameGraph)
            DrawFrameGraph();

        bool bImGuiMenu = (m_bOpen && IsImGuiMenuStyle());
        m_bImGuiMenuShown = bImGuiMenu;

        // Right-hand panel; with the menu open it is shown only on the
        // COMBAT LAB tab and only when the screen is wide enough not to
        // overlap the menu text.
        DrawAIView();

        // The AI radar takes the right-hand column while it is enabled.
        if (CCombatLab::IsPlaying() && CCombatLab::IsOverlayEnabled() && !CEnemyAITrace::IsEnabled() &&
            (!m_bOpen || ((m_page == PAGE_COMBATLAB) && (CScreen::Width() >= 1280))))
        {
            // With the menu open the frame graph moves to the right edge.
            float graphScale = MaxF(1.0f, static_cast<float>(CScreen::Height()) / 480.0f);
            int32 overlayY = ((m_bOpen && m_bFrameGraph) ? static_cast<int32>(70.0f * graphScale) + 26 : 30);
            DrawCombatLabOverlay(CScreen::Width() - 344, overlayY);
        };

        // The menu is drawn last so it stays on top of the overlays.
        if (m_bOpen)
        {
            int32 menuBottom = 0;
            if (bImGuiMenu && DrawMenuImGui())
            {
                menuBottom = m_iLastMenuBottom;
            }
            else
            {
                m_bImGuiMenuShown = false;
#if defined(TMNT2_IMGUI)
                CPCDebugImGui::MarkHidden();
#endif
                menuBottom = DrawMenu();
            };
            m_iLastMenuBottom = menuBottom;

            // Compact telemetry: below the menu only while it clears the
            // player gauges at the bottom of the screen, otherwise beside it.
            const int32 TELEMETRY_H = 4 * 16;
            bool bAIPanel = ((m_page == PAGE_AIVIEW) && CEnemyAITrace::IsEnabled() && m_bAIDecisionPanel);
            int32 hudTop = static_cast<int32>(static_cast<float>(CScreen::Height()) * 0.80f);
            if (!bAIPanel && ((menuBottom + 8 + TELEMETRY_H) <= hudTop))
                DrawTelemetryCompact(12, menuBottom + 8);
            else if (!CEnemyAITrace::IsEnabled() && (m_page != PAGE_COMBATLAB) &&
                     ((m_iLastMenuRight + 560) <= CScreen::Width()))
                DrawTelemetryCompact(m_iLastMenuRight + 12, 12);
        }
        else
        {
#if defined(TMNT2_IMGUI)
            CPCDebugImGui::MarkHidden();
#endif
            DrawHint();
            if (m_bTelemetryEnabled)
                DrawTelemetry(12, 30);
        };
    };

private:
    void Open(CDebugToolsProcess& owner, int32 label)
    {
        m_bOpen = true;
        m_uNavHeld = 0;
        m_iNavFrames = 0;
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
                // Steps grow with the value and stop where the live enemies
                // reach the engine cap (see CDebugDifficulty::StepValue).
                CDebugDifficulty::StepValue(setting, direction, multiplier, difficulty);
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
                // Manual mode is only reachable with the Manual camera row ON.
                if ((m_iCameraMode == CMapCamera::MODE_MANUAL) && !CGameStageDebug::CAMERA_MANUAL_ALLOWED)
                    m_iCameraMode = InvClamp(m_iCameraMode + (direction >= 0 ? 1 : -1), 0, CMapCamera::MODEMAX - 1);

                m_bShowcaseCamera = (m_iCameraMode == CMapCamera::MODE_MANUAL);
                ApplyCameraMode();
            }
            else if (selected == 3)
            {
                SetManualCamera(!CGameStageDebug::CAMERA_MANUAL_ALLOWED);
            };
            break;

        case PAGE_GRAPHICS:
            if (selected == 1)
            {
                m_iMenuSize = InvClamp(m_iMenuSize + direction, 0, MENUSIZE_NUM - 1);
            }
            else if (selected == 2)
            {
                m_iMenuStyle = InvClamp(m_iMenuStyle + direction, 0, MENUSTYLE_NUM - 1);
            }
            else
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

        case PAGE_AIVIEW:
            AdjustAIViewItem(selected, direction, multiplier);
            break;

        case PAGE_OVERHAUL:
            CCombatOverhaul::AdjustItem(selected, direction, multiplier);
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
            else if (selected == 3)
                SetManualCamera(!CGameStageDebug::CAMERA_MANUAL_ALLOWED);
            break;

        case PAGE_GRAPHICS:
            if (selected == 1)
                m_iMenuSize = InvClamp(m_iMenuSize + 1, 0, MENUSIZE_NUM - 1);
            else if (selected == 2)
                m_iMenuStyle = InvClamp(m_iMenuStyle + 1, 0, MENUSTYLE_NUM - 1);
            else
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

        case PAGE_AIVIEW:
            ActivateAIViewItem(selected);
            break;

        case PAGE_OVERHAUL:
            if (!CCombatOverhaul::IsNumericItem(selected))
                CCombatOverhaul::AdjustItem(selected, 1, 1);
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
            else if (selected == 3)
                SetManualCamera(false);
            else
                ResetShowcaseCamera();
            break;

        case PAGE_GRAPHICS:
            if (selected == 1)
                m_iMenuSize = MENUSIZE_NORMAL;
            else if (selected == 2)
                m_iMenuStyle = MENUSTYLE_IMGUI;
            else
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

        case PAGE_AIVIEW:
            ResetAIViewItem(selected);
            break;

        case PAGE_OVERHAUL:
            CCombatOverhaul::ResetItem(selected);
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
            SetManualCamera(false);
            ResetShowcaseCamera();
            break;

        case PAGE_GRAPHICS:
            ApplyMSAA(0);
            m_iMenuSize = MENUSIZE_NORMAL;
            m_iMenuStyle = MENUSTYLE_IMGUI;
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

        case PAGE_AIVIEW:
            for (int32 i = 0; i < AIITEM_NUM; ++i)
                ResetAIViewItem(i);
            break;

        case PAGE_OVERHAUL:
            CCombatOverhaul::ResetAll();
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


    void SetManualCamera(bool bAllowed)
    {
        CGameStageDebug::CAMERA_MANUAL_ALLOWED = bAllowed;
        CCombatOverhaul::SetManualCamera(bAllowed);
        if (!bAllowed && (m_bShowcaseCamera || (m_iCameraMode == CMapCamera::MODE_MANUAL)))
            ResetShowcaseCamera();
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

        // Camera locked (default): the right stick, LT / RT and R3 never
        // touch the camera, with the menu open or closed. Only the Manual
        // camera row unlocks them (and then lock-on is off).
        if (!CGameStageDebug::CAMERA_MANUAL_ALLOWED)
        {
            if (m_bShowcaseCamera || (m_iCameraMode == CMapCamera::MODE_MANUAL))
                ResetShowcaseCamera();

            return;
        };

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
        case PAGE_CAMERA:     return 4;
        case PAGE_GRAPHICS:   return 3;
        case PAGE_HITBOXES:   return 3;
        case PAGE_TELEMETRY:  return 9;
        case PAGE_COMBATLAB:  return LABITEM_NUM;
        case PAGE_AIVIEW:     return AIITEM_NUM;
        case PAGE_OVERHAUL:   return CCombatOverhaul::GetItemNum();
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


    // Returns the bottom of the menu in screen pixels.
    int32 DrawMenu(void) const
    {
        RwCamera* pCamera = CCamera::CameraCurrent();
        if ((m_iMenuSize == MENUSIZE_CLASSIC) || !CGameFont::IsOpen() || !pCamera)
        {
            DrawMenuClassic();
            return 408;
        };

        static IM2DBATCH s_batch;
        static MENUTEXT s_aText[MENUTEXT_MAX];
        int32 nText = 0;

        s_batch.m_nVertex = 0;
        s_batch.m_fZ = RwIm2DGetNearScreenZ();
        s_batch.m_fRhw = 1.0f / RwCameraGetNearClipPlane(pCamera);

        // Layout is done in the game's virtual screen (640x480 units), the
        // same space the engine font and the move labels use.
        const float VX = CSprite::m_fVirtualScreenX;
        const float VY = CSprite::m_fVirtualScreenY;
        const float VW = CSprite::m_fVirtualScreenW;
        const float VH = CSprite::m_fVirtualScreenH;
        const float SW = static_cast<float>(CScreen::Width());
        const float SH = static_cast<float>(CScreen::Height());

        const float S       = s_afMenuSizeScale[m_iMenuSize];
        const float T       = 10.5f * S;    // item text height
        const float TSMALL  = 8.4f * S;
        const float ROW     = 13.0f * S;
        const float HEADROW = 10.5f * S;
        const float PAD     = 6.0f * S;
        const float X0      = VX + 8.0f;
        const float W       = MinF(384.0f * S, VW - 16.0f);
        const float X1      = X0 + W;
        const float TOP     = VY + 6.0f;

        const RwRGBA COL_PANEL    = { 0x0A, 0x0E, 0x15, 0xEE };
        const RwRGBA COL_BORDER   = { 0x3A, 0x9C, 0xD8, 0xFF };
        const RwRGBA COL_TITLEBAR = { 0x14, 0x3A, 0x5C, 0xFF };
        const RwRGBA COL_TAB      = { 0x1C, 0x26, 0x34, 0xFF };
        const RwRGBA COL_TABON    = { 0xFF, 0xA8, 0x28, 0xFF };
        const RwRGBA COL_ROWSEL   = { 0x1E, 0x4A, 0x74, 0xF0 };
        const RwRGBA COL_ROWALT   = { 0xFF, 0xFF, 0xFF, 0x0A };
        const RwRGBA COL_LINE     = { 0x3A, 0x9C, 0xD8, 0x70 };
        const RwRGBA COL_TRACK    = { 0xFF, 0xFF, 0xFF, 0x26 };
        const RwRGBA COL_FILL     = { 0x48, 0xC8, 0xFF, 0xE0 };
        const RwRGBA COL_FILLMAX  = { 0xFF, 0xA8, 0x28, 0xF0 };
        const RwRGBA TXT_TITLE    = { 0xFF, 0xFF, 0xFF, 0xFF };
        const RwRGBA TXT_ACCENT   = { 0x78, 0xD8, 0xFF, 0xFF };
        const RwRGBA TXT_ITEM     = { 0xE4, 0xEA, 0xF4, 0xFF };
        const RwRGBA TXT_SEL      = { 0xFF, 0xD0, 0x60, 0xFF };
        const RwRGBA TXT_VALUE    = { 0xB8, 0xF0, 0xFF, 0xFF };
        const RwRGBA TXT_ON       = { 0x70, 0xF0, 0x90, 0xFF };
        const RwRGBA TXT_OFF      = { 0x98, 0xA0, 0xB0, 0xFF };
        const RwRGBA TXT_DIM      = { 0xA0, 0xAC, 0xC0, 0xFF };
        const RwRGBA TXT_DARK     = { 0x10, 0x14, 0x1A, 0xFF };

        auto quad = [&](float x0, float y0, float x1, float y1, const RwRGBA& color)
        {
            Im2DQuad(s_batch,
                     ((x0 - VX) / VW) * SW, ((y0 - VY) / VH) * SH,
                     ((x1 - VX) / VW) * SW, ((y1 - VY) / VH) * SH,
                     color);
        };

        // Text is anchored on its baseline; rowTop/rowHeight centre it in a row.
        auto text = [&](float x, float rowTop, float rowHeight, float height, const RwRGBA& color, const char* pszText)
        {
            if ((nText >= MENUTEXT_MAX) || !pszText[0])
                return;

            MENUTEXT& rText = s_aText[nText++];
            rText.m_fX = x;
            rText.m_fY = rowTop + (rowHeight * 0.5f) + (height * 0.30f);
            rText.m_fHeight = height;
            rText.m_color = color;
            std::snprintf(rText.m_szText, sizeof(rText.m_szText), "%s", pszText);
            MenuSanitize(rText.m_szText);
        };

        auto textRight = [&](float xRight, float rowTop, float rowHeight, float height, const RwRGBA& color, const char* pszText)
        {
            char buffer[200];
            std::snprintf(buffer, sizeof(buffer), "%s", pszText);
            MenuSanitize(buffer);
            text(xRight - MenuTextWidth(buffer, height), rowTop, rowHeight, height, color, buffer);
        };

        // Two quads are reserved for the border and the panel; their size is
        // only known once the content has been laid out.
        int32 panelVertex = s_batch.m_nVertex;
        quad(X0, TOP, X1, TOP + 1.0f, COL_BORDER);
        quad(X0, TOP, X1, TOP + 1.0f, COL_PANEL);

        float y = TOP;

        // ---- title bar ----
        const float TITLE = 15.0f * S;
        quad(X0, y, X1, y + TITLE, COL_TITLEBAR);
        text(X0 + PAD, y, TITLE, T, TXT_TITLE, "TMNT2 DEBUG TOOLS");
        {
            char status[64];
            std::snprintf(status, sizeof(status), "%s   %s",
                          (CCombatLab::IsActive() ? "COMBAT LAB" : "GAME PAUSED"), MENU_VERSION);
            textRight(X1 - PAD, y, TITLE, TSMALL, TXT_ACCENT, status);
        };
        y += TITLE + (4.0f * S);

        // ---- tabs: wrap into rows, then stretch each row to the full width
        // so every row lines up; text is centred in its tab ----
        {
            const float TABH   = 15.0f * S;
            const float TABT   = 8.4f * S;
            const float TABGAP = 3.0f * S;
            const float ROWGAP = 3.5f * S;
            const float LEFT   = X0 + PAD;
            const float AVAIL  = (X1 - PAD) - LEFT;

            float aWidth[PAGE_NUM];
            int32 aRowStart[PAGE_NUM + 1];
            int32 nRow = 0;
            float rowWidth = 0.0f;
            for (int32 i = 0; i < PAGE_NUM; ++i)
            {
                aWidth[i] = MenuTextWidth(s_apszPageName[i], TABT) + (12.0f * S);
                float add = aWidth[i] + ((rowWidth > 0.0f) ? TABGAP : 0.0f);
                if ((nRow == 0) || ((rowWidth + add) > AVAIL))
                {
                    aRowStart[nRow++] = i;
                    rowWidth = aWidth[i];
                }
                else
                {
                    rowWidth += add;
                };
            };
            aRowStart[nRow] = PAGE_NUM;

            for (int32 r = 0; r < nRow; ++r)
            {
                int32 first = aRowStart[r];
                int32 last = aRowStart[r + 1];
                float used = 0.0f;
                for (int32 i = first; i < last; ++i)
                    used += aWidth[i];
                float gaps = TABGAP * static_cast<float>(last - first - 1);
                float extra = MaxF(0.0f, (AVAIL - used - gaps) / static_cast<float>(last - first));
                // the last row is only stretched when it is nearly full
                if ((r == (nRow - 1)) && (nRow > 1) && ((used + gaps) < (AVAIL * 0.70f)))
                    extra = MinF(extra, 10.0f * S);

                float cx = LEFT;
                for (int32 i = first; i < last; ++i)
                {
                    float w = aWidth[i] + extra;
                    bool bActive = (i == static_cast<int32>(m_page));
                    quad(cx, y, cx + w, y + TABH, (bActive ? COL_ROWSEL : COL_TAB));
                    if (bActive)
                        quad(cx, y + TABH - (2.2f * S), cx + w, y + TABH, COL_TABON);
                    float textW = MenuTextWidth(s_apszPageName[i], TABT);
                    text(cx + ((w - textW) * 0.5f), y - (0.8f * S), TABH, TABT, (bActive ? TXT_SEL : TXT_DIM), s_apszPageName[i]);
                    cx += w + TABGAP;
                };
                y += TABH + ((r < (nRow - 1)) ? ROWGAP : 0.0f);
            };
            y += 5.0f * S;
        };
        quad(X0 + PAD, y, X1 - PAD, y + 0.8f, COL_LINE);
        y += 3.0f * S;

        // ---- item list ----
        int32 itemCount = GetItemCount(m_page);
        int32 selected = m_aiSelection[m_page];

        // Height left for the list once the fixed parts below it are counted.
        const float FIXED_BELOW = 96.0f * S;
        float listBudget = MinF(246.0f * S, (VY + VH - 6.0f) - y - FIXED_BELOW);
        if (listBudget < (ROW * 4.0f))
            listBudget = ROW * 4.0f;

        auto rowsHeight = [&](int32 first, int32 last) -> float
        {
            float h = 0.0f;
            for (int32 i = first; i <= last; ++i)
            {
                if (GetSectionTitle(m_page, i))
                    h += HEADROW;
                h += ROW;
            };
            return h;
        };

        int32& rScroll = m_aiScroll[m_page];
        if ((rScroll < 0) || (rScroll >= itemCount) || (rowsHeight(0, itemCount - 1) <= listBudget))
            rScroll = 0;
        if (selected < rScroll)
            rScroll = selected;
        while ((rScroll < selected) && (rowsHeight(rScroll, selected) > listBudget))
            ++rScroll;

        int32 lastShown = rScroll - 1;
        float used = 0.0f;
        for (int32 i = rScroll; i < itemCount; ++i)
        {
            const char* pszSection = GetSectionTitle(m_page, i);
            float need = ROW + (pszSection ? HEADROW : 0.0f);
            if ((used + need) > (listBudget + 0.01f))
                break;

            if (pszSection)
            {
                text(X0 + PAD, y, HEADROW, TSMALL, TXT_ACCENT, pszSection);
                float lineX = X0 + PAD + MenuTextWidth(pszSection, TSMALL) + (5.0f * S);
                if (lineX < (X1 - PAD))
                    quad(lineX, y + (HEADROW * 0.5f), X1 - PAD, y + (HEADROW * 0.5f) + 0.7f, COL_LINE);
                y += HEADROW;
            };

            bool bSelected = (i == selected);
            if (bSelected)
            {
                quad(X0 + 2.0f, y, X1 - 2.0f, y + ROW, COL_ROWSEL);
                quad(X0 + 2.0f, y, X0 + 2.0f + (2.2f * S), y + ROW, COL_TABON);
            }
            else if (i & 1)
            {
                quad(X0 + 2.0f, y, X1 - 2.0f, y + ROW, COL_ROWALT);
            };

            // "Label  value": the item formatters separate them with 2+ spaces.
            char line[200];
            FormatItemText(i, line, sizeof(line));
            char* pszValue = std::strstr(line, "  ");
            if (pszValue)
            {
                *pszValue = '\0';
                ++pszValue;
                while (*pszValue == ' ')
                    ++pszValue;
            };

            float labelX = X0 + PAD + (4.0f * S);
            float valueRight = X1 - PAD;
            float sliderX0 = 0.0f;
            float sliderX1 = 0.0f;
            float fraction = -1.0f;
            bool bAtMax = false;
            if ((m_page == PAGE_DIFFICULTY) && (i >= 1) && (i <= CDebugDifficulty::SETTING_NUM))
            {
                CDebugDifficulty::SETTING setting = static_cast<CDebugDifficulty::SETTING>(i - 1);
                float value = CDebugDifficulty::GetValue(setting, CGameData::Option().Play().GetDifficulty());
                float minimum = CDebugDifficulty::GetMinimum(setting);
                float maximum = MaxF(CDebugDifficulty::GetEffectiveMaximum(setting), value);
                fraction = (maximum > minimum ? Clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f) : 1.0f);
                bAtMax = (fraction >= 0.999f);
                sliderX0 = X0 + (W * 0.50f);
                sliderX1 = X0 + (W * 0.63f);
            };

            float labelHeight = T;
            float labelRoom = (fraction >= 0.0f ? sliderX0 - labelX - (6.0f * S) : W * 0.62f);
            float valueWidth = (pszValue ? MenuTextWidth(pszValue, T) : 0.0f);
            if (fraction < 0.0f)
                labelRoom = MaxF(labelRoom, (valueRight - valueWidth - (6.0f * S)) - labelX);
            float labelWidth = MenuTextWidth(line, labelHeight);
            if (labelWidth > labelRoom)
                labelHeight = MaxF(T * 0.72f, T * (labelRoom / labelWidth));

            text(labelX, y, ROW, labelHeight, (bSelected ? TXT_SEL : TXT_ITEM), line);

            if (pszValue && pszValue[0])
            {
                RwRGBA color = TXT_VALUE;
                if (!std::strncmp(pszValue, "ON", 2))
                    color = TXT_ON;
                else if (!std::strncmp(pszValue, "OFF", 3) || !std::strncmp(pszValue, "EMPTY", 5))
                    color = TXT_OFF;

                float valueHeight = T;
                float valueRoom = valueRight - (fraction >= 0.0f ? sliderX1 + (4.0f * S) : labelX + MinF(labelWidth, labelRoom) + (6.0f * S));
                if ((valueWidth > valueRoom) && (valueRoom > 1.0f))
                    valueHeight = MaxF(T * 0.72f, T * (valueRoom / valueWidth));

                textRight(valueRight, y, ROW, valueHeight, color, pszValue);
            };

            if (fraction >= 0.0f)
            {
                float barY0 = y + (ROW * 0.5f) - (1.6f * S);
                float barY1 = y + (ROW * 0.5f) + (1.6f * S);
                quad(sliderX0, barY0, sliderX1, barY1, COL_TRACK);
                if (fraction > 0.0f)
                    quad(sliderX0, barY0, sliderX0 + ((sliderX1 - sliderX0) * fraction), barY1, (bAtMax ? COL_FILLMAX : COL_FILL));
            };

            y += ROW;
            used += need;
            lastShown = i;
        };

        y += 3.0f * S;
        quad(X0 + PAD, y, X1 - PAD, y + 0.8f, COL_LINE);
        y += 3.0f * S;

        // ---- help for the selected item ----
        const char* scope = "";
        const char* description = "";
        GetItemHelp(selected, scope, description);

        const float SCOPEROW = 11.0f * S;
        text(X0 + PAD, y, SCOPEROW, TSMALL, TXT_ACCENT, scope);
        if ((rScroll > 0) || (lastShown < (itemCount - 1)))
        {
            char range[48];
            std::snprintf(range, sizeof(range), "items %d-%d of %d", rScroll + 1, lastShown + 1, itemCount);
            textRight(X1 - PAD, y, SCOPEROW, TSMALL, TXT_DIM, range);
        };
        y += SCOPEROW;

        static char s_aLine[6][200];
        const float DESCT = 9.4f * S;
        const float DESCROW = 11.0f * S;
        int32 nLine = MenuWrap(description, DESCT, W - (PAD * 2.0f), s_aLine, 4);
        for (int32 n = 0; n < nLine; ++n)
            text(X0 + PAD, y + (DESCROW * static_cast<float>(n)), DESCROW, DESCT, TXT_ITEM, s_aLine[n]);
        y += DESCROW * static_cast<float>(nLine < 3 ? 3 : nLine);    // stable height

        y += 2.0f * S;
        quad(X0 + PAD, y, X1 - PAD, y + 0.8f, COL_LINE);
        y += 2.0f * S;

        // ---- controls ----
        static const char* s_apszHint[] =
        {
            "Keys:  Arrows move / change (hold = faster)   PgUp/PgDn x5   Enter run   Backspace reset item   Del reset tab   Q/E tab   Esc close",
            "Pad:  D-pad or stick   LB/RB tab   A run   X reset item   Y reset tab   B close   Start pause   (camera: only with Camera > Manual camera ON)",
        };
        const float HINTT = 7.6f * S;
        const float HINTROW = 9.0f * S;
        for (int32 h = 0; h < COUNT_OF(s_apszHint); ++h)
        {
            int32 n = MenuWrap(s_apszHint[h], HINTT, W - (PAD * 2.0f), s_aLine, 3);
            for (int32 k = 0; k < n; ++k)
            {
                text(X0 + PAD, y, HINTROW, HINTT, TXT_DIM, s_aLine[k]);
                y += HINTROW;
            };
        };
        y += 3.0f * S;

        // ---- patch the reserved border + panel quads to the final size ----
        {
            int32 saved = s_batch.m_nVertex;
            s_batch.m_nVertex = panelVertex;
            quad(X0 - 1.0f, TOP - 1.0f, X1 + 1.0f, y + 1.0f, COL_BORDER);
            quad(X0, TOP, X1, y, COL_PANEL);
            s_batch.m_nVertex = saved;
        };
        m_iLastMenuRight = static_cast<int32>((((X1 + 1.0f) - VX) / VW) * SW);

        CSystem2D::PushRenderState();
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, s_batch.m_aVertex, s_batch.m_nVertex);
        for (int32 n = 0; n < nText; ++n)
        {
            const MENUTEXT& rText = s_aText[n];
            CGameFont::SetHeight(rText.m_fHeight);
            CGameFont::SetRGBA(rText.m_color);
            CGameFont::Show(rText.m_szText, rText.m_fX, rText.m_fY);
        };
        CSystem2D::PopRenderState();

        return static_cast<int32>((((y + 1.0f) - VY) / VH) * SH);
    };


    void DrawMenuClassic(void) const
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


    // "Label  x1.65   50-99%": the multiplier plus what it currently gives
    // on the enemies that are alive, in the setting's real unit.
    void FormatDifficultyValue(
        CDebugDifficulty::SETTING setting,
        GAMETYPES::DIFFICULTY difficulty,
        char* buffer,
        size_t capacity
    ) const
    {
        float value = CDebugDifficulty::GetValue(setting, difficulty);
        CDebugDifficulty::UNIT unit = CDebugDifficulty::GetUnit(setting);

        char effect[64];
        effect[0] = '\0';

        if (unit != CDebugDifficulty::UNIT_MULTIPLIER)
        {
            CDebugDifficulty::LIVEINFO info;
            if (!CDebugDifficulty::GetLiveInfo(setting, info))
            {
                std::snprintf(effect, sizeof(effect), "   not used here");
            }
            else
            {
                const char* pszMax = ((info.m_fNowMin >= (info.m_fCap - 0.5f)) ? " MAX" : "");
                int32 lo = static_cast<int32>(info.m_fNowMin + 0.5f);
                int32 hi = static_cast<int32>(info.m_fNowMax + 0.5f);

                switch (unit)
                {
                case CDebugDifficulty::UNIT_PERCENT:
                    if (lo == hi)
                        std::snprintf(effect, sizeof(effect), "   %d%%%s", hi, pszMax);
                    else
                        std::snprintf(effect, sizeof(effect), "   %d-%d%%%s", lo, hi, pszMax);
                    break;

                case CDebugDifficulty::UNIT_DISTANCE:
                    if (lo == hi)
                        std::snprintf(effect, sizeof(effect), "   range %d%s", hi, pszMax);
                    else
                        std::snprintf(effect, sizeof(effect), "   range %d-%d%s", lo, hi, pszMax);
                    break;

                default:
                    // AI awareness: front detection reach in world units
                    if ((info.m_fNowMax - info.m_fNowMin) < 0.05f)
                        std::snprintf(effect, sizeof(effect), "   reach %.1f", info.m_fNowMax);
                    else
                        std::snprintf(effect, sizeof(effect), "   reach %.1f-%.1f", info.m_fNowMin, info.m_fNowMax);
                    break;
                };
            };
        };

        std::snprintf(buffer, capacity, "%-30s x%.2f%s", s_apszDifficultySettingName[setting], value, effect);
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
                FormatDifficultyValue(setting, difficulty, buffer, capacity);
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
            else if (index == 3)
                std::snprintf(buffer, capacity, "Manual camera (RS, LT, RT)    %s", (CGameStageDebug::CAMERA_MANUAL_ALLOWED ? "ON" : "OFF"));
            else
                std::snprintf(buffer, capacity, "Reset to default game camera");
            break;

        case PAGE_GRAPHICS:
            if (index == 1)
                std::snprintf(buffer, capacity, "Debug menu size               %s", s_apszMenuSizeName[m_iMenuSize]);
            else if (index == 2)
            {
                const char* pszReason = "";
#if defined(TMNT2_IMGUI)
                if ((m_iMenuStyle == MENUSTYLE_IMGUI) && !CPCDebugImGui::IsAvailable())
                    pszReason = CPCDebugImGui::GetFailReason();
#endif
                std::snprintf(buffer, capacity, "Debug menu style              %s%s%s", s_apszMenuStyleName[m_iMenuStyle],
                              (pszReason[0] ? " - " : ""), pszReason);
            }
            else
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

        case PAGE_AIVIEW:
            FormatAIViewItem(index, buffer, capacity);
            break;

        case PAGE_OVERHAUL:
            CCombatOverhaul::FormatItem(index, buffer, capacity);
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
                    "Automatic is the game's own camera. Manual (orbit with the right stick) can only be chosen while Manual camera below is ON." :
                (index == 3 ?
                    "OFF (default): the camera is fully automatic; right stick, LT, RT and R3 never move it, and R3 / right stick are lock-on. ON: right stick orbits, LT / RT change distance, R3 resets the camera, and lock-on is switched off." :
                    "Returns to automatic game camera, normal zoom, and the correct player path mode.")));
            break;

        case PAGE_GRAPHICS:
            scope = ((index == 1) || (index == 2) ? "MENU APPEARANCE" : "LIVE GRAPHICS DEVICE");
            description = (index == 1 ?
                "Size of this menu (text size in the ImGui menu). Classic is the previous small fixed-size text menu." :
                (index == 2 ?
                "ImGui: resizable window with mouse support (click rows, drag sliders, double-click to run), plus the keyboard and pad controls. Game font: the in-game font menu. The window layout is saved in TMNT2_debugtools_imgui.ini." :
                "Changes MSAA Off/2X/4X/8X immediately; fullscreen and driver support are required."));
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

        case PAGE_AIVIEW:
            GetAIViewItemHelp(index, scope, description);
            break;

        case PAGE_OVERHAUL:
            CCombatOverhaul::GetItemHelp(index, scope, description);
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
            std::snprintf(buffer, capacity, "Enemy type                    %s  (%d of %d)",
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


    // ---- ImGui menu (Debug Tools v1.2.14) ----
    //
    // A second front end for the same menu: pages, items, values, help and
    // actions all come from the functions the game-font menu uses
    // (GetItemCount, FormatItemText, GetItemHelp, AdjustSelected, ...).
    // Mouse clicks are queued here (Draw is const) and applied in Move.

    enum UIACTIONTYPE
    {
        UIACTION_PAGE = 0,
        UIACTION_SELECT,
        UIACTION_ADJUST,
        UIACTION_ACTIVATE,
        UIACTION_RESET_ITEM,
        UIACTION_RESET_PAGE,
        UIACTION_SET_DIFFICULTY,
        UIACTION_CLOSE,
    };

    struct UIACTION
    {
        int32 m_type;
        int32 m_page;
        int32 m_index;
        int32 m_direction;
        float m_fValue;
    };

    enum { UIACTION_MAX = 16 };


    void PushUIAction(int32 type, int32 page, int32 index, int32 direction = 0, float value = 0.0f) const
    {
        if (m_nUIAction >= UIACTION_MAX)
            return;

        UIACTION& rAction = m_aUIAction[m_nUIAction++];
        rAction.m_type = type;
        rAction.m_page = page;
        rAction.m_index = index;
        rAction.m_direction = direction;
        rAction.m_fValue = value;
    };


    // Returns false when the menu was closed by an action.
    bool ApplyUIActions(CDebugToolsProcess& owner)
    {
        int32 count = m_nUIAction;
        m_nUIAction = 0;

        for (int32 n = 0; n < count; ++n)
        {
            const UIACTION& rAction = m_aUIAction[n];
            if ((rAction.m_page < 0) || (rAction.m_page >= PAGE_NUM))
                continue;

            if (rAction.m_type == UIACTION_CLOSE)
            {
                Close(owner);
                return false;
            };

            if (rAction.m_page != static_cast<int32>(m_page))
            {
                m_bLabEnterArmed = false;
                m_page = static_cast<PAGE>(rAction.m_page);
            };

            if (rAction.m_type == UIACTION_PAGE)
            {
                int32 itemCount = GetItemCount(m_page);
                if (m_aiSelection[m_page] >= itemCount)
                    m_aiSelection[m_page] = (itemCount - 1);
                continue;
            };

            if (rAction.m_type == UIACTION_RESET_PAGE)
            {
                ResetPage();
                continue;
            };

            int32 itemCount = GetItemCount(m_page);
            if ((rAction.m_index < 0) || (rAction.m_index >= itemCount))
                continue;

            if (m_aiSelection[m_page] != rAction.m_index)
            {
                m_bLabEnterArmed = false;
                m_aiSelection[m_page] = rAction.m_index;
            };

            switch (rAction.m_type)
            {
            case UIACTION_ADJUST:
                AdjustSelected(rAction.m_direction, 1);
                break;

            case UIACTION_ACTIVATE:
                ActivateSelected(owner);
                break;

            case UIACTION_RESET_ITEM:
                ResetSelected();
                break;

            case UIACTION_SET_DIFFICULTY:
                if ((m_page == PAGE_DIFFICULTY) && (rAction.m_index >= 1) && (rAction.m_index <= CDebugDifficulty::SETTING_NUM))
                {
                    CDebugDifficulty::SetCustomValue(
                        static_cast<CDebugDifficulty::SETTING>(rAction.m_index - 1),
                        rAction.m_fValue,
                        CGameData::Option().Play().GetDifficulty()
                    );
                };
                break;

            default:
                break;
            };

            if (!m_bOpen)
                return false;
        };

        return true;
    };


    bool IsImGuiMenuStyle(void) const
    {
#if defined(TMNT2_IMGUI)
        return ((m_iMenuStyle == MENUSTYLE_IMGUI) && CPCDebugImGui::IsAvailable());
#else
        return false;
#endif
    };


#if defined(TMNT2_IMGUI)
    static ImVec4 ImColor4(const RwRGBA& color)
    {
        return ImVec4(color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f, color.alpha / 255.0f);
    };


    // Splits "Label  value" (two or more spaces) like the game-font menu does.
    static const char* SplitItemText(char* line)
    {
        char* pszValue = std::strstr(line, "  ");
        if (!pszValue)
            return "";

        *pszValue = '\0';
        ++pszValue;
        while (*pszValue == ' ')
            ++pszValue;
        return pszValue;
    };


    void DrawImGuiItems(PAGE page, float height) const
    {
        const int32 itemCount = GetItemCount(page);
        const int32 selected = m_aiSelection[page];
        const bool bSelectionMoved = ((selected != m_iUILastSelection) || (static_cast<int32>(page) != m_iUILastPage));
        const GAMETYPES::DIFFICULTY difficulty = CGameData::Option().Play().GetDifficulty();
        const float em = ImGui::GetFontSize();

        const ImVec4 COL_ACCENT = ImVec4(0.47f, 0.85f, 1.00f, 1.00f);
        const ImVec4 COL_ON     = ImVec4(0.44f, 0.94f, 0.56f, 1.00f);
        const ImVec4 COL_OFF    = ImVec4(0.60f, 0.63f, 0.69f, 1.00f);
        const ImVec4 COL_VALUE  = ImVec4(0.72f, 0.94f, 1.00f, 1.00f);
        const ImVec4 COL_SEL    = ImVec4(1.00f, 0.82f, 0.38f, 1.00f);

        if (!ImGui::BeginChild("items", ImVec2(0.0f, height), ImGuiChildFlags_Borders))
        {
            ImGui::EndChild();
            return;
        };

        for (int32 i = 0; i < itemCount; ++i)
        {
            const char* pszSection = GetSectionTitle(page, i);
            if (pszSection)
            {
                if (i > 0)
                    ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
                ImGui::SeparatorText(pszSection);
                ImGui::PopStyleColor();
            };

            char line[200];
            FormatItemText(i, line, sizeof(line));
            const char* pszValue = SplitItemText(line);

            bool bDifficultySetting = ((page == PAGE_DIFFICULTY) && (i >= 1) && (i <= CDebugDifficulty::SETTING_NUM));
            bool bToggle = (!std::strcmp(pszValue, "ON") || !std::strcmp(pszValue, "OFF"));
            bool bAction = (pszValue[0] == '\0');
            bool bSelected = (i == selected);

            ImGui::PushID(i);

            // Row: label (selectable, full width) / control / value
            float rowStartY = ImGui::GetCursorPosY();
            float rowRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::PushStyleColor(ImGuiCol_Text, (bSelected ? COL_SEL : ImGui::GetStyleColorVec4(ImGuiCol_Text)));
            if (ImGui::Selectable(line, bSelected,
                                  ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap,
                                  ImVec2(0.0f, em * 1.45f)))
            {
                PushUIAction(UIACTION_SELECT, page, i);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    PushUIAction((bAction ? UIACTION_ACTIVATE : UIACTION_ADJUST), page, i, 1);
            };
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            {
                const char* scope = "";
                const char* description = "";
                GetItemHelp(i, scope, description);
                if (description && description[0])
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(em * 26.0f);
                    if (scope && scope[0])
                        ImGui::TextColored(COL_ACCENT, "%s", scope);
                    ImGui::TextUnformatted(description);
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                };
            };

            if (bSelected && bSelectionMoved)
                ImGui::SetScrollHereY(0.5f);

            // controls are right-aligned on the same row
            float right = rowRight;
            float controlW = em * 11.0f;
            float valueW = em * 9.5f;
            float rowY = rowStartY + ((em * 1.45f) - ImGui::GetFrameHeight()) * 0.5f;

            if (bDifficultySetting)
            {
                CDebugDifficulty::SETTING setting = static_cast<CDebugDifficulty::SETTING>(i - 1);
                float value = CDebugDifficulty::GetValue(setting, difficulty);
                float minimum = CDebugDifficulty::GetMinimum(setting);
                float maximum = MaxF(CDebugDifficulty::GetEffectiveMaximum(setting), value);

                // value text after the "x1.00" multiplier
                const char* pszEffect = std::strstr(pszValue, "   ");
                while (pszEffect && (*pszEffect == ' '))
                    ++pszEffect;

                ImGui::SetCursorPos(ImVec2(right - controlW - valueW, rowY));
                ImGui::SetNextItemWidth(controlW);
                float edit = value;
                if (ImGui::SliderFloat("##v", &edit, minimum, maximum, "x%.2f", ImGuiSliderFlags_AlwaysClamp))
                    PushUIAction(UIACTION_SET_DIFFICULTY, page, i, 0, edit);
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    PushUIAction(UIACTION_RESET_ITEM, page, i);

                if (pszEffect && pszEffect[0])
                {
                    ImGui::SetCursorPos(ImVec2(right - valueW + (em * 0.5f), rowY + ImGui::GetStyle().FramePadding.y));
                    ImGui::TextColored(COL_VALUE, "%s", pszEffect);
                };
            }
            else if (bToggle)
            {
                bool bOn = (pszValue[1] == 'N');
                ImGui::SetCursorPos(ImVec2(right - valueW, rowY));
                if (ImGui::Checkbox("##t", &bOn))
                    PushUIAction(UIACTION_ADJUST, page, i, 1);
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored((bOn ? COL_ON : COL_OFF), "%s", pszValue);
            }
            else if (bAction)
            {
                ImGui::SetCursorPos(ImVec2(right - valueW, rowY));
                if (ImGui::Button("Run", ImVec2(valueW, 0.0f)))
                    PushUIAction(UIACTION_ACTIVATE, page, i);
            }
            else
            {
                float buttonW = ImGui::GetFrameHeight();
                ImGui::SetCursorPos(ImVec2(right - valueW - controlW, rowY));
                if (ImGui::ArrowButton("##l", ImGuiDir_Left))
                    PushUIAction(UIACTION_ADJUST, page, i, -1);
                ImGui::SameLine();
                ImGui::SetCursorPosX(right - valueW - (buttonW + ImGui::GetStyle().ItemSpacing.x));
                if (ImGui::ArrowButton("##r", ImGuiDir_Right))
                    PushUIAction(UIACTION_ADJUST, page, i, 1);

                // values can be long (enemy names, file names): clip to the row
                ImGui::SetCursorPos(ImVec2(right - valueW - controlW + buttonW + ImGui::GetStyle().ItemSpacing.x, rowY + ImGui::GetStyle().FramePadding.y));
                ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
                                    ImVec2(ImGui::GetWindowPos().x + right - valueW - buttonW - (ImGui::GetStyle().ItemSpacing.x * 2.0f),
                                           ImGui::GetCursorScreenPos().y + em * 1.5f), true);
                ImGui::TextColored(COL_VALUE, "%s", pszValue);
                ImGui::PopClipRect();
            };

            ImGui::SetCursorPosY(rowStartY + (em * 1.45f) + ImGui::GetStyle().ItemSpacing.y);
            ImGui::PopID();
        };

        ImGui::EndChild();

        m_iUILastSelection = selected;
        m_iUILastPage = static_cast<int32>(page);
    };


    void DrawImGuiAIWindow(void) const
    {
        if (!CEnemyAITrace::IsEnabled() || !m_bAIDecisionPanel)
            return;

        static PANELLINE s_aLine[64];
        int32 nLine = CollectAIDecisionLines(s_aLine, COUNT_OF(s_aLine));

        const ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.45f, io.DisplaySize.y * 0.55f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.42f, io.DisplaySize.y * 0.36f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Enemy AI decisions###AIDecisions"))
        {
            if (ImGui::ArrowButton("##prev", ImGuiDir_Left))
                PushUIAction(UIACTION_ADJUST, PAGE_AIVIEW, AIITEM_SELECT, -1);
            ImGui::SameLine();
            if (ImGui::ArrowButton("##next", ImGuiDir_Right))
                PushUIAction(UIACTION_ADJUST, PAGE_AIVIEW, AIITEM_SELECT, 1);
            ImGui::SameLine();
            ImGui::TextDisabled("inspect another enemy");
            ImGui::Separator();

            if (nLine == 0)
                ImGui::TextDisabled("No enemy to inspect yet.");
            for (int32 n = 0; n < nLine; ++n)
                ImGui::TextColored(ImColor4(s_aLine[n].m_color), "%s", s_aLine[n].m_szText);
        };
        ImGui::End();
    };
#endif /* defined(TMNT2_IMGUI) */


    // Returns false when ImGui could not draw; the caller uses the game-font menu.
    bool DrawMenuImGui(void) const
    {
#if defined(TMNT2_IMGUI)
        float scale = s_afMenuSizeScale[(m_iMenuSize == MENUSIZE_CLASSIC) ? MENUSIZE_NORMAL : m_iMenuSize];
        if (!CPCDebugImGui::BeginFrame(scale))
            return false;

        const ImGuiIO& io = ImGui::GetIO();
        const float em = ImGui::GetFontSize();
        const ImVec4 COL_ACCENT = ImVec4(0.47f, 0.85f, 1.00f, 1.00f);

        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.012f, io.DisplaySize.y * 0.02f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(MinF(io.DisplaySize.x * 0.46f, em * 44.0f), io.DisplaySize.y * 0.86f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(em * 26.0f, em * 18.0f), io.DisplaySize);

        char title[96];
        std::snprintf(title, sizeof(title), "TMNT2 Debug Tools  %s  -  %s###DebugTools",
                      MENU_VERSION, (CCombatLab::IsActive() ? "COMBAT LAB" : "GAME PAUSED"));

        if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse))
        {
            // ---- tabs (wrap to the window width) ----
            float avail = ImGui::GetContentRegionAvail().x;
            float x = 0.0f;
            for (int32 i = 0; i < PAGE_NUM; ++i)
            {
                float w = ImGui::CalcTextSize(s_apszPageName[i]).x + (ImGui::GetStyle().FramePadding.x * 2.0f);
                if ((i > 0) && ((x + ImGui::GetStyle().ItemSpacing.x + w) <= avail))
                {
                    ImGui::SameLine();
                    x += ImGui::GetStyle().ItemSpacing.x;
                }
                else
                {
                    x = 0.0f;
                };
                x += w;

                bool bActive = (i == static_cast<int32>(m_page));
                if (bActive)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.29f, 0.45f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.82f, 0.38f, 1.0f));
                };
                if (ImGui::Button(s_apszPageName[i]))
                    PushUIAction(UIACTION_PAGE, i, 0);
                if (bActive)
                {
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x, max.y - 3.0f), max, IM_COL32(255, 168, 40, 255), 2.0f);
                    ImGui::PopStyleColor(2);
                };
            };
            ImGui::Spacing();

            // ---- items, then help for the selected one ----
            float helpH = (em * 4.2f) + (ImGui::GetStyle().ItemSpacing.y * 3.0f);
            float footerH = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
            float listH = MaxF(em * 6.0f, ImGui::GetContentRegionAvail().y - helpH - footerH);
            DrawImGuiItems(m_page, listH);

            {
                const char* scope = "";
                const char* description = "";
                GetItemHelp(m_aiSelection[m_page], scope, description);
                ImGui::BeginChild("help", ImVec2(0.0f, helpH), ImGuiChildFlags_None);
                if (scope && scope[0])
                    ImGui::TextColored(COL_ACCENT, "%s", scope);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(description ? description : "");
                ImGui::PopTextWrapPos();
                ImGui::EndChild();
            };

            // ---- footer ----
            if (ImGui::Button("Reset item"))
                PushUIAction(UIACTION_RESET_ITEM, m_page, m_aiSelection[m_page]);
            ImGui::SameLine();
            if (ImGui::Button("Reset tab"))
                PushUIAction(UIACTION_RESET_PAGE, m_page, 0);
            ImGui::SameLine();
            if (ImGui::Button("Close"))
                PushUIAction(UIACTION_CLOSE, m_page, 0);
            ImGui::SameLine();
            ImGui::TextDisabled("F4 close   Q/E tab   arrows / mouse   double-click runs   right-click slider resets");

            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            m_iLastMenuBottom = static_cast<int32>(pos.y + size.y);
            m_iLastMenuRight = static_cast<int32>(pos.x + size.x);
        };
        ImGui::End();

        if (m_page == PAGE_AIVIEW)
            DrawImGuiAIWindow();

        CPCDebugImGui::EndFrame();
        return true;
#else
        return false;
#endif
    };


    // ---- In-world AI awareness zones (read-only; same formulas as the AI) ----
    //
    //  view   : CAIUtils::IsObjectViewArea2 - reach = suitable + (radius - suitable) * view ratio,
    //           view ratio = front + (rear - front) * |angle| / 180 deg
    //  attack : CBaseGrapplerAI::IsTakeAttack - distance <= suitable, |angle| <= 36 deg x front view
    //  run    : CBaseGrapplerAI::IsTakeRunAttack - 2 x suitable while running, same cone
    //  guard  : CBaseGrapplerAI::IsTakeGuard - distance <= suitable, |angle| <= 90 deg x front view
    //  probes : terrain checks at suitable distance ahead (attack) and behind (Foot Ninja back-away)
    //  patrol : CAIUtils::IsInsidePatrolArea - circle at the patrol origin (no circle = unlimited)

    struct ZONEDRAW
    {
        IM2DBATCH* m_pBatch;
        RwMatrix m_view;
        float m_fSW;
        float m_fSH;
    };


    static void ZoneFlush(ZONEDRAW& zd)
    {
        if (zd.m_pBatch->m_nVertex > 0)
            RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, zd.m_pBatch->m_aVertex, zd.m_pBatch->m_nVertex);
        zd.m_pBatch->m_nVertex = 0;
    };


    static void ZoneRoom(ZONEDRAW& zd, int32 need)
    {
        if ((zd.m_pBatch->m_nVertex + need) > IM2DBATCH::VERTEX_MAX)
            ZoneFlush(zd);
    };


    static RwV3d ZoneCam(const ZONEDRAW& zd, const RwV3d& world)
    {
        RwV3d cam = Math::VECTOR3_ZERO;
        RwV3dTransformPoint(&cam, &world, &zd.m_view);
        return cam;
    };


    static void ZoneScreen(const ZONEDRAW& zd, const RwV3d& cam, float& sx, float& sy)
    {
        sx = (cam.x / cam.z) * zd.m_fSW;
        sy = (cam.y / cam.z) * zd.m_fSH;
    };


    // World-space line, clipped against the camera near plane.
    static void ZoneLine(ZONEDRAW& zd, const RwV3d& a, const RwV3d& b, float width, const RwRGBA& color)
    {
        RwV3d ca = ZoneCam(zd, a);
        RwV3d cb = ZoneCam(zd, b);
        const float NEAR_Z = 0.6f;
        if ((ca.z < NEAR_Z) && (cb.z < NEAR_Z))
            return;

        if (ca.z < NEAR_Z)
        {
            float t = (NEAR_Z - ca.z) / (cb.z - ca.z);
            ca.x += (cb.x - ca.x) * t; ca.y += (cb.y - ca.y) * t; ca.z = NEAR_Z;
        }
        else if (cb.z < NEAR_Z)
        {
            float t = (NEAR_Z - cb.z) / (ca.z - cb.z);
            cb.x += (ca.x - cb.x) * t; cb.y += (ca.y - cb.y) * t; cb.z = NEAR_Z;
        };

        float x0, y0, x1, y1;
        ZoneScreen(zd, ca, x0, y0);
        ZoneScreen(zd, cb, x1, y1);
        if ((MaxF(x0, x1) < -zd.m_fSW) || (MinF(x0, x1) > (zd.m_fSW * 2.0f)) ||
            (MaxF(y0, y1) < -zd.m_fSH) || (MinF(y0, y1) > (zd.m_fSH * 2.0f)))
            return;

        ZoneRoom(zd, 6);
        Im2DLine(*zd.m_pBatch, x0, y0, x1, y1, width, color);
    };


    // World-space triangle; dropped when any corner is behind the camera.
    static void ZoneTri(ZONEDRAW& zd, const RwV3d& a, const RwV3d& b, const RwV3d& c, const RwRGBA& color)
    {
        const RwV3d aCam[3] = { ZoneCam(zd, a), ZoneCam(zd, b), ZoneCam(zd, c) };
        for (int32 i = 0; i < 3; ++i)
        {
            if (aCam[i].z < 0.6f)
                return;
        };

        ZoneRoom(zd, 3);
        IM2DBATCH& batch = *zd.m_pBatch;
        for (int32 i = 0; i < 3; ++i)
        {
            float sx, sy;
            ZoneScreen(zd, aCam[i], sx, sy);
            RwIm2DVertex* pVertex = &batch.m_aVertex[batch.m_nVertex++];
            RwIm2DVertexSetScreenX(pVertex, sx);
            RwIm2DVertexSetScreenY(pVertex, sy);
            RwIm2DVertexSetScreenZ(pVertex, batch.m_fZ);
            RwIm2DVertexSetIntRGBA(pVertex, color.red, color.green, color.blue, color.alpha);
            RwIm2DVertexSetU(pVertex, 0.0f, batch.m_fRhw);
            RwIm2DVertexSetV(pVertex, 0.0f, batch.m_fRhw);
            RwIm2DVertexSetRecipCameraZ(pVertex, batch.m_fRhw);
        };
    };


    static RwV3d ZonePoint(const RwV3d& center, float direction, float radius)
    {
        RwV3d point = center;
        point.x += Math::Sin(direction) * radius;
        point.z += Math::Cos(direction) * radius;
        return point;
    };


    // Sector from -half..+half around 'direction'. 'fill' alpha 0 = outline only.
    static void ZoneWedge(ZONEDRAW& zd, const RwV3d& center, float direction, float half, float radius,
                          const RwRGBA& fill, const RwRGBA& line, float width)
    {
        if ((radius <= 0.0f) || (half <= 0.0f))
            return;

        half = MinF(half, MATH_PI);
        int32 segments = Clamp(static_cast<int32>(half * 12.0f), 4, 36);
        RwV3d prev = ZonePoint(center, direction - half, radius);
        bool bFull = (half >= (MATH_PI - 0.001f));
        if (!bFull)
            ZoneLine(zd, center, prev, width, line);

        for (int32 i = 1; i <= segments; ++i)
        {
            float a = direction - half + ((2.0f * half) * (static_cast<float>(i) / static_cast<float>(segments)));
            RwV3d next = ZonePoint(center, a, radius);
            if (fill.alpha)
                ZoneTri(zd, center, prev, next, fill);
            ZoneLine(zd, prev, next, width, line);
            prev = next;
        };

        if (!bFull)
            ZoneLine(zd, center, prev, width, line);
    };


    // Where a player can become the target: the view reach of IsObjectViewArea2,
    // limited by the action radius that ICalcHateOfGeometry checks next.
    static float ZoneViewReach(const ENEMYTYPES::CHARACTERISTIC& c, float angleDiff)
    {
        float ratio = c.m_fRatioOfFrontView + ((c.m_fRatioOfRearView - c.m_fRatioOfFrontView) * (std::fabs(angleDiff) * MATH_INV_PI));
        float reach = c.m_fDistanceOfSuitable + ((c.m_fRadiusOfAction - c.m_fDistanceOfSuitable) * ratio);
        return MinF(reach, c.m_fRadiusOfAction);
    };


    CEnemy* FindZoneFocusEnemy(void) const
    {
        if (CEnemyAITrace::IsEnabled())
        {
            const CEnemyAITrace::SLOT* pSlot = CEnemyAITrace::GetSlot(GetAISelectedSlot());
            if (pSlot)
            {
                CGameObject* pObject = CGameObjectManager::GetObject(pSlot->m_hEnemy);
                if (pObject && (pObject->GetType() == GAMEOBJECTTYPE::ENEMY))
                    return static_cast<CEnemy*>(pObject);
            };
        };

        // Without the visualizer: the enemy nearest to P1.
        IGamePlayer* pPlayer = (CGameProperty::GetPlayerNum() > 0 ? CGameProperty::Player(0) : nullptr);
        if (!pPlayer)
            return nullptr;

        RwV3d vPlayer = Math::VECTOR3_ZERO;
        pPlayer->GetPosition(&vPlayer);

        CEnemy* pBest = nullptr;
        float bestSq = FLT_MAX;
        for (int32 i = 0; i < CGameProperty::GetEnemyMax(); ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!pEnemy)
                continue;
            RwV3d v = Math::VECTOR3_ZERO;
            pEnemy->GetPosition(&v);
            float dx = v.x - vPlayer.x;
            float dz = v.z - vPlayer.z;
            if (((dx * dx) + (dz * dz)) < bestSq)
            {
                bestSq = (dx * dx) + (dz * dz);
                pBest = pEnemy;
            };
        };
        return pBest;
    };


    void DrawAwarenessZones(void) const
    {
        if (m_iAIZones == AIZONES_OFF)
            return;

        RwCamera* pCamera = CCamera::CameraCurrent();
        if (!pCamera || !CGameStage::GetCurrent() || !CGameProperty::IsCameraViewAvailable())
            return;

        static IM2DBATCH s_batch;
        ZONEDRAW zd;
        zd.m_pBatch = &s_batch;
        s_batch.m_nVertex = 0;
        s_batch.m_fZ = RwIm2DGetNearScreenZ();
        s_batch.m_fRhw = 1.0f / RwCameraGetNearClipPlane(pCamera);
        RwMatrixSetIdentity(&zd.m_view);
        CGameProperty::GetCameraViewMatrix(&zd.m_view);
        zd.m_fSW = static_cast<float>(CScreen::Width());
        zd.m_fSH = static_cast<float>(CScreen::Height());
        const float px = MaxF(1.0f, zd.m_fSH / 480.0f);

        IGamePlayer* pPlayer = (CGameProperty::GetPlayerNum() > 0 ? CGameProperty::Player(0) : nullptr);
        bool bPlayer = (pPlayer && pPlayer->IsAlive());
        RwV3d vPlayer = Math::VECTOR3_ZERO;
        if (bPlayer)
            pPlayer->GetPosition(&vPlayer);

        CEnemy* pFocus = FindZoneFocusEnemy();

        struct ZLABEL { RwV3d pos; char text[48]; RwRGBA color; };
        static ZLABEL s_aLabel[64];
        int32 nLabel = 0;
        auto addLabel = [&](const RwV3d& pos, const RwRGBA& color, const char* pszText)
        {
            if (!m_bAIZoneLabels || (nLabel >= COUNT_OF(s_aLabel)))
                return;
            s_aLabel[nLabel].pos = pos;
            s_aLabel[nLabel].color = color;
            std::snprintf(s_aLabel[nLabel].text, sizeof(s_aLabel[nLabel].text), "%s", pszText);
            ++nLabel;
        };

        CSystem2D::PushRenderState();
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);

        for (int32 i = 0; i < CGameProperty::GetEnemyMax(); ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!pEnemy)
                continue;
            if ((m_iAIZones == AIZONES_FOCUS) && (pEnemy != pFocus))
                continue;

            const CEnemyCharacter& rChr = pEnemy->Character();
            const ENEMYTYPES::CHARACTERISTIC& c = rChr.AICharacteristic();
            const ENEMYTYPES::FEATURE& feature = rChr.Feature();
            CEnemyCharacter::DEBUGAIKIND kind = rChr.GetDebugAIKind();
            bool bGrappler = ((kind == CEnemyCharacter::DEBUGAIKIND_GRAPPLER) || (kind == CEnemyCharacter::DEBUGAIKIND_FOOTNINJA));
            bool bFocus = (pEnemy == pFocus);

            RwV3d vFoot = Math::VECTOR3_ZERO;
            rChr.Compositor().GetFootPosition(&vFoot);
            vFoot.y += 0.04f;
            float dir = rChr.Compositor().GetDirection();
            float S = c.m_fDistanceOfSuitable;
            float lineW = (bFocus ? 2.0f : 1.4f) * px;

            // where P1 is, measured the way IsObjectViewArea measures it
            bool bInView = false;
            bool bInAttack = false;
            bool bInGuard = false;
            float playerDist = 0.0f;
            if (bPlayer)
            {
                RwV3d at = vPlayer;
                at.y = vFoot.y;
                float dx = at.x - vFoot.x;
                float dz = at.z - vFoot.z;
                playerDist = std::sqrt((dx * dx) + (dz * dz));
                float diff = CEnemyUtils::RadianCorrect(Math::ATan2(dx, dz) - dir);
                bInView = (ZoneViewReach(c, diff) > playerDist);
                bInAttack = (playerDist <= S) && (std::fabs(diff) <= (c.m_fRatioOfFrontView * MATH_DEG2RAD(36.0f)));
                bInGuard = (playerDist <= S) && (std::fabs(diff) <= (c.m_fRatioOfFrontView * MATH_DEG2RAD(90.0f)));
            };

            // ---- patrol area ----
            if (feature.m_fPatrolRadius > 0.0f)
            {
                RwV3d origin = feature.m_vPatrolOrigin;
                origin.y = vFoot.y;
                ZoneWedge(zd, origin, 0.0f, MATH_PI, feature.m_fPatrolRadius,
                          { 0, 0, 0, 0 }, { 0x60, 0xE0, 0x80, 0x90 }, 1.2f * px);
                if (bFocus)
                {
                    char text[48];
                    std::snprintf(text, sizeof(text), "patrol %.1f", feature.m_fPatrolRadius);
                    addLabel(ZonePoint(origin, 0.0f, feature.m_fPatrolRadius), { 0x60, 0xE0, 0x80, 0xFF }, text);
                };
            };

            // ---- view / awareness area ----
            {
                const int32 SEG = 48;
                RwRGBA fill = (bInView ? RwRGBA{ 0x40, 0xC8, 0xFF, 0x30 } : RwRGBA{ 0x40, 0xC8, 0xFF, 0x18 });
                RwRGBA line = (bInView ? RwRGBA{ 0x70, 0xE0, 0xFF, 0xE0 } : RwRGBA{ 0x40, 0xA8, 0xE0, 0x90 });
                RwV3d prev = ZonePoint(vFoot, dir - MATH_PI, ZoneViewReach(c, -MATH_PI));
                for (int32 s = 1; s <= SEG; ++s)
                {
                    float diff = -MATH_PI + (MATH_PI2 * (static_cast<float>(s) / static_cast<float>(SEG)));
                    RwV3d next = ZonePoint(vFoot, dir + diff, ZoneViewReach(c, diff));
                    ZoneTri(zd, vFoot, prev, next, fill);
                    ZoneLine(zd, prev, next, lineW, line);
                    prev = next;
                };

                char text[48];
                std::snprintf(text, sizeof(text), "view %.1f", ZoneViewReach(c, 0.0f));
                if (bFocus)
                {
                    addLabel(ZonePoint(vFoot, dir, ZoneViewReach(c, 0.0f)), { 0x70, 0xE0, 0xFF, 0xFF }, text);
                    std::snprintf(text, sizeof(text), "rear %.1f", ZoneViewReach(c, MATH_PI));
                    addLabel(ZonePoint(vFoot, dir + MATH_PI, ZoneViewReach(c, MATH_PI)), { 0x70, 0xE0, 0xFF, 0xFF }, text);
                };
            };

            if (bGrappler)
            {
                float attackHalf = c.m_fRatioOfFrontView * MATH_DEG2RAD(36.0f);
                float guardHalf = c.m_fRatioOfFrontView * MATH_DEG2RAD(90.0f);

                // guard cone (outline), run-attack reach (outline), attack cone (filled)
                ZoneWedge(zd, vFoot, dir, guardHalf, S, { 0, 0, 0, 0 },
                          (bInGuard ? RwRGBA{ 0xFF, 0xE0, 0x30, 0xF0 } : RwRGBA{ 0xFF, 0xE0, 0x30, 0x80 }), lineW);
                ZoneWedge(zd, vFoot, dir, attackHalf, S * 2.0f, { 0, 0, 0, 0 }, { 0xFF, 0x98, 0x28, 0x70 }, 1.0f * px);
                ZoneWedge(zd, vFoot, dir, attackHalf, S,
                          (bInAttack ? RwRGBA{ 0xFF, 0x40, 0x40, 0x70 } : RwRGBA{ 0xFF, 0x40, 0x40, 0x30 }),
                          { 0xFF, 0x50, 0x50, 0xE0 }, lineW);

                // terrain probe points
                RwV3d probe = ZonePoint(vFoot, dir, S);
                ZoneLine(zd, ZonePoint(probe, dir + (MATH_PI * 0.5f), 0.18f), ZonePoint(probe, dir - (MATH_PI * 0.5f), 0.18f), 2.0f * px, { 0xFF, 0xFF, 0xFF, 0xD0 });
                if (kind == CEnemyCharacter::DEBUGAIKIND_FOOTNINJA)
                {
                    RwV3d back = ZonePoint(vFoot, dir + MATH_PI, S);
                    ZoneLine(zd, vFoot, back, 1.0f * px, { 0xFF, 0x98, 0x28, 0x90 });
                    ZoneLine(zd, ZonePoint(back, dir + (MATH_PI * 0.5f), 0.18f), ZonePoint(back, dir - (MATH_PI * 0.5f), 0.18f), 2.0f * px, { 0xFF, 0x98, 0x28, 0xE0 });
                };

                if (bFocus)
                {
                    char text[48];
                    std::snprintf(text, sizeof(text), "attack %.1f / %.0f deg", S, MATH_RAD2DEG(attackHalf) * 2.0f);
                    addLabel(ZonePoint(vFoot, dir + attackHalf, S), { 0xFF, 0x70, 0x70, 0xFF }, text);
                    std::snprintf(text, sizeof(text), "guard %.0f deg", MATH_RAD2DEG(MinF(guardHalf, MATH_PI)) * 2.0f);
                    addLabel(ZonePoint(vFoot, dir - guardHalf, S), { 0xFF, 0xE0, 0x30, 0xFF }, text);
                    std::snprintf(text, sizeof(text), "run attack %.1f", S * 2.0f);
                    addLabel(ZonePoint(vFoot, dir, S * 2.0f), { 0xFF, 0xA8, 0x40, 0xFF }, text);
                };
            };

            // facing arrow
            {
                RwV3d tip = ZonePoint(vFoot, dir, MaxF(0.8f, S * 0.6f));
                ZoneLine(zd, vFoot, tip, 2.4f * px, { 0xFF, 0xFF, 0xFF, 0xE0 });
                ZoneLine(zd, tip, ZonePoint(tip, dir + (MATH_PI * 0.85f), 0.3f), 2.4f * px, { 0xFF, 0xFF, 0xFF, 0xE0 });
                ZoneLine(zd, tip, ZonePoint(tip, dir - (MATH_PI * 0.85f), 0.3f), 2.4f * px, { 0xFF, 0xFF, 0xFF, 0xE0 });
            };

            // line to P1 coloured by what P1 is inside
            if (bPlayer && bInView)
            {
                RwV3d at = vPlayer;
                at.y = vFoot.y;
                RwRGBA color = (bInAttack ? RwRGBA{ 0xFF, 0x50, 0x50, 0xC0 } : RwRGBA{ 0x70, 0xE0, 0xFF, 0x90 });
                ZoneLine(zd, vFoot, at, 1.0f * px, color);
            };

            if (bFocus || (m_iAIZones == AIZONES_FOCUS))
            {
                char text[48];
                std::snprintf(text, sizeof(text), "P1 %.1f%s%s%s", playerDist,
                              (bInView ? "  seen" : "  not seen"),
                              (bGrappler && bInAttack ? "  ATTACK" : ""),
                              (bGrappler && bInGuard && !bInAttack ? "  guard" : ""));
                if (bPlayer)
                    addLabel(vFoot, { 0xFF, 0xFF, 0xFF, 0xFF }, text);
            };
        };

        ZoneFlush(zd);
        CSystem2D::PopRenderState();

        // labels
        if (nLabel > 0)
        {
            m_font.Background({ 0x00, 0x00, 0x00, 0xA0 });
            for (int32 n = 0; n < nLabel; ++n)
            {
                RwV3d cam = ZoneCam(zd, s_aLabel[n].pos);
                if (cam.z < 0.6f)
                    continue;
                float sx, sy;
                ZoneScreen(zd, cam, sx, sy);
                if ((sx < 0.0f) || (sy < 0.0f) || (sx > (zd.m_fSW - 40.0f)) || (sy > (zd.m_fSH - 16.0f)))
                    continue;
                m_font.Color(s_aLabel[n].color);
                m_font.Position(static_cast<int32>(sx) + 4, static_cast<int32>(sy) - 8);
                m_font.Print("%s", s_aLabel[n].text);
            };
        };
    };


    // ---- AI VIEW tab: live enemy AI visualizer (CEnemyAITrace) ----

    int32 GetAISelectedSlot(void) const
    {
        const CEnemyAITrace::SLOT* pSlot = CEnemyAITrace::GetSlot(m_iAISelected);
        if (pSlot)
            return m_iAISelected;

        // fall back to the lowest-numbered live enemy
        int32 best = -1;
        int32 bestLabel = 1 << 30;
        for (int32 i = 0; i < CEnemyAITrace::GetSlotNum(); ++i)
        {
            const CEnemyAITrace::SLOT* pAny = CEnemyAITrace::GetSlot(i);
            if (pAny && (pAny->m_iLabel < bestLabel))
            {
                best = i;
                bestLabel = pAny->m_iLabel;
            };
        };
        return best;
    };


    void CycleAISelection(int32 direction)
    {
        // order slots by their display number and step through them
        int32 aOrder[CEnemyAITrace::SLOT_MAX];
        int32 n = 0;
        for (int32 label = 1; label <= CEnemyAITrace::SLOT_MAX; ++label)
        {
            for (int32 i = 0; i < CEnemyAITrace::GetSlotNum(); ++i)
            {
                const CEnemyAITrace::SLOT* pSlot = CEnemyAITrace::GetSlot(i);
                if (pSlot && (pSlot->m_iLabel == label))
                    aOrder[n++] = i;
            };
        };

        if (n == 0)
            return;

        int32 current = GetAISelectedSlot();
        int32 pos = 0;
        for (int32 k = 0; k < n; ++k)
        {
            if (aOrder[k] == current)
                pos = k;
        };

        m_iAISelected = aOrder[InvClamp(pos + direction, 0, n - 1)];
    };


    void AdjustAIViewItem(int32 selected, int32 direction, int32 multiplier)
    {
        switch (selected)
        {
        case AIITEM_ENABLE:
            CEnemyAITrace::SetEnabled(!CEnemyAITrace::IsEnabled());
            break;
        case AIITEM_RADAR_RANGE:
            m_fAIRadarRange = Clamp(m_fAIRadarRange + static_cast<float>(direction * multiplier), 3.0f, 40.0f);
            break;
        case AIITEM_TIMELINE:
            m_bAITimeline = !m_bAITimeline;
            break;
        case AIITEM_TIMELINE_SPAN:
            m_fAITimelineSpan = Clamp(m_fAITimelineSpan + static_cast<float>(direction * multiplier), 4.0f, 60.0f);
            break;
        case AIITEM_FREEZE:
            CEnemyAITrace::SetTimelinePaused(!CEnemyAITrace::IsTimelinePaused());
            break;
        case AIITEM_SELECT:
            CycleAISelection(direction);
            break;
        case AIITEM_ZONES:
            m_iAIZones = InvClamp(m_iAIZones + direction, 0, AIZONES_NUM - 1);
            break;
        case AIITEM_ZONE_LABELS:
            m_bAIZoneLabels = !m_bAIZoneLabels;
            break;
        case AIITEM_DECISIONS:
            m_bAIDecisionPanel = !m_bAIDecisionPanel;
            break;
        case AIITEM_RECORD:
            CEnemyAITrace::SetFileRecording(!CEnemyAITrace::IsFileRecording());
            break;
        default:
            break;
        };
    };


    void ActivateAIViewItem(int32 selected)
    {
        switch (selected)
        {
        case AIITEM_SUMMARY:
            CEnemyAITrace::WriteSummary("manual");
            break;

        case AIITEM_CLEAR:
            CEnemyAITrace::Clear();
            break;

        case AIITEM_SIX_NINJAS:
            if (CCombatLab::IsPlaying())
            {
                if (!CEnemyAITrace::IsEnabled())
                    CEnemyAITrace::SetEnabled(true);
                CCombatLab::SelectEnemyType(0);
                CCombatLab::SetEnemyCount(CCombatLab::SPAWN_MAX);
                CCombatLab::ResetScenario();
                CEnemyAITrace::Clear();
                ResetShowcaseCamera();
            };
            break;

        default:
            AdjustAIViewItem(selected, 1, 1);
            break;
        };
    };


    void ResetAIViewItem(int32 selected)
    {
        switch (selected)
        {
        case AIITEM_ENABLE:        CEnemyAITrace::SetEnabled(false); break;
        case AIITEM_RADAR_RANGE:   m_fAIRadarRange = 8.0f; break;
        case AIITEM_TIMELINE:      m_bAITimeline = true; break;
        case AIITEM_TIMELINE_SPAN: m_fAITimelineSpan = 12.0f; break;
        case AIITEM_FREEZE:        CEnemyAITrace::SetTimelinePaused(false); break;
        case AIITEM_DECISIONS:     m_bAIDecisionPanel = true; break;
        case AIITEM_RECORD:        CEnemyAITrace::SetFileRecording(false); break;
        case AIITEM_ZONES:         m_iAIZones = AIZONES_OFF; break;
        case AIITEM_ZONE_LABELS:   m_bAIZoneLabels = true; break;
        default: break;
        };
    };


    void FormatAIViewItem(int32 index, char* buffer, size_t capacity) const
    {
        switch (index)
        {
        case AIITEM_ENABLE:
            std::snprintf(buffer, capacity, "AI visualizer                 %s", CEnemyAITrace::IsEnabled() ? "ON" : "OFF");
            break;
        case AIITEM_RADAR_RANGE:
            std::snprintf(buffer, capacity, "Radar range                   %.0f units", m_fAIRadarRange);
            break;
        case AIITEM_TIMELINE:
            std::snprintf(buffer, capacity, "Timeline                      %s", m_bAITimeline ? "ON" : "OFF");
            break;
        case AIITEM_TIMELINE_SPAN:
            std::snprintf(buffer, capacity, "Timeline length               %.0f s", m_fAITimelineSpan);
            break;
        case AIITEM_FREEZE:
            std::snprintf(buffer, capacity, "Freeze timeline               %s", CEnemyAITrace::IsTimelinePaused() ? "ON" : "OFF");
            break;
        case AIITEM_SELECT:
            {
                int32 slot = GetAISelectedSlot();
                const CEnemyAITrace::SLOT* pSlot = CEnemyAITrace::GetSlot(slot);
                if (pSlot)
                {
                    char name[48];
                    FormatEnemyName(pSlot->m_id, name, sizeof(name));
                    std::snprintf(buffer, capacity, "Inspect enemy                 #%d %s", pSlot->m_iLabel, name);
                }
                else
                {
                    std::snprintf(buffer, capacity, "Inspect enemy                 none");
                };
            }
            break;
        case AIITEM_DECISIONS:
            std::snprintf(buffer, capacity, "Decision history panel        %s", m_bAIDecisionPanel ? "ON" : "OFF");
            break;
        case AIITEM_RECORD:
            std::snprintf(buffer, capacity, "Record to TMNT2_aitrace.jsonl  %s", CEnemyAITrace::IsFileRecording() ? "ON" : "OFF");
            break;
        case AIITEM_SUMMARY:
            std::snprintf(buffer, capacity, "Write overlap summary to file");
            break;
        case AIITEM_CLEAR:
            std::snprintf(buffer, capacity, "Clear recorded data");
            break;
        case AIITEM_SIX_NINJAS:
            std::snprintf(buffer, capacity, "Six Foot Ninjas test%s", CCombatLab::IsPlaying() ? "" : "  (in Combat Lab)");
            break;
        case AIITEM_ZONES:
            {
                static const char* s_apszZones[] = { "OFF", "ALL ENEMIES", "INSPECTED ENEMY" };
                std::snprintf(buffer, capacity, "Show awareness zones  %s", s_apszZones[Clamp(m_iAIZones, 0, AIZONES_NUM - 1)]);
            }
            break;
        case AIITEM_ZONE_LABELS:
            std::snprintf(buffer, capacity, "Zone labels  %s", m_bAIZoneLabels ? "ON" : "OFF");
            break;
        default:
            break;
        };
    };


    void GetAIViewItemHelp(int32 index, const char*& scope, const char*& description) const
    {
        switch (index)
        {
        case AIITEM_ENABLE:
            scope = "DIAGNOSTICS - OFF BY DEFAULT";
            description = "Records what each enemy's AI really evaluated: every roll with its threshold, the action checks and the order chosen. Read-only: no extra RNG calls, no AI calls, no behaviour change.";
            break;
        case AIITEM_ZONES:
        case AIITEM_ZONE_LABELS:
            scope = "IN-WORLD - DRAWN FROM EACH ENEMY'S LIVE VALUES";
            description = "Blue: what the enemy can notice (view reach, longer in front, shorter behind). Red: attack cone. Yellow: guard cone. Orange: run-attack reach. Green: patrol area. Lines turn bright when P1 is inside. Uses the current multipliers. Works without the visualizer.";
            break;
        case AIITEM_RADAR_RANGE:
            scope = "DISPLAY";
            description = "Top-down radar centred on P1, world-aligned (up = +Z). Inner ring = Foot Ninja attack distance (2.0).";
            break;
        case AIITEM_TIMELINE:
        case AIITEM_TIMELINE_SPAN:
            scope = "DISPLAY";
            description = "One row per enemy, coloured by its real status category over the last seconds. Colours: red attack, yellow guard, blue move, gray wait, orange back-away, purple hit/down.";
            break;
        case AIITEM_FREEZE:
            scope = "DISPLAY";
            description = "Stops the timeline view so a moment can be read. Recording continues; unfreeze to see new data.";
            break;
        case AIITEM_SELECT:
        case AIITEM_DECISIONS:
            scope = "DECISION HISTORY";
            description = "Shows the selected enemy's last decisions: why it re-decided, every roll (value vs threshold, retail vs current), checks, and the order it took. Left/Right picks the enemy.";
            break;
        case AIITEM_RECORD:
        case AIITEM_SUMMARY:
            scope = "FILE - TMNT2_aitrace.jsonl";
            description = "Writes multipliers, status changes, decisions with rolls, 10 Hz positions and overlap summaries. Open it in tools/AIVisualizer/ai_replay.html for replay and statistics.";
            break;
        case AIITEM_CLEAR:
            scope = "DIAGNOSTICS";
            description = "Clears histories, timelines and the overlap statistics.";
            break;
        default:
            scope = "COMBAT LAB";
            description = "Spawns six Foot Ninjas (sword) in the Foot Clan arena with a fixed seed and clears the statistics. Pick AI mode and multipliers first.";
            break;
        };
    };


    // Thin quad from (x0,y0) to (x1,y1), screen pixels.
    static void Im2DLine(IM2DBATCH& batch, float x0, float y0, float x1, float y1, float width, const RwRGBA& color)
    {
        if ((batch.m_nVertex + 6) > IM2DBATCH::VERTEX_MAX)
            return;

        float dx = x1 - x0;
        float dy = y1 - y0;
        float len = std::sqrt((dx * dx) + (dy * dy));
        if (len < 0.001f)
            return;

        float nx = (-dy / len) * (width * 0.5f);
        float ny = (dx / len) * (width * 0.5f);
        const float ax[6] = { x0 + nx, x1 + nx, x0 - nx, x0 - nx, x1 + nx, x1 - nx };
        const float ay[6] = { y0 + ny, y1 + ny, y0 - ny, y0 - ny, y1 + ny, y1 - ny };
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


    static void Im2DRing(IM2DBATCH& batch, float cx, float cy, float radius, float width, const RwRGBA& color)
    {
        const int32 SEGMENTS = 40;
        float px = cx + radius;
        float py = cy;
        for (int32 i = 1; i <= SEGMENTS; ++i)
        {
            float a = (static_cast<float>(i) / static_cast<float>(SEGMENTS)) * MATH_PI2;
            float nx = cx + (Math::Cos(a) * radius);
            float ny = cy + (Math::Sin(a) * radius);
            Im2DLine(batch, px, py, nx, ny, width, color);
            px = nx;
            py = ny;
        };
    };


    static RwRGBA CategoryRGBA(int32 category, uint8 alpha)
    {
        uint8 rgb[3];
        CEnemyAITrace::CategoryColor(category, rgb);
        RwRGBA color = { rgb[0], rgb[1], rgb[2], alpha };
        return color;
    };


    // Draws radar, overview, timeline and the decision panel.
    void DrawAIView(void) const
    {
        RwCamera* pCamera = CCamera::CameraCurrent();
        if (!CEnemyAITrace::IsEnabled() || !pCamera || !CGameStage::GetCurrent() || !CGameProperty::IsCameraViewAvailable())
            return;

        IGamePlayer* pPlayer = (CGameProperty::GetPlayerNum() > 0 ? CGameProperty::Player(0) : nullptr);
        if (!pPlayer || !pPlayer->IsAlive())
            return;

        static IM2DBATCH s_batch;
        s_batch.m_nVertex = 0;
        s_batch.m_fZ = RwIm2DGetNearScreenZ();
        s_batch.m_fRhw = 1.0f / RwCameraGetNearClipPlane(pCamera);

        const float SW = static_cast<float>(CScreen::Width());
        const float SH = static_cast<float>(CScreen::Height());
        const float px = MaxF(1.0f, SH / 480.0f);

        // ---- radar (right side, below the frame graph) ----
        float size = MinF(SH * 0.40f, SW * 0.26f);
        float rx1 = SW - (12.0f * px);
        float rx0 = rx1 - size;
        float ry0 = (m_bFrameGraph ? (88.0f * px) : (12.0f * px));
        float ry1 = ry0 + size;
        float cx = (rx0 + rx1) * 0.5f;
        float cy = (ry0 + ry1) * 0.5f;
        float scale = (size * 0.5f) / m_fAIRadarRange;

        RwV3d vPlayer = Math::VECTOR3_ZERO;
        pPlayer->GetPosition(&vPlayer);
        float fPlayerDir = pPlayer->GetRotY();

        Im2DQuad(s_batch, rx0, ry0, rx1, ry1, { 0x06, 0x0A, 0x12, 0xD0 });
        Im2DLine(s_batch, rx0, ry0, rx1, ry0, 1.5f, { 0x3A, 0x9C, 0xD8, 0xFF });
        Im2DLine(s_batch, rx0, ry1, rx1, ry1, 1.5f, { 0x3A, 0x9C, 0xD8, 0xFF });
        Im2DLine(s_batch, rx0, ry0, rx0, ry1, 1.5f, { 0x3A, 0x9C, 0xD8, 0xFF });
        Im2DLine(s_batch, rx1, ry0, rx1, ry1, 1.5f, { 0x3A, 0x9C, 0xD8, 0xFF });
        for (float r = 5.0f; r <= m_fAIRadarRange; r += 5.0f)
            Im2DRing(s_batch, cx, cy, r * scale, 1.0f, { 0xFF, 0xFF, 0xFF, 0x22 });
        Im2DRing(s_batch, cx, cy, 2.0f * scale, 1.2f, { 0xFF, 0x60, 0x60, 0x60 });

        auto toScreen = [&](const RwV3d& v, float& sx, float& sy)
        {
            sx = cx + ((v.x - vPlayer.x) * scale);
            sy = cy - ((v.z - vPlayer.z) * scale);
        };

        auto inside = [&](float sx, float sy) -> bool
        {
            return ((sx >= rx0 + 2.0f) && (sx <= rx1 - 2.0f) && (sy >= ry0 + 2.0f) && (sy <= ry1 - 2.0f));
        };

        // player: white triangle pointing where P1 faces
        {
            float fx = Math::Sin(fPlayerDir);
            float fy = -Math::Cos(fPlayerDir);
            float tip = 9.0f * px;
            Im2DLine(s_batch, cx - (fy * tip * 0.6f) - (fx * tip * 0.5f), cy + (fx * tip * 0.6f) - (fy * tip * 0.5f),
                     cx + (fx * tip), cy + (fy * tip), 2.0f * px, { 0xFF, 0xFF, 0xFF, 0xFF });
            Im2DLine(s_batch, cx + (fy * tip * 0.6f) - (fx * tip * 0.5f), cy - (fx * tip * 0.6f) - (fy * tip * 0.5f),
                     cx + (fx * tip), cy + (fy * tip), 2.0f * px, { 0xFF, 0xFF, 0xFF, 0xFF });
            Im2DQuad(s_batch, cx - (2.5f * px), cy - (2.5f * px), cx + (2.5f * px), cy + (2.5f * px), { 0xFF, 0xFF, 0xFF, 0xFF });
        };

        struct LABEL { float x; float y; int32 label; bool bSel; };
        LABEL aLabel[CEnemyAITrace::SLOT_MAX];
        int32 nLabel = 0;
        int32 selectedSlot = GetAISelectedSlot();

        for (int32 i = 0; i < CEnemyAITrace::GetSlotNum(); ++i)
        {
            const CEnemyAITrace::SLOT* pSlot = CEnemyAITrace::GetSlot(i);
            if (!pSlot)
                continue;

            CGameObject* pObject = CGameObjectManager::GetObject(pSlot->m_hEnemy);
            if (!pObject || (pObject->GetType() != GAMEOBJECTTYPE::ENEMY))
                continue;

            CEnemy* pEnemy = static_cast<CEnemy*>(pObject);
            RwV3d vPos = Math::VECTOR3_ZERO;
            pEnemy->GetPosition(&vPos);

            float sx = 0.0f;
            float sy = 0.0f;
            toScreen(vPos, sx, sy);
            bool bInside = inside(sx, sy);
            if (!bInside)
            {
                // clamp to the border so off-radar enemies stay visible
                sx = Clamp(sx, rx0 + (5.0f * px), rx1 - (5.0f * px));
                sy = Clamp(sy, ry0 + (5.0f * px), ry1 - (5.0f * px));
            };

            RwRGBA color = CategoryRGBA(pSlot->m_category, 0xFF);

            if ((pSlot->m_target >= 0) && bInside)
                Im2DLine(s_batch, sx, sy, cx, cy, 1.0f, { color.red, color.green, color.blue, 0x50 });

            float dir = pEnemy->Character().Compositor().GetDirection();
            float fx = Math::Sin(dir);
            float fy = -Math::Cos(dir);
            float half = 4.5f * px;
            if (pSlot->m_bInRange)
                Im2DQuad(s_batch, sx - half - (2.0f * px), sy - half - (2.0f * px), sx + half + (2.0f * px), sy + half + (2.0f * px),
                         { 0xFF, 0xFF, 0xFF, 0xD0 });
            if (i == selectedSlot)
                Im2DRing(s_batch, sx, sy, half * 2.4f, 1.5f * px, { 0xFF, 0xD0, 0x60, 0xFF });
            Im2DQuad(s_batch, sx - half, sy - half, sx + half, sy + half, color);
            Im2DLine(s_batch, sx, sy, sx + (fx * half * 2.6f), sy + (fy * half * 2.6f), 1.8f * px, color);

            if (nLabel < CEnemyAITrace::SLOT_MAX)
            {
                aLabel[nLabel].x = sx + half + (2.0f * px);
                aLabel[nLabel].y = sy - half - (6.0f * px);
                aLabel[nLabel].label = pSlot->m_iLabel;
                aLabel[nLabel].bSel = (i == selectedSlot);
                ++nLabel;
            };
        };

        // ---- timeline (bottom) ----
        const CEnemyAITrace::OVERVIEW& ov = CEnemyAITrace::Overview();
        float tx0 = 12.0f * px;
        float tx1 = SW - (12.0f * px);
        float rowH = 7.0f * px;
        int32 nRow = 0;
        int32 aRowSlot[CEnemyAITrace::SLOT_MAX];
        for (int32 label = 1; label <= CEnemyAITrace::SLOT_MAX; ++label)
        {
            for (int32 i = 0; i < CEnemyAITrace::GetSlotNum(); ++i)
            {
                const CEnemyAITrace::SLOT* pSlot = CEnemyAITrace::GetSlot(i);
                if (pSlot && (pSlot->m_iLabel == label) && (nRow < 8))
                    aRowSlot[nRow++] = i;
            };
        };

        float ty1 = SH - (10.0f * px);
        float ty0 = ty1 - (static_cast<float>(nRow) * (rowH + (1.0f * px))) - (2.0f * px);
        float labelW = 26.0f * px;
        float now = CEnemyAITrace::TimelineNow();
        float span = m_fAITimelineSpan;
        if (m_bAITimeline && (nRow > 0))
        {
            Im2DQuad(s_batch, tx0, ty0 - (2.0f * px), tx1, ty1 + (2.0f * px), { 0x06, 0x0A, 0x12, 0xC8 });
            float gx0 = tx0 + labelW;
            float gw = tx1 - gx0 - (4.0f * px);
            for (int32 r = 0; r < nRow; ++r)
            {
                const CEnemyAITrace::SLOT* pSlot = CEnemyAITrace::GetSlot(aRowSlot[r]);
                float y0 = ty0 + (static_cast<float>(r) * (rowH + (1.0f * px)));
                float y1 = y0 + rowH;
                int32 count = (pSlot->m_nSegment < CEnemyAITrace::SEGMENT_HISTORY ? pSlot->m_nSegment : CEnemyAITrace::SEGMENT_HISTORY);
                for (int32 k = 0; k < count; ++k)
                {
                    int32 index = pSlot->m_nSegment - 1 - k;
                    const CEnemyAITrace::SEGMENT& rSeg = pSlot->m_aSegment[index % CEnemyAITrace::SEGMENT_HISTORY];
                    float segEnd = now;
                    if (k > 0)
                        segEnd = pSlot->m_aSegment[(index + 1) % CEnemyAITrace::SEGMENT_HISTORY].m_fStart;
                    if (segEnd < (now - span))
                        break;
                    float segStart = MaxF(rSeg.m_fStart, now - span);
                    if (segStart >= segEnd)
                        continue;
                    float x0 = gx0 + (((segStart - (now - span)) / span) * gw);
                    float x1 = gx0 + (((segEnd - (now - span)) / span) * gw);
                    Im2DQuad(s_batch, x0, y0, MaxF(x0 + 1.0f, x1), y1, CategoryRGBA(rSeg.m_category, 0xE8));
                };
            };

            // one tick per second
            for (float t = 0.0f; t <= span; t += 1.0f)
            {
                float x = gx0 + gw - ((t / span) * gw);
                Im2DLine(s_batch, x, ty0 - (2.0f * px), x, ty0, 1.0f, { 0xFF, 0xFF, 0xFF, 0x60 });
            };
        };

        CSystem2D::PushRenderState();
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, s_batch.m_aVertex, s_batch.m_nVertex);
        CSystem2D::PopRenderState();

        // ---- text ----
        m_font.Background({ 0x00, 0x00, 0x00, 0x00 });
        for (int32 n = 0; n < nLabel; ++n)
        {
            m_font.Color(aLabel[n].bSel ? RwRGBA{ 0xFF, 0xD0, 0x60, 0xFF } : RwRGBA{ 0xFF, 0xFF, 0xFF, 0xFF });
            m_font.Position(static_cast<int32>(aLabel[n].x), static_cast<int32>(aLabel[n].y));
            m_font.Print("%d", aLabel[n].label);
        };

        m_font.Background({ 0x06, 0x0A, 0x12, 0xD0 });
        m_font.Color({ 0x78, 0xD8, 0xFF, 0xFF });
        m_font.Position(static_cast<int32>(rx0), static_cast<int32>(ry0) - 16);
        m_font.Print("AI VIEW  r=%.0f  %s", m_fAIRadarRange, CEnemyAITrace::IsTimelinePaused() ? "FROZEN" : "");

        int32 oy = static_cast<int32>(ry1) + 4;
        int32 ox = static_cast<int32>(rx0);
        m_font.Color({ 0xE8, 0xEE, 0xF8, 0xFF });
        m_font.Position(ox, oy);
        m_font.Print("Active %d  attack %d  in range %d", ov.m_nActive, ov.m_nAttacking, ov.m_nInRange);
        m_font.Position(ox, oy + 16);
        m_font.Print("guard %d  move %d  wait %d  hit %d  back %d",
                     ov.m_anCategory[CEnemyAITrace::CAT_GUARD], ov.m_anCategory[CEnemyAITrace::CAT_MOVE],
                     ov.m_anCategory[CEnemyAITrace::CAT_WAIT], ov.m_anCategory[CEnemyAITrace::CAT_HIT],
                     ov.m_anCategory[CEnemyAITrace::CAT_SPECIAL]);
        {
            float overlapSec = 0.0f;
            for (int32 k = 2; k <= CEnemyAITrace::SLOT_MAX; ++k)
                overlapSec += ov.m_afAttackingTime[k];
            float pct = (ov.m_fSpan > 0.0f ? (overlapSec / ov.m_fSpan) * 100.0f : 0.0f);
            int32 startPct = (ov.m_nAttackStarts > 0 ? (ov.m_nAttackStartsOverlapping * 100) / ov.m_nAttackStarts : 0);
            m_font.Position(ox, oy + 32);
            m_font.Print("Max at once %d  2+ attacking %.0f%% of %.0fs", ov.m_nMaxAttacking, pct, ov.m_fSpan);
            m_font.Position(ox, oy + 48);
            m_font.Print("Attack starts %d, %d%% while another attacked", ov.m_nAttackStarts, startPct);
        };

        if (m_bAITimeline && (nRow > 0))
        {
            m_font.Background({ 0x00, 0x00, 0x00, 0x00 });
            m_font.Color({ 0xE8, 0xEE, 0xF8, 0xFF });
            for (int32 r = 0; r < nRow; ++r)
            {
                const CEnemyAITrace::SLOT* pSlot = CEnemyAITrace::GetSlot(aRowSlot[r]);
                float y0 = ty0 + (static_cast<float>(r) * (rowH + (1.0f * px)));
                m_font.Position(static_cast<int32>(tx0 + (3.0f * px)), static_cast<int32>(y0 + (rowH * 0.5f)) - 8);
                m_font.Print("#%d", pSlot->m_iLabel);
            };
            m_font.Background({ 0x06, 0x0A, 0x12, 0xC8 });
            m_font.Color({ 0xA0, 0xAC, 0xC0, 0xFF });
            m_font.Position(static_cast<int32>(tx0), static_cast<int32>(ty0) - 20);
            m_font.Print("Timeline last %.0fs (now at right)  red attack  yellow guard  blue move  gray wait  orange back-away  purple hit", span);
        };

        // ---- decision panel ----
        if (m_bAIDecisionPanel)
        {
            int32 panelX = 12;
            int32 panelY = (m_bOpen ? m_iLastMenuBottom + 8 : 30);
            int32 panelBottom = static_cast<int32>(m_bAITimeline && (nRow > 0) ? ty0 - (24.0f * px) : SH - 12.0f);
            if (!m_bOpen || ((m_page == PAGE_AIVIEW) && !m_bImGuiMenuShown))
                DrawAIDecisionPanel(panelX, panelY, panelBottom);
        };
    };


    struct PANELLINE
    {
        RwRGBA m_color;
        char m_szText[256];
    };


    void DrawAIDecisionPanel(int32 x, int32 y, int32 bottom) const
    {
        if ((y + 40) > bottom)
            return;

        static PANELLINE s_aLine[64];
        int32 maxLines = Clamp((bottom - y) / 16, 0, COUNT_OF(s_aLine));
        int32 nLine = CollectAIDecisionLines(s_aLine, maxLines);

        m_font.Background({ 0x06, 0x0A, 0x12, 0xD8 });
        for (int32 n = 0; n < nLine; ++n)
        {
            m_font.Color(s_aLine[n].m_color);
            m_font.Position(x, y + (n * 16));
            m_font.Print("%s", s_aLine[n].m_szText);
        };
    };


    // Lines of the decision panel for the inspected enemy; shared by the
    // game-font overlay and the ImGui window.
    int32 CollectAIDecisionLines(PANELLINE* aLine, int32 maxLines) const
    {
        const CEnemyAITrace::SLOT* pSlot = CEnemyAITrace::GetSlot(GetAISelectedSlot());
        if (!pSlot || (maxLines <= 0))
            return 0;

        CGameObject* pObject = CGameObjectManager::GetObject(pSlot->m_hEnemy);
        if (!pObject || (pObject->GetType() != GAMEOBJECTTYPE::ENEMY))
            return 0;

        CEnemy* pEnemy = static_cast<CEnemy*>(pObject);
        const CEnemyCharacter& rChr = pEnemy->Character();
        const ENEMYTYPES::CHARACTERISTIC& rNow = rChr.AICharacteristic();
        const ENEMYTYPES::CHARACTERISTIC& rBase = rChr.GetDebugBaseAICharacteristic();

        int32 line = 0;
        char text[256];

        auto print = [&](const RwRGBA& color, const char* pszText)
        {
            if (line >= maxLines)
                return line;
            aLine[line].m_color = color;
            std::snprintf(aLine[line].m_szText, sizeof(aLine[line].m_szText), "%s", pszText);
            ++line;
        };

        const RwRGBA HEAD = { 0x78, 0xD8, 0xFF, 0xFF };
        const RwRGBA TEXT = { 0xE8, 0xEE, 0xF8, 0xFF };
        const RwRGBA DIM  = { 0xA0, 0xAC, 0xC0, 0xFF };
        const RwRGBA SEL  = { 0xFF, 0xD0, 0x60, 0xFF };

        char name[48];
        char status[32];
        FormatEnemyName(pSlot->m_id, name, sizeof(name));
        FormatEnemyStatus(pSlot->m_status, status, sizeof(status));
        std::snprintf(text, sizeof(text), "#%d %s   %s (%s) for %.1fs   HP %d/%d", pSlot->m_iLabel, name, status,
                      CEnemyAITrace::CategoryName(pSlot->m_category), CEnemyAITrace::Now() - pSlot->m_fStatusStart,
                      pEnemy->GetHP(), pEnemy->GetHPMax());
        print(SEL, text);

        if (!pSlot->m_bGeneralAI)
        {
            print(DIM, "This enemy does not use the General AI; only its status is shown.");
            return line;
        };

        if (pSlot->m_target >= 0)
            std::snprintf(text, sizeof(text), "Target P%d at %.1f   attack distance %.1f, front cone %.0f deg   %s",
                          pSlot->m_target + 1, pSlot->m_fTargetDist, rNow.m_fDistanceOfSuitable,
                          rNow.m_fRatioOfFrontView * 36.0f * 2.0f, (pSlot->m_bInRange ? "IN RANGE + FACING" : "not in range"));
        else
            std::snprintf(text, sizeof(text), "No target (player not seen, out of radius, or activity roll failed)");
        print(TEXT, text);

        std::snprintf(text, sizeof(text), "Think every %.3fs (%.2f since)%s", pSlot->m_fThinkInterval, pSlot->m_fSinceThink,
                      pSlot->m_bHasBackAway ?
                      (pSlot->m_fBackAwayCooldown > 0.0f ? "   back-away cooldown" : "   back-away READY") : "");
        if (pSlot->m_bHasBackAway && (pSlot->m_fBackAwayCooldown > 0.0f))
        {
            char cd[32];
            std::snprintf(cd, sizeof(cd), " %.1fs", pSlot->m_fBackAwayCooldown);
            strcat_s(text, sizeof(text), cd);
        };
        print(TEXT, text);

        std::snprintf(text, sizeof(text), "AI now (retail): activity %.0f%% (%.0f%%)  think %.0f%% (%.0f%%)  move %.0f%%  view %.1f/%.1f (%.1f/%.1f)",
                      rNow.m_fRatioOfActivity * 100.0f, rBase.m_fRatioOfActivity * 100.0f,
                      rNow.m_fThinkingFrequency * 100.0f, rBase.m_fThinkingFrequency * 100.0f,
                      rNow.m_fRatioOfMove * 100.0f,
                      rNow.m_fRatioOfFrontView, rNow.m_fRatioOfRearView,
                      rBase.m_fRatioOfFrontView, rBase.m_fRatioOfRearView);
        print(DIM, text);

        {
            const uint8* pBaseRow = nullptr;
            const uint8* pNowRow = nullptr;
            int32 count = 0;
            if (rChr.GetDebugFrequencyRows(&pBaseRow, &pNowRow, &count) && (count > 11))
            {
                std::snprintf(text, sizeof(text),
                              "Attack%% A %d(%d) AA %d(%d) AAA %d(%d) B %d(%d) C %d(%d) run %d(%d)   Guard%% vs A %d(%d) B %d(%d) shot %d(%d) jump %d(%d)",
                              pNowRow[0], pBaseRow[0], pNowRow[1], pBaseRow[1], pNowRow[2], pBaseRow[2],
                              pNowRow[3], pBaseRow[3], pNowRow[4], pBaseRow[4], pNowRow[5], pBaseRow[5],
                              pNowRow[6], pBaseRow[6], pNowRow[7], pBaseRow[7], pNowRow[8], pBaseRow[8],
                              pNowRow[9], pBaseRow[9]);
                print(DIM, text);
            };
        };

        // newest decision in full, then one line each for older ones
        for (int32 ago = 0; ago < CEnemyAITrace::DECISION_HISTORY; ++ago)
        {
            const CEnemyAITrace::DECISION* pDecision = CEnemyAITrace::GetDecision(*pSlot, ago);
            if (!pDecision)
                break;

            const char* pszType = CEnemyAITrace::TypeName(pDecision->m_order, pDecision->m_typeId);
            std::snprintf(text, sizeof(text), "%s %.1fs ago: %s%s%s%s   (%s; prev order ran %.1fs, %d wait checks, %d activity fails, %d no-target)",
                          (ago == 0 ? "LAST" : "    "),
                          CEnemyAITrace::Now() - pDecision->m_fTime,
                          CEnemyAITrace::OrderName(pDecision->m_order),
                          (pszType[0] ? " " : ""), pszType,
                          (pDecision->m_target >= 0 ? (pDecision->m_target == 0 ? " -> P1" : " -> P2+") : ""),
                          CEnemyAITrace::ReasonName(pDecision->m_reason), pDecision->m_fPrevOrderSec,
                          pDecision->m_nWaitChecks, pDecision->m_nWaitActivityFail, pDecision->m_nWaitNoTarget);
            print(ago == 0 ? HEAD : DIM, text);

            if (ago == 0)
            {
                for (int32 e = 0; e < pDecision->m_nEvent; ++e)
                {
                    char event[200];
                    CEnemyAITrace::FormatEvent(pDecision->m_aEvent[e], &rChr, event, sizeof(event));
                    std::snprintf(text, sizeof(text), "   %s", event);
                    print(TEXT, text);
                };
                if (pDecision->m_bEventOverflow)
                    print(DIM, "   (more checks than the panel stores)");
            };
        };

        return line;
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


    // ---- Combat Overhaul display: markers over enemies, player gauges, badge ----

    static void OverhaulFrame(IM2DBATCH& batch, float x0, float y0, float x1, float y1, float t, const RwRGBA& color)
    {
        Im2DQuad(batch, x0, y0, x1, y0 + t, color);
        Im2DQuad(batch, x0, y1 - t, x1, y1, color);
        Im2DQuad(batch, x0, y0, x0 + t, y1, color);
        Im2DQuad(batch, x1 - t, y0, x1, y1, color);
    };


    static void OverhaulBar(IM2DBATCH& batch, float x, float y, float w, float h, float value, const RwRGBA& fill)
    {
        const RwRGBA back = { 0x00, 0x00, 0x00, 0xB8 };
        const RwRGBA edge = { 0xFF, 0xFF, 0xFF, 0x70 };
        Im2DQuad(batch, x - 1.0f, y - 1.0f, x + w + 1.0f, y + h + 1.0f, back);
        if (value > 0.0f)
            Im2DQuad(batch, x, y, x + (w * MinF(value, 1.0f)), y + h, fill);
        OverhaulFrame(batch, x - 1.0f, y - 1.0f, x + w + 1.0f, y + h + 1.0f, 1.0f, edge);
    };


    void DrawOverhaulText(const char* pszText, float px, float py, float pixelHeight, const RwRGBA& color, bool bCenter) const
    {
        // Pixel position -> the font's virtual screen space.
        float sw = static_cast<float>(CScreen::Width());
        float sh = static_cast<float>(CScreen::Height());
        float height = pixelHeight * (CSprite::m_fVirtualScreenH / sh);
        float vx = CSprite::m_fVirtualScreenX + ((px / sw) * CSprite::m_fVirtualScreenW);
        float vy = CSprite::m_fVirtualScreenY + ((py / sh) * CSprite::m_fVirtualScreenH);
        if (bCenter)
            vx -= (CGameFont::GetStringWidth(pszText, height) * 0.5f);

        float shadow = MaxF(0.6f, height * 0.07f);
        CGameFont::SetHeight(height);
        CGameFont::SetRGBA(0x00, 0x00, 0x00, 0xE0);
        CGameFont::Show(pszText, vx + shadow, vy + shadow);
        CGameFont::SetRGBA(color.red, color.green, color.blue, color.alpha);
        CGameFont::Show(pszText, vx, vy);
    };


    void DrawOverhaulHud(void) const
    {
        RwCamera* pCamera = CCamera::CameraCurrent();
        if (!pCamera || !CGameStage::GetCurrent() || !CGameProperty::IsCameraViewAvailable())
            return;

        const float sw = static_cast<float>(CScreen::Width());
        const float sh = static_cast<float>(CScreen::Height());
        const float scale = MaxF(1.0f, sh / 480.0f);

        static IM2DBATCH s_batch;
        s_batch.m_nVertex = 0;
        s_batch.m_fZ = RwIm2DGetNearScreenZ();
        s_batch.m_fRhw = 1.0f / RwCameraGetNearClipPlane(pCamera);

        // ---- markers in the world ----
        int32 markerNum = CCombatOverhaul::GetMarkerNum();
        for (int32 i = 0; i < markerNum; ++i)
        {
            CCombatOverhaul::MARKERINFO marker;
            if (!CCombatOverhaul::GetMarker(i, marker))
                continue;

            float nx = 0.0f;
            float ny = 0.0f;
            if (!ProjectToNormalized(marker.m_vPos, nx, ny))
                continue;

            float x = nx * sw;
            float y = ny * sh;

            switch (marker.m_type)
            {
            case CCombatOverhaul::MARKER_ATTACK_LIGHT:
            case CCombatOverhaul::MARKER_ATTACK_HEAVY:
                {
                    // An exclamation mark that pops in large and settles.
                    bool bHeavy = (marker.m_type == CCombatOverhaul::MARKER_ATTACK_HEAVY);
                    float pop = (marker.m_fValue < 0.15f ? (1.6f - (marker.m_fValue * 4.0f)) : 1.0f);
                    float fade = (marker.m_fValue > 0.75f ? (1.0f - marker.m_fValue) * 4.0f : 1.0f);
                    uint8 alpha = static_cast<uint8>(255.0f * Clamp(fade, 0.0f, 1.0f));
                    RwRGBA color = { 0x30, 0xE8, 0xFF, alpha };
                    if (bHeavy)
                        color = { 0xFF, 0x30, 0x28, alpha };
                    RwRGBA outline = { 0x00, 0x00, 0x00, static_cast<uint8>(alpha * 0.8f) };

                    float w = (bHeavy ? 7.0f : 5.0f) * scale * pop;
                    float h = (bHeavy ? 24.0f : 18.0f) * scale * pop;
                    float gap = 3.0f * scale * pop;
                    float o = 1.5f * scale;
                    float top = y - h - gap - w;

                    Im2DQuad(s_batch, x - (w * 0.5f) - o, top - o, x + (w * 0.5f) + o, top + h + o, outline);
                    Im2DQuad(s_batch, x - (w * 0.5f) - o, y - w - o, x + (w * 0.5f) + o, y + o, outline);
                    Im2DQuad(s_batch, x - (w * 0.5f), top, x + (w * 0.5f), top + h, color);
                    Im2DQuad(s_batch, x - (w * 0.5f), y - w, x + (w * 0.5f), y, color);
                }
                break;

            case CCombatOverhaul::MARKER_LOCKON:
            case CCombatOverhaul::MARKER_SOFT_TARGET:
                {
                    // Four corner brackets around the target.
                    bool bLock = (marker.m_type == CCombatOverhaul::MARKER_LOCKON);
                    RwRGBA color = { 0xFF, 0xD8, 0x30, 0xF0 };
                    if (!bLock)
                        color = { 0xFF, 0xFF, 0xFF, 0x90 };

                    float r = (bLock ? 20.0f : 13.0f) * scale;
                    float l = (bLock ? 8.0f : 5.0f) * scale;
                    float t = (bLock ? 2.5f : 1.5f) * scale;
                    for (int32 c = 0; c < 4; ++c)
                    {
                        float sx = ((c & 1) ? 1.0f : -1.0f);
                        float sy = ((c & 2) ? 1.0f : -1.0f);
                        float cx = x + (sx * r);
                        float cy = y + (sy * r);
                        Im2DQuad(s_batch, MinF(cx, cx - (sx * l)), MinF(cy, cy - (sy * t)),
                                          MaxF(cx, cx - (sx * l)), MaxF(cy, cy - (sy * t)), color);
                        Im2DQuad(s_batch, MinF(cx, cx - (sx * t)), MinF(cy, cy - (sy * l)),
                                          MaxF(cx, cx - (sx * t)), MaxF(cy, cy - (sy * l)), color);
                    };
                }
                break;

            case CCombatOverhaul::MARKER_ENEMY_GAUGE:
                {
                    float w = 44.0f * scale;
                    float h = 4.0f * scale;
                    float top = y - (14.0f * scale);
                    if (marker.m_fValue > 0.0f)
                        OverhaulBar(s_batch, x - (w * 0.5f), top, w, h, marker.m_fValue, { 0xFF, 0xA0, 0x20, 0xF0 });
                    if (marker.m_fValue2 > 0.0f)
                        OverhaulBar(s_batch, x - (w * 0.5f), top + h + (3.0f * scale), w, h, marker.m_fValue2, { 0xC0, 0x60, 0xFF, 0xF0 });
                }
                break;

            default:
                break;
            };
        };

        // ---- player gauges (one block per player, above the retail gauges) ----
        struct HUDTEXT
        {
            char m_szText[48];
            float m_fX;
            float m_fY;
            RwRGBA m_color;
        };

        HUDTEXT aText[16];
        int32 nText = 0;

        const float barW = 110.0f * scale;
        const float barH = 6.0f * scale;
        const float textH = 11.0f * scale;
        const float blockY = sh * 0.705f;

        for (int32 no = 0; no < GAMETYPES::PLAYERS_MAX; ++no)
        {
            CCombatOverhaul::PLAYERHUD hud;
            if (!CCombatOverhaul::GetPlayerHud(no, hud))
                continue;

            float x = (sw * 0.035f) + (static_cast<float>(no) * sw * 0.24f);
            float y = blockY;

            if (hud.m_fPosture >= 0.0f)
            {
                RwRGBA fill = { 0xFF, 0xA0, 0x20, 0xF0 };
                if (hud.m_bPostureBroken)
                    fill = { 0xFF, 0x30, 0x28, 0xF0 };
                OverhaulBar(s_batch, x, y, barW, barH, hud.m_fPosture, fill);

                if (nText < COUNT_OF(aText))
                {
                    HUDTEXT& t = aText[nText++];
                    std::snprintf(t.m_szText, sizeof(t.m_szText), "%s",
                                  hud.m_bPostureBroken ? "POSTURE BROKEN - ATTACK!" : "POSTURE");
                    t.m_fX = x + barW + (6.0f * scale);
                    t.m_fY = y - (3.0f * scale);
                    t.m_color = fill;
                    t.m_color.alpha = 0xFF;
                };

                y += barH + (6.0f * scale);
            };

            if (hud.m_fBurst >= 0.0f)
            {
                RwRGBA fill = { 0xC0, 0x60, 0xFF, 0xF0 };
                if (hud.m_bBurstReady)
                    fill = { 0xFF, 0xFF, 0xFF, 0xFF };
                OverhaulBar(s_batch, x, y, barW, barH, hud.m_fBurst, fill);

                if (nText < COUNT_OF(aText))
                {
                    HUDTEXT& t = aText[nText++];
                    std::snprintf(t.m_szText, sizeof(t.m_szText), "%s",
                                  hud.m_bBurstReady ? "BREAK READY: GUARD + JUMP" : "COMBO BREAK");
                    t.m_fX = x + barW + (6.0f * scale);
                    t.m_fY = y - (3.0f * scale);
                    t.m_color = { 0xE0, 0xB0, 0xFF, 0xFF };
                };

                y += barH + (6.0f * scale);
            };

            if ((hud.m_iCrystal >= 0) && (nText < COUNT_OF(aText)))
            {
                static const char* s_apszCrystal[4] = { "RED: ATTACK UP", "GREEN: DEFENCE UP", "WHITE: FREE SHURIKEN", "ORANGE: INSTANT CHARGE" };
                static const RwRGBA s_aCrystalColor[4] =
                {
                    { 0xFF, 0x50, 0x40, 0xFF },
                    { 0x50, 0xF0, 0x70, 0xFF },
                    { 0xF4, 0xF4, 0xFF, 0xFF },
                    { 0xFF, 0xA0, 0x30, 0xFF },
                };

                int32 c = Clamp(hud.m_iCrystal, 0, 3);
                HUDTEXT& t = aText[nText++];
                std::snprintf(t.m_szText, sizeof(t.m_szText), "PARRY %s  %.1fs", s_apszCrystal[c], MaxF(0.0f, hud.m_fCrystalTime));
                t.m_fX = x;
                t.m_fY = y;
                t.m_color = s_aCrystalColor[c];
                y += textH + (2.0f * scale);
            };

            if (hud.m_bLockOn && (nText < COUNT_OF(aText)))
            {
                HUDTEXT& t = aText[nText++];
                std::snprintf(t.m_szText, sizeof(t.m_szText), "LOCK-ON");
                t.m_fX = x;
                t.m_fY = y;
                t.m_color = { 0xFF, 0xD8, 0x30, 0xFF };
            };
        };

        // ---- BEFORE / AFTER badge ----
        float badgeTime = CCombatOverhaul::GetBadgeTime();
        bool bBadge = (CCombatOverhaul::IsBadgeAlways() || (badgeTime < 2.5f));
        bool bAfter = CCombatOverhaul::IsEnabled();
        const char* pszBadge = (bAfter ? "AFTER - COMBAT OVERHAUL" : "BEFORE - RETAIL COMBAT");
        const float badgeH = 18.0f * scale;
        const float badgeY = sh * 0.12f;
        if (bBadge)
        {
            float halfW = 150.0f * scale;
            RwRGBA back = { 0x00, 0x00, 0x00, 0xB0 };
            RwRGBA edge = { 0x40, 0xE0, 0x70, 0xF0 };
            if (!bAfter)
                edge = { 0xC8, 0xC8, 0xD0, 0xF0 };
            Im2DQuad(s_batch, (sw * 0.5f) - halfW, badgeY - (5.0f * scale), (sw * 0.5f) + halfW, badgeY + badgeH + (7.0f * scale), back);
            OverhaulFrame(s_batch, (sw * 0.5f) - halfW, badgeY - (5.0f * scale), (sw * 0.5f) + halfW, badgeY + badgeH + (7.0f * scale), 2.0f * scale, edge);
        };

        if (s_batch.m_nVertex > 0)
        {
            CSystem2D::PushRenderState();
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
            RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, s_batch.m_aVertex, s_batch.m_nVertex);
            CSystem2D::PopRenderState();
        };

        if (((nText > 0) || bBadge) && CGameFont::IsOpen())
        {
            CSystem2D::PushRenderState();

            for (int32 i = 0; i < nText; ++i)
                DrawOverhaulText(aText[i].m_szText, aText[i].m_fX, aText[i].m_fY, textH, aText[i].m_color, false);

            if (bBadge)
            {
                RwRGBA color = { 0x60, 0xFF, 0x90, 0xFF };
                if (!bAfter)
                    color = { 0xE8, 0xE8, 0xF0, 0xFF };
                DrawOverhaulText(pszBadge, sw * 0.5f, badgeY, badgeH, color, true);
            };

            CSystem2D::PopRenderState();
        };
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
        if (m_bOpen)
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
    uint32 m_uNavHeld;
    int32 m_iNavFrames;
    int32 m_iMenuSize;
    int32 m_iMenuStyle;
    mutable UIACTION m_aUIAction[UIACTION_MAX];
    mutable int32 m_nUIAction;
    mutable int32 m_iUILastSelection;
    mutable int32 m_iUILastPage;
    mutable int32 m_aiScroll[PAGE_NUM];
    int32 m_iAISelected;
    float m_fAIRadarRange;
    float m_fAITimelineSpan;
    bool m_bAITimeline;
    bool m_bAIDecisionPanel;
    int32 m_iAIZones;
    bool m_bAIZoneLabels;
    mutable int32 m_iLastMenuBottom;
    mutable int32 m_iLastMenuRight;
    mutable bool m_bImGuiMenuShown;
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
