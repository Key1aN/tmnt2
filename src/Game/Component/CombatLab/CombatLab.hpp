#pragma once

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/Component/Enemy/EnemyID.hpp"
#include "Game/Component/GameMain/StageID.hpp"
#include "Game/Component/Player/PlayerID.hpp"
#include "Game/System/Character/CharacterTypes.hpp"


class CCharacter;
class CHitAttackData;
class CGameStage;


//
//  Combat Laboratory runtime (Debug Tools builds only).
//
//  The lab is a normal CStageBaseSequence that loads one retail Battle Nexus
//  arena (map M60X), never activates the arena's enemy generators, and spawns
//  enemies through the same CEnemy::New path the generators use. Every hook
//  here is inert unless IsPlaying() is true, so campaign stages are unaffected.
//
class CCombatLab
{
public:
    enum RULESET
    {
        RULESET_VANILLA = 0,
        RULESET_EXPERIMENTAL,

        RULESET_NUM,
    };

    enum AIMODE
    {
        AIMODE_ORIGINAL = 0,    // retail Normal AI table, no Debug Difficulty override
        AIMODE_PASSIVE,         // StopAI on lab enemies (same call as "Freeze enemy AI")
        AIMODE_CUSTOM,          // whatever the Debug Tools Difficulty tab holds
        AIMODE_AGGRESSIVE,      // preset of existing Difficulty-tab AI values

        AIMODE_NUM,
    };

    enum
    {
        ARENA_ENEMY_MAX = 3,
        SPAWN_MAX       = 6,
    };

    struct ARENA
    {
        const char*     m_pszName;
        STAGEID::VALUE  m_idStage;
        ENEMYID::VALUE  m_aEnemy[ARENA_ENEMY_MAX];  // ID_DUMMY terminates
    };

    struct STATS
    {
        int32 m_nPlayerHits;            // player attacks that caught an enemy and were not ineffective
        int32 m_nPlayerHitsGuarded;     // ... of which the enemy guarded
        int32 m_nPlayerHitsIneffective;
        int32 m_nEnemyHitsOnPlayer;
        float m_fDamageDealt;           // theoretical damage from CCharacterAttackCalculator::CalcDamage
        float m_fDamageReceived;
        int32 m_nChain;                 // consecutive player hits with gaps <= CHAIN_GAP_SEC
        int32 m_nChainMax;
        float m_fLastPlayerHitTime;
        int32 m_nPlayerKOPrevented;
        int32 m_nEnemyKOPrevented;
        int32 m_nEnemyKilled;
        int32 m_aResultCount[CHARACTERTYPES::ATTACKRESULTTYPE_GUARD_BREAK + 1];
        char  m_szLastAttackMotion[32];
        char  m_szLastTarget[40];
        char  m_szLastResult[24];
        int32 m_iLastDamage;
        int32 m_iLastTargetHP;
        int32 m_iLastTargetHPMax;
        float m_fStartTime;
    };

    static const float CHAIN_GAP_SEC;

public:
    // launch / sequence flow
    static void InitializeLaunch(void);
    static bool ConsumeLaunchRequest(void);
    static void RequestEnterFromGameplay(void);
    static bool ConsumeEnterRequest(void);
    static void RequestRestart(void);
    static bool ConsumeRestartRequest(void);
    static void RequestExit(void);
    static bool IsExitRequested(void);

    static void OnSequenceAttach(void);
    static void OnSequenceDetach(void);
    static void OnPlayStart(CGameStage* pStage);
    static void OnPlayEnd(void);
    static void Period(void);
    static bool IsActive(void);
    static bool IsPlaying(void);

    // configuration (arena / player apply on the next (re)load)
    static int32 GetArenaNum(void);
    static const ARENA& GetArena(int32 index);
    static int32 GetSelectedArena(void);
    static int32 GetLoadedArena(void);
    static void SelectArena(int32 index);
    static PLAYERID::VALUE GetSelectedPlayer(void);
    static PLAYERID::VALUE GetLoadedPlayer(void);
    static void SelectPlayer(PLAYERID::VALUE idPlayer);
    static bool IsReloadPending(void);

    // Crystal (upgrade) level used by the lab: -1 = save file, 0..3 = forced.
    // The retail combo tree and attack power read these live from the record
    // (AAC needs red lv1, AABB red lv2, ... power x2 at red lv3).
    static int32 GetCrystalLevel(void);
    static void SetCrystalLevel(int32 level);

    static int32 GetEnemyTypeIndex(void);
    static int32 GetEnemyTypeNum(void);
    static ENEMYID::VALUE GetEnemyType(void);
    static void SelectEnemyType(int32 index);
    static int32 GetEnemyCount(void);
    static void SetEnemyCount(int32 count);

    static bool IsInfinitePlayerHP(void);
    static void SetInfinitePlayerHP(bool bEnable);
    static bool IsInfiniteEnemyHP(void);
    static void SetInfiniteEnemyHP(bool bEnable);
    static bool IsResetClearsStats(void);
    static void SetResetClearsStats(bool bEnable);

    static RULESET GetRuleset(void);
    static void SetRuleset(RULESET ruleset);
    static bool IsExperimentalCombat(void);
    static const char* GetRulesetName(RULESET ruleset);

    static AIMODE GetAIMode(void);
    static void SetAIModeValue(AIMODE mode);
    static const char* GetAIModeName(AIMODE mode);

    static void RestoreDefaults(void);

    // scenario
    static int32 SpawnEnemies(void);
    static void ClearEnemies(void);
    static void ResetScenario(void);
    static int32 GetSpawnedAliveNum(void);
    static uint32 GetScenarioSeed(void);

    // diagnostics
    static const STATS& Stats(void);
    static void ResetStats(void);
    static bool IsLogEnabled(void);
    static void SetLogEnabled(bool bEnable);
    static void WriteSummaryToLog(const char* pszReason);
    static bool IsOverlayEnabled(void);
    static void SetOverlayEnabled(bool bEnable);

    // Called from CCharacter::OnMessageCatchAttack after OnDamage (reactions)
    // and CalcDamage ran, and before MESSAGEID_RECVDMG is sent.
    static void OnCatchAttack(
        CCharacter& rTarget,
        CHitAttackData& rAttack,
        CHARACTERTYPES::ATTACKRESULTTYPE attackresult,
        int32 iDamage
    );

    static const char* AttackResultName(CHARACTERTYPES::ATTACKRESULTTYPE attackresult);
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
