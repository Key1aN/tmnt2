#include "CombatLab.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/Component/Enemy/Enemy.hpp"
#include "Game/Component/Enemy/EnemyCharacter.hpp"
#include "Game/Component/Enemy/CharacterCompositor.hpp"
#include "Game/Component/GameData/GameData.hpp"
#include "Game/Component/GameMain/DebugDifficulty.hpp"
#include "Game/Component/GameMain/GamePlayer.hpp"
#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/GameMain/GameStage.hpp"
#include "Game/Component/Player/Player.hpp"
#include "Game/Component/Player/PlayerCharacter.hpp"
#include "Game/System/Character/Character.hpp"
#include "Game/System/GameObject/GameObject.hpp"
#include "Game/System/GameObject/GameObjectManager.hpp"
#include "Game/System/Hit/HitAttackData.hpp"
#include "Game/System/Map/WorldMap.hpp"
#include "System/Common/Configure.hpp"
#include "System/PC/PCCrashReporter.hpp"
#include "System/PC/PCFrameStats.hpp"
#include "System/PC/PCModFeatures.hpp"

#ifdef GetObject
#undef GetObject
#endif /* GetObject */

#include <cstdio>
#include <cstring>


namespace
{
    //
    //  Every arena below is the retail Battle Nexus tournament map (M60X).
    //  The rosters were read from the retail US TMNT.DAT stage LPACs
    //  (AFS entries 90, 114, 110): only enemies whose model, motion set and
    //  parameters are packed in that stage can be spawned there.
    //
    static const CCombatLab::ARENA s_aArena[] =
    {
        { "Nexus Arena - Foot Clan",     STAGEID::ID_ST60X_A01, { ENEMYID::ID_FOOT_NINJA_SWORD, ENEMYID::ID_FOOT_NINJA_STAFF, ENEMYID::ID_FOOT_NINJA_ARCHER } },
        { "Nexus Arena - Foot Elite",    STAGEID::ID_ST60X_C05, { ENEMYID::ID_FOOT_TECH_NINJA,  ENEMYID::ID_FOOT_SUMO_NINJA,  ENEMYID::ID_DUMMY } },
        { "Nexus Arena - Foot Tech/Mech",STAGEID::ID_ST60X_C01, { ENEMYID::ID_FOOT_TECH_NINJA,  ENEMYID::ID_FOOT_GUNNER_FOOT_MECH, ENEMYID::ID_DUMMY } },
    };


    static const uint32 SCENARIO_SEED       = 0x20041019;
    static const float  SPAWN_DISTANCE      = 4.5f;
    static const float  SPAWN_SPREAD_RAD    = 0.55f;
    static const float  SPAWN_TERRITORY     = 100.0f;
    static const int32  RESET_RETRY_FRAMES  = 30;

    // default scenario
    static const int32           DEFAULT_ARENA       = 0;
    static const PLAYERID::VALUE DEFAULT_PLAYER      = PLAYERID::ID_LEO;
    static const int32           DEFAULT_ENEMY_COUNT = 3;
    static const int32           DEFAULT_CRYSTAL_LEVEL = 3;


    struct LABSTATE
    {
        // flow
        bool m_bLaunchChecked;
        bool m_bLaunchRequest;
        bool m_bEnterRequest;
        bool m_bRestartRequest;
        bool m_bExitRequest;
        bool m_bActive;
        bool m_bPlaying;
        CGameStage* m_pStage;

        // config
        int32 m_iArenaSelected;
        int32 m_iArenaLoaded;
        PLAYERID::VALUE m_idPlayerSelected;
        PLAYERID::VALUE m_idPlayerLoaded;
        int32 m_iEnemyType;
        int32 m_iEnemyCount;
        bool m_bInfinitePlayerHP;
        bool m_bInfiniteEnemyHP;
        bool m_bResetClearsStats;
        bool m_bLog;
        bool m_bOverlay;
        CCombatLab::RULESET m_ruleset;
        CCombatLab::AIMODE m_aimode;
        int32 m_iCrystalLevel;

        // scenario
        bool m_bHaveHome;
        RwV3d m_vHome;
        float m_fHomeDir;
        uint32 m_ahEnemy[GAMETYPES::ENEMY_MAX];
        int32 m_nEnemy;
        int32 m_iPendingResetFrames;
        bool m_bPlayerNegation;
        CGameStage* m_pNegationStage;

        // restored on exit
        GAMETYPES::GAMEMODE m_savedGamemode;
        GAMETYPES::DIFFICULTY m_savedDifficulty;
        CDebugDifficulty::SNAPSHOT m_savedDebugDifficulty;
        bool m_bSaved;

        CCombatLab::STATS m_stats;
    };


    static LABSTATE s_lab =
    {
        false, false, false, false, false, false, false, nullptr,
        DEFAULT_ARENA, DEFAULT_ARENA, DEFAULT_PLAYER, DEFAULT_PLAYER,
        0, DEFAULT_ENEMY_COUNT,
        true, true, true, true, true,
        CCombatLab::RULESET_VANILLA, CCombatLab::AIMODE_ORIGINAL,
        DEFAULT_CRYSTAL_LEVEL,
    };


    // Stage time is only readable while a stage exists; the exit summary is
    // written after the stage is gone, so keep the last value seen.
    static float s_fLabLastTime = 0.0f;

    static float LabNow(void)
    {
        if (CGameStage::GetCurrent())
            s_fLabLastTime = CGameProperty::GetTotalElapsedTime();
        return s_fLabLastTime;
    };


    static CPCDebugLogFile& LabLog(void)
    {
        static CPCDebugLogFile s_log("TMNT2_combatlab.log");
        return s_log;
    };


    static int32 ArenaEnemyNum(int32 arena)
    {
        int32 num = 0;
        for (int32 i = 0; i < CCombatLab::ARENA_ENEMY_MAX; ++i)
        {
            if (s_aArena[arena].m_aEnemy[i] == ENEMYID::ID_DUMMY)
                break;
            ++num;
        };
        return num;
    };


    static CEnemy* GetLabEnemy(uint32 hEnemy)
    {
        CGameObject* pObject = CGameObjectManager::GetObject(hEnemy);
        if (!pObject || (pObject->GetType() != GAMEOBJECTTYPE::ENEMY))
            return nullptr;
        return static_cast<CEnemy*>(pObject);
    };


    static void CompactEnemyList(void)
    {
        int32 out = 0;
        for (int32 i = 0; i < s_lab.m_nEnemy; ++i)
        {
            if (GetLabEnemy(s_lab.m_ahEnemy[i]))
                s_lab.m_ahEnemy[out++] = s_lab.m_ahEnemy[i];
        };
        s_lab.m_nEnemy = out;
    };


    static void ApplyPassiveAI(bool bPassive)
    {
        // Same engine calls as the existing Debug Tools "Freeze enemy AI" tool,
        // limited to the enemies the lab spawned.
        for (int32 i = 0; i < s_lab.m_nEnemy; ++i)
        {
            CEnemy* pEnemy = GetLabEnemy(s_lab.m_ahEnemy[i]);
            if (!pEnemy)
                continue;

            bool bRunning = pEnemy->Character().IsRunningAI();
            if (bPassive && bRunning)
                pEnemy->StopAI();
            else if (!bPassive && !bRunning)
                pEnemy->StartAI();
        };
    };


    static void ApplyAIModeDifficulty(CCombatLab::AIMODE mode)
    {
        switch (mode)
        {
        case CCombatLab::AIMODE_ORIGINAL:
        case CCombatLab::AIMODE_PASSIVE:
            // Lab forces the option difficulty to Normal and runs in Nexus
            // game mode, so PRESET_GAME is the unmodified retail Normal row.
            CDebugDifficulty::SetPreset(CDebugDifficulty::PRESET_GAME, GAMETYPES::DIFFICULTY_NORMAL);
            break;

        case CCombatLab::AIMODE_CUSTOM:
            // Keep whatever the Difficulty tab currently holds.
            break;

        case CCombatLab::AIMODE_AGGRESSIVE:
            {
                // Only existing Difficulty-tab parameters; HP and damage stay 1.0.
                GAMETYPES::DIFFICULTY normal = GAMETYPES::DIFFICULTY_NORMAL;
                CDebugDifficulty::SetPreset(CDebugDifficulty::PRESET_VANILLA, normal);
                CDebugDifficulty::SetCustomValue(CDebugDifficulty::SETTING_AI_THINKING,      1.75f, normal);
                CDebugDifficulty::SetCustomValue(CDebugDifficulty::SETTING_AI_ACTIVITY,      1.50f, normal);
                CDebugDifficulty::SetCustomValue(CDebugDifficulty::SETTING_AI_AWARENESS,     1.50f, normal);
                CDebugDifficulty::SetCustomValue(CDebugDifficulty::SETTING_ATTACK_FREQUENCY, 1.50f, normal);
                CDebugDifficulty::SetCustomValue(CDebugDifficulty::SETTING_ATTACK_INTERVAL,  0.50f, normal);
            }
            break;

        default:
            break;
        };
    };


    static void ApplyPlayerNegation(void)
    {
        bool bWant = (s_lab.m_bPlaying && s_lab.m_bInfinitePlayerHP);
        CGameStage* pStage = CGameStage::GetCurrent();

        // CGamePlayerContainer clears the negation counter for every stage, so
        // the lab owns exactly one reference tied to the stage that received it.
        if (s_lab.m_bPlayerNegation && (s_lab.m_pNegationStage != pStage))
        {
            s_lab.m_bPlayerNegation = false;
            s_lab.m_pNegationStage = nullptr;
        };

        if (bWant && !s_lab.m_bPlayerNegation && pStage)
        {
            CGamePlayer::SetDamageNegation(true);
            s_lab.m_bPlayerNegation = true;
            s_lab.m_pNegationStage = pStage;
        }
        else if (!bWant && s_lab.m_bPlayerNegation)
        {
            CGamePlayer::SetDamageNegation(false);
            s_lab.m_bPlayerNegation = false;
            s_lab.m_pNegationStage = nullptr;
        };
    };


    static void ApplyCrystalLevel(void)
    {
        if (!s_lab.m_bSaved)
            return;

        if (s_lab.m_iCrystalLevel < 0)
        {
            // Back to the exact pre-lab record (the backup stays valid).
            CGameData::PopRecord();
        }
        else
        {
            int32 num = s_lab.m_iCrystalLevel * 10;
            CGameData::Record().Item().ForceSetCrystalNum(GAMETYPES::CRYSTALTYPE_RED,    num);
            CGameData::Record().Item().ForceSetCrystalNum(GAMETYPES::CRYSTALTYPE_GREEN,  num);
            CGameData::Record().Item().ForceSetCrystalNum(GAMETYPES::CRYSTALTYPE_WHITE,  num);
            CGameData::Record().Item().ForceSetCrystalNum(GAMETYPES::CRYSTALTYPE_ORANGE, num);
        };
    };


    static void FormatEnemyLabel(CEnemyCharacter& rEnemyChr, char* buffer, size_t capacity)
    {
        const char* pszName = ENEMYID::GetName(rEnemyChr.GetID());
        std::snprintf(buffer, capacity, "%s", (pszName && pszName[0]) ? pszName : "enemy");
    };


    static void FinishRealReset(void)
    {
        IGamePlayer* pPlayer = (CGameProperty::GetPlayerNum() > 0 ? CGameProperty::Player(0) : nullptr);
        if (pPlayer && pPlayer->IsAlive())
        {
            if (s_lab.m_bHaveHome)
            {
                pPlayer->Relocation(&s_lab.m_vHome, s_lab.m_fHomeDir, false);

                // Relocation grants 3 s of respawn invincibility; the lab wants
                // the player hittable immediately so every repetition matches.
                CPlayerCharacter* pChr = pPlayer->GetCurrentCharacter();
                if (pChr)
                {
                    pChr->SetInvincibilityEnable(false, 0.0f);
                    pChr->SetBlinkEnable(false, 0.0f);
                };
            };

            pPlayer->AddHP(pPlayer->GetHPMax());
            pPlayer->AddShurikenNum(pPlayer->GetShurikenMax());
        };

        if (s_lab.m_bResetClearsStats)
            CCombatLab::ResetStats();

        Math::SRand(SCENARIO_SEED);
        CCombatLab::SpawnEnemies();

        if (s_lab.m_bLog)
        {
            LabLog().Write("RESET  arena=%s player=%s enemies=%d type=%s ruleset=%s ai=%s seed=0x%08X",
                           s_aArena[s_lab.m_iArenaLoaded].m_pszName,
                           PLAYERID::GetName(s_lab.m_idPlayerLoaded),
                           s_lab.m_iEnemyCount,
                           ENEMYID::GetName(CCombatLab::GetEnemyType()),
                           CCombatLab::GetRulesetName(s_lab.m_ruleset),
                           CCombatLab::GetAIModeName(s_lab.m_aimode),
                           SCENARIO_SEED);
            LabLog().Flush();
        };
    };
};


/*static*/ const float CCombatLab::CHAIN_GAP_SEC = 1.0f;


/*static*/ void CCombatLab::InitializeLaunch(void)
{
    if (s_lab.m_bLaunchChecked)
        return;

    s_lab.m_bLaunchChecked = true;

    // "-combatlab" (engine argument style) or "--combat-lab".
    bool bArg = (CConfigure::CheckArg("combatlab") || CConfigure::CheckArg("-combat-lab"));
    if (!bArg)
        return;

    if (!CPCModFeatures::IsDebugToolsEnabled())
    {
        CPCCrashReporter::Breadcrumb("COMBATLAB launch ignored: debug-tools-f4 feature is not enabled");
        return;
    };

    s_lab.m_bLaunchRequest = true;
    CPCCrashReporter::Breadcrumb("COMBATLAB launch requested by command line");
};


/*static*/ bool CCombatLab::ConsumeLaunchRequest(void)
{
    bool bResult = s_lab.m_bLaunchRequest;
    s_lab.m_bLaunchRequest = false;
    return bResult;
};


/*static*/ void CCombatLab::RequestEnterFromGameplay(void)
{
    s_lab.m_bEnterRequest = true;
};


/*static*/ bool CCombatLab::ConsumeEnterRequest(void)
{
    bool bResult = s_lab.m_bEnterRequest;
    s_lab.m_bEnterRequest = false;
    return bResult;
};


/*static*/ void CCombatLab::RequestRestart(void)
{
    s_lab.m_bRestartRequest = true;
    s_lab.m_bExitRequest = true;
};


/*static*/ bool CCombatLab::ConsumeRestartRequest(void)
{
    bool bResult = s_lab.m_bRestartRequest;
    s_lab.m_bRestartRequest = false;
    return bResult;
};


/*static*/ void CCombatLab::RequestExit(void)
{
    s_lab.m_bExitRequest = true;
};


/*static*/ bool CCombatLab::IsExitRequested(void)
{
    return s_lab.m_bExitRequest;
};


/*static*/ void CCombatLab::OnSequenceAttach(void)
{
    s_lab.m_bActive = true;
    s_lab.m_bPlaying = false;
    s_lab.m_bExitRequest = false;
    s_lab.m_bRestartRequest = false;
    s_lab.m_bHaveHome = false;
    s_lab.m_nEnemy = 0;
    s_lab.m_iPendingResetFrames = 0;
    s_lab.m_bPlayerNegation = false;
    s_lab.m_pNegationStage = nullptr;
    s_lab.m_iArenaLoaded = s_lab.m_iArenaSelected;
    s_lab.m_idPlayerLoaded = s_lab.m_idPlayerSelected;
    if (s_lab.m_iEnemyType >= ArenaEnemyNum(s_lab.m_iArenaLoaded))
        s_lab.m_iEnemyType = 0;

    // Everything the lab changes globally is captured here and put back on
    // detach: the campaign record (stage-played / secret checks), the game
    // mode, the option difficulty and the Debug Tools difficulty override.
    CGameData::PushRecord();
    s_lab.m_savedGamemode = CGameData::Attribute().GetGamemode();
    s_lab.m_savedDifficulty = CGameData::Option().Play().GetDifficulty();
    CDebugDifficulty::Snapshot(s_lab.m_savedDebugDifficulty);
    s_lab.m_bSaved = true;

    // Nexus game mode is what the retail arena expects: table difficulty is
    // forced to Normal and save-file secrets/cheats are ignored, so the
    // baseline does not depend on the loaded save.
    CGameData::Attribute().SetGamemode(GAMETYPES::GAMEMODE_NEXUS);
    CGameData::Option().Play().SetDifficulty(GAMETYPES::DIFFICULTY_NORMAL);
    ApplyCrystalLevel();

    ApplyAIModeDifficulty(s_lab.m_aimode);

    const ARENA& arena = s_aArena[s_lab.m_iArenaLoaded];
    CGameData::PlayParam().Clear();
    CGameData::PlayParam().AddPlayerCharacter(0, s_lab.m_idPlayerLoaded);
    CGameData::PlayParam().SetStage(arena.m_idStage);
    CGameData::PlayParam().InitForArea();

    std::memset(&s_lab.m_stats, 0x00, sizeof(s_lab.m_stats));

    CPCCrashReporter::Breadcrumb("COMBATLAB attach arena=%s stage=%d player=%s",
                                 arena.m_pszName,
                                 static_cast<int32>(arena.m_idStage),
                                 PLAYERID::GetName(s_lab.m_idPlayerLoaded));

    if (s_lab.m_bLog)
    {
        LabLog().Write("SESSION  Combat Lab v1.1.0 arena=%s player=%s crystals=%d ruleset=%s ai=%s",
                       arena.m_pszName,
                       PLAYERID::GetName(s_lab.m_idPlayerLoaded),
                       s_lab.m_iCrystalLevel,
                       GetRulesetName(s_lab.m_ruleset),
                       GetAIModeName(s_lab.m_aimode));
        LabLog().Flush();
    };
};


/*static*/ void CCombatLab::OnSequenceDetach(void)
{
    OnPlayEnd();

    if (s_lab.m_bSaved)
    {
        CDebugDifficulty::Restore(s_lab.m_savedDebugDifficulty);
        CGameData::Option().Play().SetDifficulty(s_lab.m_savedDifficulty);
        CGameData::Attribute().SetGamemode(s_lab.m_savedGamemode);
        CGameData::PopRecord();
        s_lab.m_bSaved = false;
    };

    if (s_lab.m_bLog)
    {
        WriteSummaryToLog("exit");
        LabLog().Flush();
    };

    s_lab.m_bActive = false;
    s_lab.m_bExitRequest = false;
    CPCCrashReporter::Breadcrumb("COMBATLAB detach (record, game mode, difficulty and AI override restored)");
};


/*static*/ void CCombatLab::OnPlayStart(CGameStage* pStage)
{
    s_lab.m_bPlaying = true;
    s_lab.m_pStage = pStage;

    IGamePlayer* pPlayer = (CGameProperty::GetPlayerNum() > 0 ? CGameProperty::Player(0) : nullptr);
    if (pPlayer && pPlayer->IsAlive())
    {
        // Home = the retail player start gimmick of the arena.
        pPlayer->GetPosition(&s_lab.m_vHome);
        s_lab.m_fHomeDir = pPlayer->GetRotY();
        s_lab.m_bHaveHome = true;
    };

    ApplyPlayerNegation();
    ResetStats();
    Math::SRand(SCENARIO_SEED);
    SpawnEnemies();
};


/*static*/ void CCombatLab::OnPlayEnd(void)
{
    if (!s_lab.m_bPlaying)
        return;

    s_lab.m_bPlaying = false;
    ApplyPlayerNegation();
    s_lab.m_pStage = nullptr;
    s_lab.m_nEnemy = 0;
    s_lab.m_iPendingResetFrames = 0;
};


/*static*/ void CCombatLab::Period(void)
{
    if (!s_lab.m_bPlaying)
        return;

    LabNow();
    ApplyPlayerNegation();
    CompactEnemyList();

    if (s_lab.m_aimode == AIMODE_PASSIVE)
        ApplyPassiveAI(true);

    if (s_lab.m_iPendingResetFrames > 0)
    {
        // An enemy held in a throw cannot be removed; wait until the player
        // relocation released it, then finish the reset.
        ClearEnemies();
        if ((s_lab.m_nEnemy == 0) || (--s_lab.m_iPendingResetFrames == 0))
        {
            s_lab.m_iPendingResetFrames = 0;
            FinishRealReset();
        };
    };

    if (s_lab.m_bLog)
        LabLog().FlushIfDue();
};


/*static*/ bool CCombatLab::IsActive(void)
{
    return s_lab.m_bActive;
};


/*static*/ bool CCombatLab::IsPlaying(void)
{
    return s_lab.m_bPlaying;
};


/*static*/ int32 CCombatLab::GetArenaNum(void)
{
    return COUNT_OF(s_aArena);
};


/*static*/ const CCombatLab::ARENA& CCombatLab::GetArena(int32 index)
{
    ASSERT(index >= 0);
    ASSERT(index < COUNT_OF(s_aArena));
    return s_aArena[index];
};


/*static*/ int32 CCombatLab::GetSelectedArena(void)
{
    return s_lab.m_iArenaSelected;
};


/*static*/ int32 CCombatLab::GetLoadedArena(void)
{
    return s_lab.m_iArenaLoaded;
};


/*static*/ void CCombatLab::SelectArena(int32 index)
{
    s_lab.m_iArenaSelected = InvClamp(index, 0, COUNT_OF(s_aArena) - 1);
    if (!s_lab.m_bActive)
    {
        s_lab.m_iArenaLoaded = s_lab.m_iArenaSelected;
        if (s_lab.m_iEnemyType >= ArenaEnemyNum(s_lab.m_iArenaLoaded))
            s_lab.m_iEnemyType = 0;
    };
};


/*static*/ PLAYERID::VALUE CCombatLab::GetSelectedPlayer(void)
{
    return s_lab.m_idPlayerSelected;
};


/*static*/ PLAYERID::VALUE CCombatLab::GetLoadedPlayer(void)
{
    return s_lab.m_idPlayerLoaded;
};


/*static*/ void CCombatLab::SelectPlayer(PLAYERID::VALUE idPlayer)
{
    s_lab.m_idPlayerSelected = static_cast<PLAYERID::VALUE>(
        InvClamp(static_cast<int32>(idPlayer), static_cast<int32>(PLAYERID::ID_START), static_cast<int32>(PLAYERID::ID_MAX) - 1)
    );
    if (!s_lab.m_bActive)
        s_lab.m_idPlayerLoaded = s_lab.m_idPlayerSelected;
};


/*static*/ bool CCombatLab::IsReloadPending(void)
{
    return s_lab.m_bActive &&
           ((s_lab.m_iArenaSelected != s_lab.m_iArenaLoaded) ||
            (s_lab.m_idPlayerSelected != s_lab.m_idPlayerLoaded));
};


/*static*/ int32 CCombatLab::GetCrystalLevel(void)
{
    return s_lab.m_iCrystalLevel;
};


/*static*/ void CCombatLab::SetCrystalLevel(int32 level)
{
    s_lab.m_iCrystalLevel = Clamp(level, -1, 3);
    ApplyCrystalLevel();
};


/*static*/ int32 CCombatLab::GetEnemyTypeIndex(void)
{
    return s_lab.m_iEnemyType;
};


/*static*/ int32 CCombatLab::GetEnemyTypeNum(void)
{
    return ArenaEnemyNum(s_lab.m_iArenaLoaded);
};


/*static*/ ENEMYID::VALUE CCombatLab::GetEnemyType(void)
{
    int32 num = ArenaEnemyNum(s_lab.m_iArenaLoaded);
    if (num <= 0)
        return ENEMYID::ID_DUMMY;
    return s_aArena[s_lab.m_iArenaLoaded].m_aEnemy[Clamp(s_lab.m_iEnemyType, 0, num - 1)];
};


/*static*/ void CCombatLab::SelectEnemyType(int32 index)
{
    int32 num = ArenaEnemyNum(s_lab.m_iArenaLoaded);
    s_lab.m_iEnemyType = (num > 0 ? InvClamp(index, 0, num - 1) : 0);
};


/*static*/ int32 CCombatLab::GetEnemyCount(void)
{
    return s_lab.m_iEnemyCount;
};


/*static*/ void CCombatLab::SetEnemyCount(int32 count)
{
    s_lab.m_iEnemyCount = Clamp(count, 1, static_cast<int32>(SPAWN_MAX));
};


/*static*/ bool CCombatLab::IsInfinitePlayerHP(void)
{
    return s_lab.m_bInfinitePlayerHP;
};


/*static*/ void CCombatLab::SetInfinitePlayerHP(bool bEnable)
{
    s_lab.m_bInfinitePlayerHP = bEnable;
    ApplyPlayerNegation();
};


/*static*/ bool CCombatLab::IsInfiniteEnemyHP(void)
{
    return s_lab.m_bInfiniteEnemyHP;
};


/*static*/ void CCombatLab::SetInfiniteEnemyHP(bool bEnable)
{
    s_lab.m_bInfiniteEnemyHP = bEnable;
};


/*static*/ bool CCombatLab::IsResetClearsStats(void)
{
    return s_lab.m_bResetClearsStats;
};


/*static*/ void CCombatLab::SetResetClearsStats(bool bEnable)
{
    s_lab.m_bResetClearsStats = bEnable;
};


/*static*/ CCombatLab::RULESET CCombatLab::GetRuleset(void)
{
    return s_lab.m_ruleset;
};


/*static*/ void CCombatLab::SetRuleset(RULESET ruleset)
{
    s_lab.m_ruleset = static_cast<RULESET>(Clamp(static_cast<int32>(ruleset), 0, static_cast<int32>(RULESET_NUM) - 1));
    if (s_lab.m_bLog && s_lab.m_bActive)
        LabLog().Write("RULESET  %s", GetRulesetName(s_lab.m_ruleset));
};


/*static*/ bool CCombatLab::IsExperimentalCombat(void)
{
    // Future experimental features must test this AND their own switch, so
    // Vanilla always runs the untouched retail code path.
    return (s_lab.m_bActive && (s_lab.m_ruleset == RULESET_EXPERIMENTAL));
};


/*static*/ const char* CCombatLab::GetRulesetName(RULESET ruleset)
{
    switch (ruleset)
    {
    case RULESET_VANILLA:       return "Vanilla";
    case RULESET_EXPERIMENTAL:  return "Experimental";
    default:                    return "Unknown";
    };
};


/*static*/ CCombatLab::AIMODE CCombatLab::GetAIMode(void)
{
    // Editing any Difficulty-tab value while "Original" is selected means the
    // enemies no longer run the retail table: report that honestly.
    if (s_lab.m_bActive && (s_lab.m_aimode == AIMODE_ORIGINAL) && CDebugDifficulty::IsOverrideEnabled())
        s_lab.m_aimode = AIMODE_CUSTOM;

    return s_lab.m_aimode;
};


/*static*/ void CCombatLab::SetAIModeValue(AIMODE mode)
{
    AIMODE prev = s_lab.m_aimode;
    s_lab.m_aimode = static_cast<AIMODE>(Clamp(static_cast<int32>(mode), 0, static_cast<int32>(AIMODE_NUM) - 1));

    if (!s_lab.m_bActive)
        return;

    if ((prev == AIMODE_PASSIVE) && (s_lab.m_aimode != AIMODE_PASSIVE))
        ApplyPassiveAI(false);
    else if (s_lab.m_aimode == AIMODE_PASSIVE)
        ApplyPassiveAI(true);

    ApplyAIModeDifficulty(s_lab.m_aimode);

    if (s_lab.m_bLog)
        LabLog().Write("AIMODE  %s", GetAIModeName(s_lab.m_aimode));
};


/*static*/ const char* CCombatLab::GetAIModeName(AIMODE mode)
{
    switch (mode)
    {
    case AIMODE_ORIGINAL:   return "Original (Normal)";
    case AIMODE_PASSIVE:    return "Passive dummy";
    case AIMODE_CUSTOM:     return "Custom (Difficulty tab)";
    case AIMODE_AGGRESSIVE: return "Aggressive preset";
    default:                return "Unknown";
    };
};


/*static*/ void CCombatLab::RestoreDefaults(void)
{
    SelectArena(DEFAULT_ARENA);
    SelectPlayer(DEFAULT_PLAYER);
    s_lab.m_iEnemyType = 0;
    s_lab.m_iEnemyCount = DEFAULT_ENEMY_COUNT;
    s_lab.m_bResetClearsStats = true;
    s_lab.m_bOverlay = true;
    SetCrystalLevel(DEFAULT_CRYSTAL_LEVEL);
    SetInfinitePlayerHP(true);
    SetInfiniteEnemyHP(true);
    SetAIModeValue(AIMODE_ORIGINAL);
    // The ruleset is deliberately kept: switching Vanilla/Experimental is an
    // explicit user choice and must survive scenario resets and defaults.
};


/*static*/ int32 CCombatLab::SpawnEnemies(void)
{
    if (!s_lab.m_bPlaying)
        return 0;

    CompactEnemyList();

    ENEMYID::VALUE idEnemy = GetEnemyType();
    if (idEnemy == ENEMYID::ID_DUMMY)
        return 0;

    RwV3d vOrigin = s_lab.m_vHome;
    float fBaseDir = s_lab.m_fHomeDir;
    if (!s_lab.m_bHaveHome)
    {
        vOrigin = Math::VECTOR3_ZERO;
        fBaseDir = 0.0f;
    };

    int32 count = s_lab.m_iEnemyCount;
    int32 spawned = 0;
    for (int32 i = 0; i < count; ++i)
    {
        if (s_lab.m_nEnemy >= SPAWN_MAX)
            break;

        // Deterministic arc in front of the player start, every enemy facing it.
        float fOffset = (static_cast<float>(i) - (static_cast<float>(count - 1) * 0.5f)) * SPAWN_SPREAD_RAD;
        float fDir = fBaseDir + fOffset;

        RwV3d vPos = vOrigin;
        vPos.x += Math::Sin(fDir) * SPAWN_DISTANCE;
        vPos.z += Math::Cos(fDir) * SPAWN_DISTANCE;
        vPos.y = CWorldMap::GetMapHeight(&vPos);

        ENEMYTYPES::CREATEINFO createInfo;
        std::memset(&createInfo, 0x00, sizeof(createInfo));
        createInfo.m_idEnemy    = idEnemy;
        createInfo.m_vPosition  = vPos;
        createInfo.m_fDirection = Math::ATan2(vOrigin.x - vPos.x, vOrigin.z - vPos.z);
        createInfo.m_status     = ENEMYTYPES::STATUS_IDLE;
        createInfo.m_iPattern   = 0;
        createInfo.m_fRadius    = SPAWN_TERRITORY;

        CEnemy* pEnemy = CEnemy::New(&createInfo);
        if (!pEnemy)
            break;

        pEnemy->Start();
        s_lab.m_ahEnemy[s_lab.m_nEnemy++] = pEnemy->GetHandle();
        ++spawned;
    };

    if (s_lab.m_aimode == AIMODE_PASSIVE)
        ApplyPassiveAI(true);

    if (s_lab.m_bLog)
        LabLog().Write("SPAWN  type=%s requested=%d spawned=%d alive=%d", ENEMYID::GetName(idEnemy), count, spawned, s_lab.m_nEnemy);

    return spawned;
};


/*static*/ void CCombatLab::ClearEnemies(void)
{
    for (int32 i = 0; i < s_lab.m_nEnemy; ++i)
    {
        CEnemy* pEnemy = GetLabEnemy(s_lab.m_ahEnemy[i]);
        if (pEnemy)
            pEnemy->Kill(); // same immediate removal as Debug Tools "Kill all"; refuses caught/lifted enemies
    };

    CompactEnemyList();
};


/*static*/ void CCombatLab::ResetScenario(void)
{
    if (!s_lab.m_bPlaying)
        return;

    // Player first: Relocation forces STATUS_IDLE, which releases a thrown enemy.
    IGamePlayer* pPlayer = (CGameProperty::GetPlayerNum() > 0 ? CGameProperty::Player(0) : nullptr);
    if (pPlayer && pPlayer->IsAlive() && s_lab.m_bHaveHome)
        pPlayer->Relocation(&s_lab.m_vHome, s_lab.m_fHomeDir, false);

    ClearEnemies();
    if (s_lab.m_nEnemy > 0)
    {
        s_lab.m_iPendingResetFrames = RESET_RETRY_FRAMES;
        return;
    };

    FinishRealReset();
};


/*static*/ int32 CCombatLab::GetSpawnedAliveNum(void)
{
    CompactEnemyList();
    return s_lab.m_nEnemy;
};


/*static*/ uint32 CCombatLab::GetScenarioSeed(void)
{
    return SCENARIO_SEED;
};


/*static*/ const CCombatLab::STATS& CCombatLab::Stats(void)
{
    return s_lab.m_stats;
};


/*static*/ void CCombatLab::ResetStats(void)
{
    std::memset(&s_lab.m_stats, 0x00, sizeof(s_lab.m_stats));
    s_lab.m_stats.m_fStartTime = LabNow();
    s_lab.m_stats.m_fLastPlayerHitTime = -1000.0f;
};


/*static*/ bool CCombatLab::IsLogEnabled(void)
{
    return s_lab.m_bLog;
};


/*static*/ void CCombatLab::SetLogEnabled(bool bEnable)
{
    s_lab.m_bLog = bEnable;
    if (!bEnable)
        LabLog().Flush();
};


/*static*/ void CCombatLab::WriteSummaryToLog(const char* pszReason)
{
    const STATS& s = s_lab.m_stats;
    float span = LabNow() - s.m_fStartTime;

    LabLog().Write("SUMMARY  reason=%s arena=%s player=%s ruleset=%s ai=%s span=%.1fs "
                   "hits=%d guarded=%d ineffective=%d dealt=%.0f taken_hits=%d received=%.0f "
                   "chain_max=%d killed=%d ko_prevented_player=%d ko_prevented_enemy=%d",
                   pszReason ? pszReason : "manual",
                   s_aArena[s_lab.m_iArenaLoaded].m_pszName,
                   PLAYERID::GetName(s_lab.m_idPlayerLoaded),
                   GetRulesetName(s_lab.m_ruleset),
                   GetAIModeName(s_lab.m_aimode),
                   span,
                   s.m_nPlayerHits, s.m_nPlayerHitsGuarded, s.m_nPlayerHitsIneffective, s.m_fDamageDealt,
                   s.m_nEnemyHitsOnPlayer, s.m_fDamageReceived,
                   s.m_nChainMax, s.m_nEnemyKilled, s.m_nPlayerKOPrevented, s.m_nEnemyKOPrevented);

    char results[256];
    results[0] = '\0';
    for (int32 i = 0; i < COUNT_OF(s.m_aResultCount); ++i)
    {
        if (!s.m_aResultCount[i])
            continue;
        char item[48];
        std::snprintf(item, sizeof(item), " %s=%d", AttackResultName(static_cast<CHARACTERTYPES::ATTACKRESULTTYPE>(i)), s.m_aResultCount[i]);
        strcat_s(results, sizeof(results), item);
    };
    LabLog().Write("RESULTS %s", results[0] ? results : " none");
    LabLog().Flush();
};


/*static*/ bool CCombatLab::IsOverlayEnabled(void)
{
    return s_lab.m_bOverlay;
};


/*static*/ void CCombatLab::SetOverlayEnabled(bool bEnable)
{
    s_lab.m_bOverlay = bEnable;
};


/*static*/ void CCombatLab::OnCatchAttack(
    CCharacter& rTarget,
    CHitAttackData& rAttack,
    CHARACTERTYPES::ATTACKRESULTTYPE attackresult,
    int32 iDamage
)
{
    if (!s_lab.m_bPlaying)
        return;

    STATS& s = s_lab.m_stats;
    float now = LabNow();

    CCharacter* pAttackerChr = nullptr;
    CGameObject* pAttacker = rAttack.GetObject();
    if (pAttacker && (pAttacker->GetType() == GAMEOBJECTTYPE::CHARACTER))
        pAttackerChr = static_cast<CCharacter*>(pAttacker);

    bool bAttackerPlayer = (pAttackerChr && (pAttackerChr->GetAttackCharacterType() == CCharacter::TYPE_PLAYER));
    bool bGuarded = ((attackresult == CHARACTERTYPES::ATTACKRESULTTYPE_GUARD) ||
                     (attackresult == CHARACTERTYPES::ATTACKRESULTTYPE_GUARD_IMPACT) ||
                     (attackresult == CHARACTERTYPES::ATTACKRESULTTYPE_GUARD_BREAK));
    bool bIneffective = (attackresult == CHARACTERTYPES::ATTACKRESULTTYPE_INEFFECTIVE);

    if ((attackresult >= 0) && (attackresult < COUNT_OF(s.m_aResultCount)))
        ++s.m_aResultCount[attackresult];

    if (rTarget.GetAttackCharacterType() == CCharacter::TYPE_ENEMY)
    {
        CEnemyCharacter& rEnemyChr = static_cast<CCharacterCompositor&>(rTarget).EnemyChr();
        int32& rHP = rEnemyChr.CharacterParameter().m_feature.m_iHP;
        int32 iHPMax = rEnemyChr.Feature().m_iHPMax;

        if (bAttackerPlayer)
        {
            if (bIneffective)
            {
                ++s.m_nPlayerHitsIneffective;
            }
            else
            {
                ++s.m_nPlayerHits;
                if (bGuarded)
                    ++s.m_nPlayerHitsGuarded;

                s.m_nChain = ((now - s.m_fLastPlayerHitTime) <= CHAIN_GAP_SEC ? s.m_nChain + 1 : 1);
                s.m_fLastPlayerHitTime = now;
                if (s.m_nChain > s.m_nChainMax)
                    s.m_nChainMax = s.m_nChain;
            };

            std::snprintf(s.m_szLastAttackMotion, sizeof(s.m_szLastAttackMotion), "%s",
                          (pAttackerChr->GetMotionName() ? pAttackerChr->GetMotionName() : "<none>"));
        };

        if (iDamage > 0)
            s.m_fDamageDealt += static_cast<float>(iDamage);

        // Keep the enemy alive while still sending the full retail damage
        // message: refill only when this hit would be lethal, so knockback
        // thresholds, HP-phase bosses and hit reactions see real damage.
        bool bLethal = ((iDamage > 0) && (rHP <= iDamage));
        if (bLethal)
        {
            if (s_lab.m_bInfiniteEnemyHP && !rEnemyChr.TestFlag(ENEMYTYPES::FLAG_DEATH_STATUS))
            {
                rHP = (iHPMax > iDamage ? iHPMax : (iDamage + 1));
                ++s.m_nEnemyKOPrevented;
            }
            else
            {
                ++s.m_nEnemyKilled;
            };
        };

        FormatEnemyLabel(rEnemyChr, s.m_szLastTarget, sizeof(s.m_szLastTarget));
        s.m_iLastTargetHP = rHP - (iDamage > 0 ? iDamage : 0);
        s.m_iLastTargetHPMax = iHPMax;
    }
    else if (rTarget.GetAttackCharacterType() == CCharacter::TYPE_PLAYER)
    {
        CPlayerCharacter& rPlayerChr = static_cast<CPlayerCharacter&>(rTarget);
        IGamePlayer* pGamePlayer = CGameProperty::Player(rPlayerChr.GetPlayerNo());

        if (!bIneffective)
            ++s.m_nEnemyHitsOnPlayer;
        if (iDamage > 0)
            s.m_fDamageReceived += static_cast<float>(iDamage);

        // With Infinite Player HP the existing damage negation (God mode
        // mechanism) blocks HP loss. With it OFF HP drains normally, but a
        // lethal hit refills first so the lab never enters a game-over path.
        if (pGamePlayer && pGamePlayer->IsAlive() && !s_lab.m_bInfinitePlayerHP &&
            (iDamage > 0) && (pGamePlayer->GetHP() <= iDamage))
        {
            pGamePlayer->AddHP(pGamePlayer->GetHPMax());
            ++s.m_nPlayerKOPrevented;
        };

        std::snprintf(s.m_szLastTarget, sizeof(s.m_szLastTarget), "P%d %s",
                      rPlayerChr.GetPlayerNo() + 1, PLAYERID::GetName(rPlayerChr.GetID()));
        if (pGamePlayer)
        {
            s.m_iLastTargetHP = pGamePlayer->GetHP() - ((iDamage > 0) && !s_lab.m_bInfinitePlayerHP ? iDamage : 0);
            s.m_iLastTargetHPMax = pGamePlayer->GetHPMax();
        };
    };

    std::snprintf(s.m_szLastResult, sizeof(s.m_szLastResult), "%s", AttackResultName(attackresult));
    s.m_iLastDamage = iDamage;

    if (s_lab.m_bLog)
    {
        LabLog().Write("HIT  t=%.3f atk=%s motion=%s target=%s result=%s dmg=%d hp=%d/%d chain=%d ruleset=%s",
                       now - s.m_fStartTime,
                       (pAttackerChr ? pAttackerChr->GetName() : (pAttacker ? pAttacker->GetName() : "?")),
                       (pAttackerChr && pAttackerChr->GetMotionName() ? pAttackerChr->GetMotionName() : "-"),
                       s.m_szLastTarget,
                       s.m_szLastResult,
                       iDamage,
                       s.m_iLastTargetHP,
                       s.m_iLastTargetHPMax,
                       (bAttackerPlayer ? s.m_nChain : 0),
                       GetRulesetName(s_lab.m_ruleset));
    };
};


/*static*/ const char* CCombatLab::AttackResultName(CHARACTERTYPES::ATTACKRESULTTYPE attackresult)
{
    switch (attackresult)
    {
    case CHARACTERTYPES::ATTACKRESULTTYPE_INEFFECTIVE:       return "INEFFECTIVE";
    case CHARACTERTYPES::ATTACKRESULTTYPE_DAMAGE_KNOCK:      return "KNOCK";
    case CHARACTERTYPES::ATTACKRESULTTYPE_DAMAGE_FLYAWAY:    return "FLYAWAY";
    case CHARACTERTYPES::ATTACKRESULTTYPE_DAMAGE_NOREACTION: return "NOREACTION";
    case CHARACTERTYPES::ATTACKRESULTTYPE_THROW:             return "THROW";
    case CHARACTERTYPES::ATTACKRESULTTYPE_DINDLE:            return "DINDLE";
    case CHARACTERTYPES::ATTACKRESULTTYPE_STUN:              return "STUN";
    case CHARACTERTYPES::ATTACKRESULTTYPE_SLEEP:             return "SLEEP";
    case CHARACTERTYPES::ATTACKRESULTTYPE_FREEZE:            return "FREEZE";
    case CHARACTERTYPES::ATTACKRESULTTYPE_BIND:              return "BIND";
    case CHARACTERTYPES::ATTACKRESULTTYPE_PARTY_ATTACK:      return "PARTY";
    case CHARACTERTYPES::ATTACKRESULTTYPE_GUARD:             return "GUARD";
    case CHARACTERTYPES::ATTACKRESULTTYPE_GUARD_IMPACT:      return "GUARD_IMPACT";
    case CHARACTERTYPES::ATTACKRESULTTYPE_GUARD_BREAK:       return "GUARD_BREAK";
    default:                                                 return "UNKNOWN";
    };
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
