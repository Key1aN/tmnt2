#include "TurtleCompanions.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/Component/CombatLab/CombatLab.hpp"
#include "Game/Component/Effect/EffectID.hpp"
#include "Game/Component/Effect/EffectManager.hpp"
#include "Game/Component/Enemy/CharacterCompositor.hpp"
#include "Game/Component/Enemy/EnemyCharacter.hpp"
#include "Game/Component/GameData/GameData.hpp"
#include "Game/Component/GameMain/GameLoader.hpp"
#include "Game/Component/GameMain/GamePlayer.hpp"
#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/GameMain/GameStage.hpp"
#include "Game/Component/GameMain/StageInfo.hpp"
#include "Game/Component/Gimmick/GimmickManager.hpp"
#include "Game/Component/Player/Player.hpp"
#include "Game/Component/Player/PlayerCharacter.hpp"
#include "Game/Component/Player/PlayerTracer.hpp"
#include "Game/System/GameObject/GameObjectManager.hpp"
#include "Game/System/Map/WorldMap.hpp"
#include "Game/System/Misc/Gamepad.hpp"
#include "Game/System/Misc/PadStream.hpp"
#include "Game/System/Sound/GameSound.hpp"
#include "System/PC/PCModFeatures.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>


namespace
{
    enum
    {
        TEAM_MAX    = GAMETYPES::PLAYERS_MAX,
        ENEMY_MAX   = 48,
        PLAN_MAX    = 6,
    };

    enum BUTTON
    {
        BTN_NONE = 0,
        BTN_A,
        BTN_B,
        BTN_C,
    };

    enum AISTATE
    {
        AISTATE_FOLLOW = 0,
        AISTATE_FIGHT,
        AISTATE_GUARD,
        AISTATE_CATCHUP,
        AISTATE_RECOVER,
        AISTATE_PLATFORM,

        AISTATE_NUM,
    };

    static const char* s_apszAIState[AISTATE_NUM] =
    {
        "AI - FOLLOW",
        "AI - FIGHT",
        "AI - GUARD",
        "AI - CATCH UP",
        "AI - RECOVER",
        "AI - JUMP",
    };


    //
    //  Tuning
    //
    enum TUNE
    {
        TUNE_AGGRESSION = 0,
        TUNE_GUARD_SKILL,
        TUNE_FOLLOW_DIST,
        TUNE_FIGHT_RADIUS,
        TUNE_TELEPORT_DIST,
        TUNE_DAMAGE_TAKEN,
        TUNE_CAMERA_RADIUS,

        TUNE_NUM,
    };

    struct TUNEINFO
    {
        const char* m_pszName;
        const char* m_pszFormat;
        float m_fDefault;
        float m_fMin;
        float m_fMax;
        float m_fStep;
        const char* m_pszHelp;
    };

    static const TUNEINFO s_aTuneInfo[TUNE_NUM] =
    {
        { "Aggression",              "%.2fx", 1.00f, 0.25f, 2.00f, 0.05f, "How often companions start a combo and throw shuriken. 1.00x = a combo about every 0.45 s of reach time." },
        { "Guard skill",             "%.0f%%", 50.0f, 0.0f, 100.0f, 5.0f, "Chance that a companion raises its guard when an enemy next to it starts an attack facing it." },
        { "Follow distance",         "%.1f",  3.0f,  1.5f, 8.0f,   0.5f, "How far behind you the companions stand when there is nothing to fight." },
        { "Fight radius",            "%.1f",  10.0f, 4.0f, 20.0f,  1.0f, "Companions only fight enemies within this distance of YOU, so they never wander off into a fight of their own." },
        { "Teleport distance",       "%.1f",  18.0f, 8.0f, 40.0f,  1.0f, "A companion further than this (or stuck for 2.5 s) reappears behind you with the blink effect. It waits until you are on the ground." },
        { "Companion damage taken",  "%.0f%%", 100.0f, 0.0f, 100.0f, 25.0f, "Share of damage that hits a companion and comes off the team health gauge. 100% = same as you. Falling off the map never costs health for a companion." },
        { "Camera keeps companions", "%.1f",  9.0f,  0.0f, 20.0f,  1.0f, "Companions closer than this to you are kept in frame by the camera; further ones are let go so the camera stays on you. 0 = camera follows you only." },
    };


    struct ENEMYSNAP
    {
        uint32 m_hObj;
        RwV3d  m_vPos;
        float  m_fDirection;
        float  m_fRadius;
        bool   m_bBoss;
        bool   m_bAttacking;
        bool   m_bJustAttacked;
    };


    struct ENEMYPREV
    {
        uint32 m_hObj;
        bool   m_bAttacking;
    };


    struct SLOTAI
    {
        AISTATE m_state;
        uint32  m_hTarget;
        float   m_fRetargetTime;
        int32   m_aiPlan[PLAN_MAX];
        int32   m_nPlan;
        int32   m_iStep;
        PLAYERTYPES::STATUS m_statusAtStep;
        float   m_fAttackCooldown;
        float   m_fGuardTime;
        float   m_fKnifeCooldown;
        float   m_fJumpTime;
        bool    m_bJumping;
        bool    m_bJump2nd;
        float   m_fStuckTime;
        float   m_fStuckSample;
        RwV3d   m_vStuckPos;
        bool    m_bWantMove;
        bool    m_bMoving;
        float   m_fTeleportCooldown;
        float   m_fHeightTime;
        uint32  m_uFrame;
        bool    m_bCameraIn;
    };


    struct TEAM
    {
        bool  m_bActive;
        void* m_pStage;
        int32 m_nNum;
        PLAYERID::VALUE    m_aId[TEAM_MAX];
        GAMETYPES::COSTUME m_aCostume[TEAM_MAX];
        int32 m_iHuman;
        int32 m_iHumanPad;
        SLOTAI m_aAI[TEAM_MAX];
    };


    struct STATE
    {
        bool  m_bEnabled;
        bool  m_bInstant;
        bool  m_bFillTeam;
        bool  m_bSwitchEffect;
        bool  m_bShuriken;
        float m_afTune[TUNE_NUM];

        TEAM  m_team;

        // extra turtles loaded by the load sequence for this stage
        STAGEID::VALUE  m_idLoadedStage;
        int32           m_nExtra;
        PLAYERID::VALUE m_aExtra[TEAM_MAX];

        // per frame
        float m_fLastPeriod;
        float m_fDt;
        int32 m_nEnemy;
        ENEMYSNAP m_aEnemy[ENEMY_MAX];
        int32 m_nPrev;
        ENEMYPREV m_aPrev[ENEMY_MAX];

        // badge
        float m_fBadgeStart;
        char  m_szBadge[64];
        bool  m_bBadgeOn;

        uint32 m_uRand;
        bool  m_bInit;
    };


    static STATE s_tc;


    static STATE& Tc(void)
    {
        if (!s_tc.m_bInit)
        {
            std::memset(&s_tc, 0x00, sizeof(s_tc));
            s_tc.m_bInit = true;
            s_tc.m_bEnabled = CPCModFeatures::IsDebugToolsEnabled();
            s_tc.m_bInstant = true;
            s_tc.m_bFillTeam = true;
            s_tc.m_bSwitchEffect = true;
            s_tc.m_bShuriken = true;
            for (int32 i = 0; i < TUNE_NUM; ++i)
                s_tc.m_afTune[i] = s_aTuneInfo[i].m_fDefault;
            s_tc.m_idLoadedStage = STAGEID::ID_NONE;
            s_tc.m_fLastPeriod = -1.0f;
            s_tc.m_fBadgeStart = -1000.0f;
            s_tc.m_uRand = 0x2545F491u;
        };

        return s_tc;
    };


    static float Tune(TUNE t)
    {
        return Tc().m_afTune[t];
    };


    // Own generator so the companions never shift the game's random sequence.
    static float Rand01(void)
    {
        uint32& x = Tc().m_uRand;
        x ^= (x << 13);
        x ^= (x >> 17);
        x ^= (x << 5);
        return static_cast<float>(x & 0xFFFFFF) / static_cast<float>(0x1000000);
    };


    static float Now(void)
    {
        return CGameProperty::GetTotalElapsedTime();
    };


    static void SetBadge(const char* pszText, bool bOn)
    {
        STATE& tc = Tc();
        std::snprintf(tc.m_szBadge, sizeof(tc.m_szBadge), "%s", pszText);
        tc.m_bBadgeOn = bOn;
        tc.m_fBadgeStart = (CGameStage::GetCurrent() ? Now() : 0.0f);
    };


    static const char* TurtleName(PLAYERID::VALUE id)
    {
        switch (id)
        {
        case PLAYERID::ID_LEO: return "Leonardo";
        case PLAYERID::ID_RAP: return "Raphael";
        case PLAYERID::ID_MIC: return "Michelangelo";
        case PLAYERID::ID_DON: return "Donatello";
        case PLAYERID::ID_SLA: return "Slashuur";
        case PLAYERID::ID_CAS: return "Casey Jones";
        case PLAYERID::ID_KAR: return "Karai";
        case PLAYERID::ID_SPL: return "Splinter";
        default:               return "?";
        };
    };


    //
    //  Status groups
    //
    static bool IsFreeStatus(PLAYERTYPES::STATUS s)
    {
        return ((s == PLAYERTYPES::STATUS_IDLE) ||
                (s == PLAYERTYPES::STATUS_WALK) ||
                (s == PLAYERTYPES::STATUS_RUN));
    };


    static bool IsAttackStatus(PLAYERTYPES::STATUS s)
    {
        return ((s >= PLAYERTYPES::STATUS_ATTACK_A) && (s <= PLAYERTYPES::STATUS_ATTACK_KNIFE_JUMP));
    };


    static bool IsAirStatus(PLAYERTYPES::STATUS s)
    {
        return (((s >= PLAYERTYPES::STATUS_JUMP_READY) && (s <= PLAYERTYPES::STATUS_TOUCHDOWN)) ||
                (s == PLAYERTYPES::STATUS_ATTACK_JUMP) ||
                (s == PLAYERTYPES::STATUS_ATTACK_KNIFE_JUMP));
    };


    static bool IsHurtStatus(PLAYERTYPES::STATUS s)
    {
        return ((s >= PLAYERTYPES::STATUS_CAUGHT) && (s <= PLAYERTYPES::STATUS_TUMBLER_BACK));
    };


    static bool IsDeadStatus(PLAYERTYPES::STATUS s)
    {
        return ((s == PLAYERTYPES::STATUS_DEAD) || (s == PLAYERTYPES::STATUS_DEAD_FLYAWAY));
    };


    static bool IsGuardStatus(PLAYERTYPES::STATUS s)
    {
        return ((s >= PLAYERTYPES::STATUS_GUARD_READY) && (s <= PLAYERTYPES::STATUS_GUARD_FINISH));
    };


    static bool IsLiftStatus(PLAYERTYPES::STATUS s)
    {
        return ((s >= PLAYERTYPES::STATUS_LIFT_CHALLENGE) && (s <= PLAYERTYPES::STATUS_THROW_COMBINATION));
    };


    // grounded and in control: a teleport target or a teleport moment
    static bool IsSettled(PLAYERTYPES::STATUS s)
    {
        return (!IsAirStatus(s) && !IsHurtStatus(s) && !IsDeadStatus(s) && !IsLiftStatus(s));
    };


    static bool IsEnemyAttackStatus(ENEMYTYPES::STATUS s)
    {
        return (((s >= ENEMYTYPES::STATUS_ATTACK_A) && (s <= ENEMYTYPES::STATUS_JUMP_ATTACK)) ||
                (s == ENEMYTYPES::STATUS_SPECIAL));
    };


    //
    //  Geometry (same yaw convention as CManipulator::AnalyzeInputDevice)
    //
    static float YawTo(const RwV3d& rvFrom, const RwV3d& rvTo)
    {
        return Math::ATan2(rvTo.x - rvFrom.x, rvTo.z - rvFrom.z);
    };


    static void YawVector(float fYaw, RwV3d& rvOut)
    {
        rvOut.x = Math::Sin(fYaw);
        rvOut.y = 0.0f;
        rvOut.z = Math::Cos(fYaw);
    };


    static float DistXZ(const RwV3d& a, const RwV3d& b)
    {
        float dx = (a.x - b.x);
        float dz = (a.z - b.z);
        return std::sqrt((dx * dx) + (dz * dz));
    };


    static CPlayerCharacter* SlotCharacter(int32 nSlot)
    {
        if ((nSlot < 0) || (nSlot >= CGameProperty::GetPlayerNum()))
            return nullptr;

        IGamePlayer* pGamePlayer = CGameProperty::Player(nSlot);
        if (!pGamePlayer || !pGamePlayer->IsAlive())
            return nullptr;

        return pGamePlayer->GetCurrentCharacter();
    };


    static void ResetSlotAI(SLOTAI& ai)
    {
        bool bCameraIn = ai.m_bCameraIn;
        std::memset(&ai, 0x00, sizeof(ai));
        ai.m_state = AISTATE_FOLLOW;
        ai.m_statusAtStep = PLAYERTYPES::STATUS_INVALID;
        ai.m_fAttackCooldown = 0.3f;
        ai.m_fKnifeCooldown = 1.5f;
        ai.m_bCameraIn = bCameraIn;
    };


    static void ApplyPads(void)
    {
        TEAM& team = Tc().m_team;
        for (int32 i = 0; i < team.m_nNum; ++i)
        {
            CPlayerCharacter* pChr = SlotCharacter(i);
            if (pChr)
                pChr->SetPadID((i == team.m_iHuman) ? team.m_iHumanPad : -1);
        };
    };


    static bool IsEligibleSession(void)
    {
        if (!Tc().m_bEnabled || !CPCModFeatures::IsDebugToolsEnabled())
            return false;

        if (CCombatLab::IsActive())
            return false;

        if (CGameData::Attribute().IsNexusMode() || CGameData::Attribute().IsPlayDemoMode())
            return false;

        if (CPadStreamSwitch::m_mode != CPadStream::MODE_NONE)
            return false;

        if (CGameData::PlayParam().GetPlayerNum() != 1)
            return false;

        int32 nInfo = CGameData::PlayParam().GetCharaInfoNum();
        for (int32 i = 0; i < nInfo; ++i)
        {
            if (CGameData::PlayParam().CharaInfo(i).m_iPlayerNo != 0)
                return false;
        };

        return (nInfo > 0);
    };


    static bool IsEligibleStageMode(GAMETYPES::STAGEMODE mode)
    {
        return (mode == GAMETYPES::STAGEMODE_NORMAL);
    };


    static bool IsInCharaInfo(PLAYERID::VALUE id)
    {
        int32 nInfo = CGameData::PlayParam().GetCharaInfoNum();
        for (int32 i = 0; i < nInfo; ++i)
        {
            if (CGameData::PlayParam().CharaInfo(i).m_CharacterID == id)
                return true;
        };
        return false;
    };


    //
    //  Enemy snapshot, once per frame
    //
    static void SnapshotEnemies(void)
    {
        STATE& tc = Tc();

        ENEMYPREV aPrev[ENEMY_MAX];
        int32 nPrev = tc.m_nPrev;
        std::memcpy(aPrev, tc.m_aPrev, sizeof(aPrev));

        tc.m_nEnemy = 0;
        tc.m_nPrev = 0;

        CGameObject* pObj = CGameObjectManager::GetNext(GAMEOBJECTTYPE::CHARACTER);
        while (pObj && (tc.m_nEnemy < ENEMY_MAX))
        {
            CCharacter* pChr = static_cast<CCharacter*>(pObj);
            if (pChr->GetAttackCharacterType() == CCharacter::TYPE_ENEMY)
            {
                CCharacterCompositor* pComp = static_cast<CCharacterCompositor*>(pChr);
                CEnemyCharacter& rEnemyChr = pComp->EnemyChr();
                ENEMYTYPES::STATUS status = rEnemyChr.GetStatus();

                bool bAlive = (!rEnemyChr.TestFlag(ENEMYTYPES::FLAG_DEATH_STATUS) &&
                               (status != ENEMYTYPES::STATUS_HIDE) &&
                               (status != ENEMYTYPES::STATUS_QUIT) &&
                               (status != ENEMYTYPES::STATUS_DEATH) &&
                               (status != ENEMYTYPES::STATUS_APPEAR));
                if (bAlive)
                {
                    ENEMYSNAP& e = tc.m_aEnemy[tc.m_nEnemy++];
                    e.m_hObj = pObj->GetHandle();
                    pComp->GetFootPosition(&e.m_vPos);
                    e.m_fDirection = pComp->GetDirection();
                    e.m_fRadius = Clamp(pComp->GetCollisionParameter().m_fRadius, 0.3f, 2.5f);
                    e.m_bBoss = ((pComp->GetID() >= ENEMYID::BOSSBEGIN) && (pComp->GetID() < ENEMYID::BOSSEND));
                    e.m_bAttacking = IsEnemyAttackStatus(status);

                    bool bWas = false;
                    for (int32 i = 0; i < nPrev; ++i)
                    {
                        if (aPrev[i].m_hObj == e.m_hObj)
                        {
                            bWas = aPrev[i].m_bAttacking;
                            break;
                        };
                    };
                    e.m_bJustAttacked = (e.m_bAttacking && !bWas);

                    ENEMYPREV& p = tc.m_aPrev[tc.m_nPrev++];
                    p.m_hObj = e.m_hObj;
                    p.m_bAttacking = e.m_bAttacking;
                };
            };

            pObj = CGameObjectManager::GetNext(GAMEOBJECTTYPE::CHARACTER, pObj);
        };
    };


    static const ENEMYSNAP* FindEnemy(uint32 hObj)
    {
        STATE& tc = Tc();
        for (int32 i = 0; i < tc.m_nEnemy; ++i)
        {
            if (tc.m_aEnemy[i].m_hObj == hObj)
                return &tc.m_aEnemy[i];
        };
        return nullptr;
    };


    static int32 CountOthersOnTarget(int32 nSlot, uint32 hTarget)
    {
        TEAM& team = Tc().m_team;
        int32 n = 0;
        for (int32 i = 0; i < team.m_nNum; ++i)
        {
            if ((i == nSlot) || (i == team.m_iHuman))
                continue;
            if (team.m_aAI[i].m_hTarget == hTarget)
                ++n;
        };
        return n;
    };


    static void ChooseTarget(int32 nSlot, SLOTAI& ai, const RwV3d& rvSelf, const RwV3d& rvLeader)
    {
        STATE& tc = Tc();
        float fRadius = Tune(TUNE_FIGHT_RADIUS);

        const ENEMYSNAP* pCurrent = FindEnemy(ai.m_hTarget);
        float fCurrentScore = TYPEDEF::FLOAT_MAX;

        uint32 hBest = 0;
        float fBest = TYPEDEF::FLOAT_MAX;

        for (int32 i = 0; i < tc.m_nEnemy; ++i)
        {
            const ENEMYSNAP& e = tc.m_aEnemy[i];

            if (DistXZ(e.m_vPos, rvLeader) > fRadius)
                continue;

            if (std::fabs(e.m_vPos.y - rvSelf.y) > 3.5f)
                continue;

            float fScore = DistXZ(e.m_vPos, rvSelf);
            fScore += (3.0f * static_cast<float>(CountOthersOnTarget(nSlot, e.m_hObj)));
            fScore += (0.25f * DistXZ(e.m_vPos, rvLeader));

            if (pCurrent && (e.m_hObj == pCurrent->m_hObj))
                fCurrentScore = fScore;

            if (fScore < fBest)
            {
                fBest = fScore;
                hBest = e.m_hObj;
            };
        };

        // sticky: only change for a clearly better target
        if (pCurrent && (fCurrentScore < TYPEDEF::FLOAT_MAX) && (fCurrentScore <= (fBest + 2.0f)))
            return;

        ai.m_hTarget = hBest;
        ai.m_nPlan = 0;
    };


    static void PickPlan(SLOTAI& ai, bool bBoss)
    {
        struct PLAN
        {
            int32 m_aiBtn[PLAN_MAX];
            int32 m_nBtn;
            float m_fWeight;
            float m_fBossWeight;
        };

        static const PLAN s_aPlan[] =
        {
            { { BTN_A, BTN_A, BTN_B, BTN_B, BTN_B }, 5, 3.0f, 1.0f },   // AABBB
            { { BTN_A, BTN_A, BTN_B, BTN_B, BTN_C }, 5, 2.0f, 1.0f },   // AABBC
            { { BTN_A, BTN_A, BTN_B },               3, 2.0f, 2.0f },   // AAB
            { { BTN_A, BTN_A, BTN_C },               3, 1.0f, 1.0f },   // AAC
            { { BTN_A, BTN_A },                      2, 2.0f, 3.0f },   // AA
            { { BTN_B },                             1, 1.0f, 1.0f },   // strong attack
        };

        float fTotal = 0.0f;
        for (int32 i = 0; i < COUNT_OF(s_aPlan); ++i)
            fTotal += (bBoss ? s_aPlan[i].m_fBossWeight : s_aPlan[i].m_fWeight);

        float r = Rand01() * fTotal;
        int32 iPick = 0;
        for (int32 i = 0; i < COUNT_OF(s_aPlan); ++i)
        {
            r -= (bBoss ? s_aPlan[i].m_fBossWeight : s_aPlan[i].m_fWeight);
            if (r <= 0.0f)
            {
                iPick = i;
                break;
            };
        };

        ai.m_nPlan = s_aPlan[iPick].m_nBtn;
        for (int32 i = 0; i < ai.m_nPlan; ++i)
            ai.m_aiPlan[i] = s_aPlan[iPick].m_aiBtn[i];
        ai.m_iStep = 0;
        ai.m_statusAtStep = PLAYERTYPES::STATUS_INVALID;
    };


    // Formation point behind the leader: one spot per companion
    static void FormationPoint(int32 nOrder, const CPlayerCharacter& rLeader, RwV3d& rvOut)
    {
        static const float s_afAngle[] = { 145.0f, 215.0f, 180.0f };

        RwV3d vLeader = Math::VECTOR3_ZERO;
        rLeader.GetFootPosition(&vLeader);

        float fYaw = rLeader.GetDirection() + MATH_DEG2RAD(s_afAngle[nOrder % COUNT_OF(s_afAngle)]);
        RwV3d vDir = Math::VECTOR3_ZERO;
        YawVector(fYaw, vDir);

        float fDist = Tune(TUNE_FOLLOW_DIST);
        rvOut.x = vLeader.x + (vDir.x * fDist);
        rvOut.y = vLeader.y;
        rvOut.z = vLeader.z + (vDir.z * fDist);
    };


    static bool TryTeleport(int32 nSlot, SLOTAI& ai, const CPlayerCharacter& rLeader)
    {
        if (ai.m_fTeleportCooldown > 0.0f)
            return false;

        if (!IsSettled(rLeader.GetStatus()))
            return false;

        RwV3d vLeader = Math::VECTOR3_ZERO;
        rLeader.GetFootPosition(&vLeader);

        // just behind the leader; fall back to the leader's own spot when that
        // point has no safe ground
        RwV3d vDir = Math::VECTOR3_ZERO;
        YawVector(rLeader.GetDirection() + MATH_DEG2RAD(180.0f + ((nSlot - 2) * 35.0f)), vDir);

        RwV3d vPos = { vLeader.x + (vDir.x * 1.4f), vLeader.y + 1.0f, vLeader.z + (vDir.z * 1.4f) };
        float fHeight = CWorldMap::GetMapHeight(&vPos);
        MAPTYPES::ATTRIBUTE attribute = CWorldMap::GetCollisionResultAttribute();

        if ((attribute == MAPTYPES::ATTRIBUTE_DEATH) || (std::fabs(fHeight - vLeader.y) > 0.75f))
            vPos = vLeader;
        else
            vPos.y = fHeight;

        IGamePlayer* pGamePlayer = CGameProperty::Player(nSlot);
        if (!pGamePlayer)
            return false;

        pGamePlayer->Relocation(&vPos, rLeader.GetDirection(), true);

        ai.m_fTeleportCooldown = 2.0f;
        ai.m_fStuckTime = 0.0f;
        ai.m_fHeightTime = 0.0f;
        ai.m_vStuckPos = vPos;
        ai.m_nPlan = 0;
        ai.m_hTarget = 0;
        return true;
    };


    static void MoveTo(CTurtleCompanions::AICOMMAND& rCmd, const RwV3d& rvSelf, const RwV3d& rvTo, bool bRun)
    {
        rCmd.m_bMove = true;
        rCmd.m_bRun = bRun;
        rCmd.m_fDirection = YawTo(rvSelf, rvTo);
    };


    static void SwitchTo(int32 nSlot, bool bEffect)
    {
        STATE& tc = Tc();
        TEAM& team = tc.m_team;

        if ((nSlot < 0) || (nSlot >= team.m_nNum) || (nSlot == team.m_iHuman))
            return;

        CPlayerCharacter* pNext = SlotCharacter(nSlot);
        if (!pNext || IsDeadStatus(pNext->GetStatus()))
            return;

        int32 iOld = team.m_iHuman;
        team.m_iHuman = nSlot;
        ResetSlotAI(team.m_aAI[iOld]);
        ResetSlotAI(team.m_aAI[nSlot]);
        ApplyPads();

        CGameSound::PlaySE(SDCODE_SE(4118));
        if (bEffect && tc.m_bSwitchEffect)
            CEffectManager::PlayTrace(EFFECTID::ID_BARRIER_START, new CPlayerTracer(pNext), &Math::VECTOR3_ZERO);

        char szBadge[64];
        std::snprintf(szBadge, sizeof(szBadge), "YOU: %s", TurtleName(team.m_aId[nSlot]));
        for (char* p = szBadge; *p; ++p)
        {
            if ((*p >= 'a') && (*p <= 'z'))
                *p = static_cast<char>(*p - ('a' - 'A'));
        };
        SetBadge(szBadge, true);
    };


    static void SwitchNext(void)
    {
        TEAM& team = Tc().m_team;
        for (int32 n = 1; n < team.m_nNum; ++n)
        {
            int32 i = ((team.m_iHuman + n) % team.m_nNum);
            CPlayerCharacter* pChr = SlotCharacter(i);
            if (pChr && !IsDeadStatus(pChr->GetStatus()))
            {
                SwitchTo(i, true);
                return;
            };
        };
    };


    //
    //  Menu rows
    //
    enum ITEM
    {
        ITEM_ENABLE = 0,
        ITEM_INSTANT,
        ITEM_FILL,
        ITEM_EFFECT,
        ITEM_SHURIKEN,
        ITEM_TUNE_FIRST,
        ITEM_TUNE_LAST = (ITEM_TUNE_FIRST + TUNE_NUM - 1),
        ITEM_SLOT_FIRST,
        ITEM_SLOT_LAST = (ITEM_SLOT_FIRST + TEAM_MAX - 1),
        ITEM_RESET,

        ITEM_NUM,
    };
};


/*static*/ bool CTurtleCompanions::IsEnabled(void)
{
    return Tc().m_bEnabled;
};


/*static*/ void CTurtleCompanions::SetEnabled(bool bEnable)
{
    Tc().m_bEnabled = bEnable;
    SetBadge(bEnable ? "AI COMPANIONS ON - FROM NEXT STAGE START" : "AI COMPANIONS OFF - FROM NEXT STAGE START", bEnable);
};


/*static*/ bool CTurtleCompanions::IsInstantSwitch(void)
{
    return (Tc().m_bInstant && CPCModFeatures::IsDebugToolsEnabled());
};


/*static*/ float CTurtleCompanions::GetBadgeTime(void)
{
    if (!CGameStage::GetCurrent())
        return 1000.0f;

    float fTime = (Now() - Tc().m_fBadgeStart);
    return ((fTime < 0.0f) ? 1000.0f : fTime);
};


/*static*/ bool CTurtleCompanions::GetBadgeText(char* pszBuffer, size_t capacity, bool& rbOn)
{
    if (GetBadgeTime() >= 2.0f)
        return false;

    std::snprintf(pszBuffer, capacity, "%s", Tc().m_szBadge);
    rbOn = Tc().m_bBadgeOn;
    return true;
};


/*static*/ void CTurtleCompanions::OnStageInitialize(void)
{
    STATE& tc = Tc();
    tc.m_team.m_bActive = false;
    tc.m_team.m_pStage = nullptr;
    tc.m_nEnemy = 0;
    tc.m_nPrev = 0;
    tc.m_fLastPeriod = -1.0f;
    tc.m_fBadgeStart = -1000.0f;
};


/*static*/ void CTurtleCompanions::OnStageTerminate(void)
{
    OnStageInitialize();
};


/*static*/ void CTurtleCompanions::OnLoadPlayers(STAGEID::VALUE idStage)
{
    STATE& tc = Tc();
    tc.m_idLoadedStage = STAGEID::ID_NONE;
    tc.m_nExtra = 0;

    if (!tc.m_bFillTeam || !IsEligibleSession())
        return;

    if (!IsEligibleStageMode(CStageInfo::GetMode(idStage)))
        return;

    static const PLAYERID::VALUE s_aTurtle[] =
    {
        PLAYERID::ID_LEO,
        PLAYERID::ID_RAP,
        PLAYERID::ID_MIC,
        PLAYERID::ID_DON,
    };

    int32 nTotal = CGameData::PlayParam().GetCharaInfoNum();
    for (int32 i = 0; (i < COUNT_OF(s_aTurtle)) && (nTotal < TEAM_MAX); ++i)
    {
        if (IsInCharaInfo(s_aTurtle[i]))
            continue;

        CGameLoader::LoadPlayer(s_aTurtle[i], GAMETYPES::COSTUME_NONE);
        tc.m_aExtra[tc.m_nExtra++] = s_aTurtle[i];
        ++nTotal;
    };

    tc.m_idLoadedStage = idStage;
};


/*static*/ bool CTurtleCompanions::AddPlayers(bool bBlink)
{
    STATE& tc = Tc();
    TEAM& team = tc.m_team;

    team.m_bActive = false;
    team.m_nNum = 0;

    if (!IsEligibleSession())
        return false;

    if (!IsEligibleStageMode(CGameData::PlayParam().GetStageMode()))
        return false;

    int32 nInfo = CGameData::PlayParam().GetCharaInfoNum();
    for (int32 i = 0; (i < nInfo) && (team.m_nNum < TEAM_MAX); ++i)
    {
        const CGamePlayParam::CHARAINFO& rInfo = CGameData::PlayParam().CharaInfo(i);
        team.m_aId[team.m_nNum] = rInfo.m_CharacterID;
        team.m_aCostume[team.m_nNum] = rInfo.m_Costume;
        ++team.m_nNum;
    };

    if (tc.m_idLoadedStage == CGameData::PlayParam().GetStage())
    {
        for (int32 i = 0; (i < tc.m_nExtra) && (team.m_nNum < TEAM_MAX); ++i)
        {
            team.m_aId[team.m_nNum] = tc.m_aExtra[i];
            team.m_aCostume[team.m_nNum] = GAMETYPES::COSTUME_NONE;
            ++team.m_nNum;
        };
    };

    if (team.m_nNum < 2)
    {
        team.m_nNum = 0;
        return false;
    };

    const CGamePlayParam::PLAYERCONTEXT ctx = CGameData::PlayParam().PlayerContext(0);

    team.m_iHuman = 0;
    for (int32 i = 0; i < team.m_nNum; ++i)
    {
        if (team.m_aId[i] == ctx.m_CharacterID)
        {
            team.m_iHuman = i;
            break;
        };
    };

    for (int32 i = 0; i < team.m_nNum; ++i)
    {
        CGameProperty::AddPlayerCharacter(i, team.m_aId[i], team.m_aCostume[i]);
        CGimmickManager::SetPlayerStartPosition(i, bBlink);
    };

    for (int32 i = 0; i < team.m_nNum; ++i)
    {
        CGamePlayParam::PLAYERCONTEXT slotCtx = ctx;
        slotCtx.m_CharacterID = team.m_aId[i];
        CGameProperty::Player(i)->LoadContext(slotCtx);
    };

    // slot 0 was created with the human's pad
    team.m_iHumanPad = CGameProperty::Player(0)->GetPlayer()->GetPadID();
    team.m_pStage = CGameStage::GetCurrent();
    team.m_bActive = true;

    for (int32 i = 0; i < TEAM_MAX; ++i)
    {
        team.m_aAI[i].m_bCameraIn = true;
        ResetSlotAI(team.m_aAI[i]);
    };

    ApplyPads();
    return true;
};


/*static*/ bool CTurtleCompanions::SavePlayData(void)
{
    if (!IsActive())
        return false;

    TEAM& team = Tc().m_team;
    CGameProperty::Player(team.m_iHuman)->SaveContext(CGameData::PlayParam().PlayerContext(0));
    return true;
};


/*static*/ bool CTurtleCompanions::IsActive(void)
{
    const TEAM& team = Tc().m_team;
    return (team.m_bActive &&
            (team.m_pStage != nullptr) &&
            (team.m_pStage == CGameStage::GetCurrent()) &&
            (CGameProperty::GetPlayerNum() == team.m_nNum));
};


/*static*/ int32 CTurtleCompanions::BalancePlayerNum(int32 nRetailPlayerNum)
{
    return (IsActive() ? 1 : nRetailPlayerNum);
};


/*static*/ int32 CTurtleCompanions::GetHumanPlayerNo(void)
{
    return (IsActive() ? Tc().m_team.m_iHuman : 0);
};


/*static*/ bool CTurtleCompanions::IsHumanSlot(int32 nPlayerNo)
{
    return (IsActive() && (nPlayerNo == Tc().m_team.m_iHuman));
};


/*static*/ bool CTurtleCompanions::IsAISlot(int32 nPlayerNo)
{
    return (IsActive() &&
            (nPlayerNo >= 0) &&
            (nPlayerNo < Tc().m_team.m_nNum) &&
            (nPlayerNo != Tc().m_team.m_iHuman));
};


/*static*/ int32 CTurtleCompanions::GetInputPort(int32 nPlayerNo, int32 nDefaultPort)
{
    if (!IsActive())
        return nDefaultPort;

    const TEAM& team = Tc().m_team;
    if (nPlayerNo == team.m_iHuman)
        return team.m_iHumanPad;

    if ((nPlayerNo >= 0) && (nPlayerNo < team.m_nNum))
        return -1;

    return nDefaultPort;
};


/*static*/ bool CTurtleCompanions::IsCameraTarget(int32 nPlayerNo)
{
    if (!IsActive())
        return true;

    TEAM& team = Tc().m_team;
    if (nPlayerNo == team.m_iHuman)
        return true;

    if ((nPlayerNo < 0) || (nPlayerNo >= team.m_nNum))
        return true;

    float fRadius = Tune(TUNE_CAMERA_RADIUS);
    SLOTAI& ai = team.m_aAI[nPlayerNo];

    CPlayerCharacter* pChr = SlotCharacter(nPlayerNo);
    CPlayerCharacter* pLeader = SlotCharacter(team.m_iHuman);
    if (!pChr || !pLeader || (fRadius <= 0.0f))
    {
        ai.m_bCameraIn = false;
        return false;
    };

    RwV3d vSelf = Math::VECTOR3_ZERO;
    RwV3d vLeader = Math::VECTOR3_ZERO;
    pChr->GetFootPosition(&vSelf);
    pLeader->GetFootPosition(&vLeader);

    RwV3d vDelta = Math::VECTOR3_ZERO;
    Math::Vec3_Sub(&vDelta, &vSelf, &vLeader);
    float fDist = Math::Vec3_Length(&vDelta);

    // hysteresis so a companion at the edge does not pump the zoom
    if (ai.m_bCameraIn)
        ai.m_bCameraIn = (fDist <= (fRadius * 1.3f));
    else
        ai.m_bCameraIn = (fDist <= fRadius);

    return ai.m_bCameraIn;
};


/*static*/ int32 CTurtleCompanions::ScaleReceivedDamage(const CPlayerCharacter& rPlayerChr, int32 nDamage)
{
    if (!IsAISlot(rPlayerChr.GetPlayerNo()) || (nDamage <= 0))
        return nDamage;

    float fScaled = static_cast<float>(nDamage) * (Tune(TUNE_DAMAGE_TAKEN) / 100.0f);
    return static_cast<int32>(fScaled + 0.5f);
};


/*static*/ void CTurtleCompanions::Period(void)
{
    STATE& tc = Tc();

    if (!IsActive())
        return;

    float fNow = Now();
    if (fNow == tc.m_fLastPeriod)
        return;

    tc.m_fDt = ((tc.m_fLastPeriod < 0.0f) ? 0.0f : Clamp(fNow - tc.m_fLastPeriod, 0.0f, 0.1f));
    tc.m_fLastPeriod = fNow;

    SnapshotEnemies();

    // instant switching: the human's Change Character button
    TEAM& team = tc.m_team;
    if (tc.m_bInstant && (team.m_iHumanPad >= 0) && (team.m_iHumanPad < IGamepad::Max()))
    {
        uint32 uTrigger = IGamepad::GetDigitalTrigger(team.m_iHumanPad);
        if (IGamepad::CheckFunction(uTrigger, IGamepad::FUNCTION_SWITCH_CHR))
            SwitchNext();
    };

    for (int32 i = 0; i < team.m_nNum; ++i)
    {
        SLOTAI& ai = team.m_aAI[i];
        ai.m_fAttackCooldown  -= tc.m_fDt;
        ai.m_fKnifeCooldown   -= tc.m_fDt;
        ai.m_fGuardTime       -= tc.m_fDt;
        ai.m_fTeleportCooldown -= tc.m_fDt;
        ai.m_fRetargetTime    -= tc.m_fDt;
    };
};


/*static*/ void CTurtleCompanions::BuildAICommand(CPlayerCharacter& rPlayerChr, AICOMMAND& rCmd)
{
    std::memset(&rCmd, 0x00, sizeof(rCmd));

    if (!IsActive())
        return;

    STATE& tc = Tc();
    TEAM& team = tc.m_team;

    int32 nSlot = rPlayerChr.GetPlayerNo();
    if ((nSlot < 0) || (nSlot >= team.m_nNum) || (nSlot == team.m_iHuman))
        return;

    // the manipulator may run before CPlayer::Run in a frame
    Period();

    SLOTAI& ai = team.m_aAI[nSlot];
    ++ai.m_uFrame;
    float dt = tc.m_fDt;

    CPlayerCharacter* pLeader = SlotCharacter(team.m_iHuman);
    if (!pLeader)
        return;

    PLAYERTYPES::STATUS status = rPlayerChr.GetStatus();
    PLAYERTYPES::STATUS leaderStatus = pLeader->GetStatus();

    RwV3d vSelf = Math::VECTOR3_ZERO;
    RwV3d vLeader = Math::VECTOR3_ZERO;
    rPlayerChr.GetFootPosition(&vSelf);
    pLeader->GetFootPosition(&vLeader);

    float fLeaderDist = DistXZ(vSelf, vLeader);
    float fLeaderDy = (vLeader.y - vSelf.y);

    if (IsDeadStatus(status))
        return;

    // ---- knocked down, grabbed, stunned: mash out --------------------------
    if (IsHurtStatus(status))
    {
        ai.m_state = AISTATE_RECOVER;
        ai.m_nPlan = 0;
        bool bPress = ((ai.m_uFrame & 3) == 0);
        rCmd.m_bRecover = bPress;
        rCmd.m_bPassive = bPress;
        return;
    };

    // carrying an enemy: throw it
    if (IsLiftStatus(status))
    {
        if ((status == PLAYERTYPES::STATUS_LIFT) || (status == PLAYERTYPES::STATUS_LIFT_WALK))
            rCmd.m_iAttack = BTN_A;
        return;
    };

    // ---- catch up -----------------------------------------------------------
    {
        float fFar = Tune(TUNE_TELEPORT_DIST);
        float fDist3 = std::sqrt((fLeaderDist * fLeaderDist) + (fLeaderDy * fLeaderDy));

        if (std::fabs(fLeaderDy) > 4.0f)
            ai.m_fHeightTime += dt;
        else
            ai.m_fHeightTime = 0.0f;

        bool bTeleport = ((fDist3 > fFar) ||
                          (ai.m_fStuckTime >= 2.5f) ||
                          (ai.m_fHeightTime >= 3.0f));

        if (bTeleport && IsSettled(status) && TryTeleport(nSlot, ai, *pLeader))
        {
            ai.m_state = AISTATE_CATCHUP;
            return;
        };
    };

    // ---- stuck detection ------------------------------------------------------
    ai.m_fStuckSample += dt;
    if (ai.m_fStuckSample >= 0.5f)
    {
        float fMoved = DistXZ(vSelf, ai.m_vStuckPos);
        if (ai.m_bWantMove && IsFreeStatus(status) && (fMoved < 0.25f))
            ai.m_fStuckTime += ai.m_fStuckSample;
        else
            ai.m_fStuckTime = 0.0f;

        ai.m_vStuckPos = vSelf;
        ai.m_fStuckSample = 0.0f;
    };
    ai.m_bWantMove = false;

    // ---- jumping (platforms, walls) ------------------------------------------
    if (ai.m_bJumping)
    {
        ai.m_fJumpTime += dt;
        if (IsAirStatus(status))
        {
            MoveTo(rCmd, vSelf, vLeader, true);
            ai.m_state = AISTATE_PLATFORM;
            if (!ai.m_bJump2nd && (ai.m_fJumpTime >= 0.28f) && rPlayerChr.IsEnableJump2nd() && (vSelf.y < (vLeader.y + 0.5f)))
            {
                ai.m_bJump2nd = true;
                rCmd.m_bJump = true;
            };
            return;
        };

        if (ai.m_fJumpTime > 0.2f)
            ai.m_bJumping = false;
    };

    bool bWantJump = (IsFreeStatus(status) &&
                      ((ai.m_fStuckTime >= 1.0f) ||
                       ((fLeaderDy > 1.0f) && (fLeaderDist < 7.0f) && (fLeaderDist > 0.8f) && IsSettled(leaderStatus))));

    // ---- choose what to fight --------------------------------------------------
    if (ai.m_fRetargetTime <= 0.0f)
    {
        ChooseTarget(nSlot, ai, vSelf, vLeader);
        ai.m_fRetargetTime = 0.4f;
    };

    const ENEMYSNAP* pTarget = FindEnemy(ai.m_hTarget);
    if (pTarget && (DistXZ(pTarget->m_vPos, vLeader) > (Tune(TUNE_FIGHT_RADIUS) + 4.0f)))
    {
        ai.m_hTarget = 0;
        pTarget = nullptr;
    };

    // ---- guard against an attack that is starting next to us -----------------
    if (IsGuardStatus(status) || (ai.m_fGuardTime > 0.0f))
    {
        if (ai.m_fGuardTime > 0.0f)
        {
            rCmd.m_bGuard = true;
            ai.m_state = AISTATE_GUARD;
            return;
        };
    }
    else if (IsFreeStatus(status) || IsAttackStatus(status))
    {
        float fSkill = (Tune(TUNE_GUARD_SKILL) / 100.0f);
        for (int32 i = 0; (i < tc.m_nEnemy) && (fSkill > 0.0f); ++i)
        {
            const ENEMYSNAP& e = tc.m_aEnemy[i];
            if (!e.m_bJustAttacked)
                continue;

            if (DistXZ(e.m_vPos, vSelf) > (e.m_fRadius + 2.8f))
                continue;

            RwV3d vFacing = Math::VECTOR3_ZERO;
            YawVector(e.m_fDirection, vFacing);
            RwV3d vToSelf = { vSelf.x - e.m_vPos.x, 0.0f, vSelf.z - e.m_vPos.z };
            float fLen = Math::Vec3_Length(&vToSelf);
            if (fLen > 0.001f)
                Math::Vec3_Scale(&vToSelf, &vToSelf, (1.0f / fLen));

            if (Math::Vec3_Dot(&vFacing, &vToSelf) < 0.3f)
                continue;

            // one roll per attack that starts
            if (Rand01() < fSkill)
            {
                ai.m_fGuardTime = (e.m_bBoss ? 0.7f : 0.5f);
                ai.m_nPlan = 0;
                rCmd.m_bGuard = true;
                ai.m_state = AISTATE_GUARD;
                return;
            };
        };
    };

    // ---- fight -------------------------------------------------------------
    if (pTarget)
    {
        ai.m_state = AISTATE_FIGHT;

        float fDist = DistXZ(vSelf, pTarget->m_vPos);
        float fReach = (1.3f + pTarget->m_fRadius);

        // continue the combo plan
        if (IsAttackStatus(status))
        {
            if (ai.m_nPlan > 0)
            {
                if (ai.m_statusAtStep == PLAYERTYPES::STATUS_INVALID)
                {
                    // first frame of the string: the press that started it is used
                    ai.m_statusAtStep = status;
                    ai.m_iStep = 1;
                }
                else if (status != ai.m_statusAtStep)
                {
                    // the chain moved on: the queued press was used
                    ++ai.m_iStep;
                    ai.m_statusAtStep = status;
                };

                if (ai.m_iStep < ai.m_nPlan)
                    rCmd.m_iAttack = ai.m_aiPlan[ai.m_iStep];
            };

            // steer every hit at the target
            MoveTo(rCmd, vSelf, pTarget->m_vPos, false);
            return;
        };

        if (ai.m_nPlan > 0)
        {
            // the string has ended (or was interrupted)
            ai.m_nPlan = 0;
            float fAggro = Max(Tune(TUNE_AGGRESSION), 0.05f);
            ai.m_fAttackCooldown = (0.45f * (0.6f + (Rand01() * 0.8f))) / fAggro;
        };

        if (!IsFreeStatus(status))
            return;

        // shuriken at range
        if (Tc().m_bShuriken &&
            (fDist > 5.0f) && (fDist < 15.0f) &&
            (ai.m_fKnifeCooldown <= 0.0f) &&
            rPlayerChr.IsEnableAttackKnife() &&
            (CGameProperty::Player(nSlot)->GetShurikenNum() > 0))
        {
            ai.m_fKnifeCooldown = 2.5f;
            if (Rand01() < (0.35f * Tune(TUNE_AGGRESSION)))
            {
                MoveTo(rCmd, vSelf, pTarget->m_vPos, false);
                rCmd.m_bKnife = true;
                return;
            };
        };

        if (fDist > fReach)
        {
            if (bWantJump && (ai.m_fStuckTime >= 1.0f))
            {
                ai.m_bJumping = true;
                ai.m_fJumpTime = 0.0f;
                ai.m_bJump2nd = false;
                MoveTo(rCmd, vSelf, pTarget->m_vPos, true);
                rCmd.m_bJump = true;
                return;
            };

            ai.m_bWantMove = true;
            MoveTo(rCmd, vSelf, pTarget->m_vPos, (fDist > (fReach + 2.0f)));
            return;
        };

        // in reach: start a string when ready, otherwise hold position facing it
        if (ai.m_fAttackCooldown <= 0.0f)
        {
            PickPlan(ai, pTarget->m_bBoss);
            ai.m_iStep = 0;
            ai.m_statusAtStep = PLAYERTYPES::STATUS_INVALID;
            rCmd.m_iAttack = ai.m_aiPlan[0];
            MoveTo(rCmd, vSelf, pTarget->m_vPos, false);
            return;
        };

        return;
    };

    // ---- follow -------------------------------------------------------------
    ai.m_state = AISTATE_FOLLOW;

    if (bWantJump)
    {
        ai.m_bJumping = true;
        ai.m_fJumpTime = 0.0f;
        ai.m_bJump2nd = false;
        MoveTo(rCmd, vSelf, vLeader, true);
        rCmd.m_bJump = true;
        return;
    };

    if (!IsFreeStatus(status) && !IsAirStatus(status))
        return;

    int32 nOrder = 0;
    for (int32 i = 0; i < nSlot; ++i)
    {
        if (i != team.m_iHuman)
            ++nOrder;
    };

    RwV3d vSpot = Math::VECTOR3_ZERO;
    FormationPoint(nOrder, *pLeader, vSpot);
    float fSpotDist = DistXZ(vSelf, vSpot);

    // start moving when out of place, stop once back in place
    float fStart = 1.4f;
    float fStop  = 0.6f;
    if (ai.m_bMoving)
        ai.m_bMoving = (fSpotDist > fStop);
    else
        ai.m_bMoving = (fSpotDist > fStart);

    if (ai.m_bMoving)
    {
        ai.m_bWantMove = true;
        MoveTo(rCmd, vSelf, vSpot, ((fSpotDist > 3.0f) || (fLeaderDist > (Tune(TUNE_FOLLOW_DIST) + 2.5f))));
        if (fLeaderDist > 8.0f)
            ai.m_state = AISTATE_CATCHUP;
    };
};


/*static*/ bool CTurtleCompanions::CanRetailSwitch(const CPlayerCharacter& rPlayerChr, bool& rbResult)
{
    if (!IsInstantSwitch() || IsActive())
        return false;

    if (rPlayerChr.TestAttribute(PLAYERTYPES::ATTRIBUTE_CONFUSION))
    {
        rbResult = false;
        return true;
    };

    // any grounded state the player controls; not in the air (the next
    // turtle would start idle in mid-air), not hurt, not carrying an enemy
    PLAYERTYPES::STATUS s = rPlayerChr.GetStatus();
    rbResult = (IsFreeStatus(s) ||
                ((s >= PLAYERTYPES::STATUS_DASH_CANCEL) && (s <= PLAYERTYPES::STATUS_DASH_FINISH)) ||
                IsGuardStatus(s) ||
                (IsAttackStatus(s) && (s != PLAYERTYPES::STATUS_ATTACK_JUMP) && (s != PLAYERTYPES::STATUS_ATTACK_KNIFE_JUMP)) ||
                (s == PLAYERTYPES::STATUS_TOUCHDOWN) ||
                (s == PLAYERTYPES::STATUS_PUSH));
    return true;
};


//
//  Debug Tools page
//
/*static*/ int32 CTurtleCompanions::GetItemNum(void)
{
    return ITEM_NUM;
};


/*static*/ bool CTurtleCompanions::IsNumericItem(int32 index)
{
    return ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST));
};


/*static*/ const char* CTurtleCompanions::GetItemHeader(int32 index)
{
    switch (index)
    {
    case ITEM_ENABLE:      return "AI COMPANIONS AND INSTANT SWITCHING";
    case ITEM_TUNE_FIRST:  return "COMPANION AI (LIVE)";
    case ITEM_SLOT_FIRST:  return "TEAM IN THIS STAGE (SELECT A ROW TO TAKE CONTROL)";
    default:               return nullptr;
    };
};


/*static*/ void CTurtleCompanions::FormatItem(int32 index, char* pszBuffer, size_t capacity)
{
    STATE& tc = Tc();

    // Label and value are separated by two or more spaces and a switch value
    // is exactly ON / OFF: the Debug Tools menu relies on both.
    if (index == ITEM_ENABLE)
    {
        std::snprintf(pszBuffer, capacity, "%-29s  %s", "AI companions (F9)", tc.m_bEnabled ? "ON" : "OFF");
    }
    else if (index == ITEM_INSTANT)
    {
        std::snprintf(pszBuffer, capacity, "%-29s  %s", "Instant switching", tc.m_bInstant ? "ON" : "OFF");
    }
    else if (index == ITEM_FILL)
    {
        std::snprintf(pszBuffer, capacity, "%-29s  %s", "Team", tc.m_bFillTeam ? "All four turtles" : "Selected only");
    }
    else if (index == ITEM_EFFECT)
    {
        std::snprintf(pszBuffer, capacity, "%-29s  %s", "Switch effect", tc.m_bSwitchEffect ? "ON" : "OFF");
    }
    else if (index == ITEM_SHURIKEN)
    {
        std::snprintf(pszBuffer, capacity, "%-29s  %s", "Companions throw shuriken", tc.m_bShuriken ? "ON" : "OFF");
    }
    else if ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST))
    {
        int32 t = index - ITEM_TUNE_FIRST;
        char szValue[32];
        std::snprintf(szValue, sizeof(szValue), s_aTuneInfo[t].m_pszFormat, tc.m_afTune[t]);
        std::snprintf(pszBuffer, capacity, "%-29s  %s", s_aTuneInfo[t].m_pszName, szValue);
    }
    else if ((index >= ITEM_SLOT_FIRST) && (index <= ITEM_SLOT_LAST))
    {
        int32 slot = index - ITEM_SLOT_FIRST;
        char szLabel[40];
        std::snprintf(szLabel, sizeof(szLabel), "Slot %d", slot + 1);

        const char* pszValue = "-";
        if (IsActive() && (slot < tc.m_team.m_nNum))
        {
            std::snprintf(szLabel, sizeof(szLabel), "%s", TurtleName(tc.m_team.m_aId[slot]));
            pszValue = ((slot == tc.m_team.m_iHuman) ? "YOU" : s_apszAIState[tc.m_team.m_aAI[slot].m_state]);
        };

        std::snprintf(pszBuffer, capacity, "%-29s  %s", szLabel, pszValue);
    }
    else if (index == ITEM_RESET)
    {
        std::snprintf(pszBuffer, capacity, "%s", "Reset companion AI amounts");
    }
    else if (capacity > 0)
    {
        pszBuffer[0] = '\0';
    };
};


/*static*/ void CTurtleCompanions::GetItemHelp(int32 index, const char*& rpszScope, const char*& rpszDescription)
{
    if (index == ITEM_ENABLE)
    {
        rpszScope = "NEXT STAGE START - SINGLE PLAYER";
        rpszDescription = "ON: in single-player story stages the whole team is on the field and the turtles you do not control fight beside you. OFF: retail, one turtle at a time. F9 switches at any time; it takes effect when the next stage (or a retry) starts. Not used in co-op, Battle Nexus, vehicle stages, the lair or Combat Lab.";
    }
    else if (index == ITEM_INSTANT)
    {
        rpszScope = "LIVE";
        rpszDescription = "Change Character (L2 / LT by default) works at once. With companions: control jumps to the next turtle on the field, from any state, even mid-combo or while hit. Without companions: the retail swap is allowed while walking, running, dashing, guarding or attacking, not only when standing still. OFF = retail rule (standing still only).";
    }
    else if (index == ITEM_FILL)
    {
        rpszScope = "NEXT STAGE START";
        rpszDescription = "All four turtles: turtles you did not pick at character select join the team as companions (up to four characters). Selected only: just the characters you picked; with one character there are no companions.";
    }
    else if (index == ITEM_EFFECT)
    {
        rpszScope = "LIVE";
        rpszDescription = "Plays the retail character-change flash on the turtle you switch to.";
    }
    else if (index == ITEM_SHURIKEN)
    {
        rpszScope = "LIVE";
        rpszDescription = "Companions throw shuriken at enemies 5 to 15 units away, from their own shuriken count.";
    }
    else if ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST))
    {
        rpszScope = "LIVE AMOUNT";
        rpszDescription = s_aTuneInfo[index - ITEM_TUNE_FIRST].m_pszHelp;
    }
    else if ((index >= ITEM_SLOT_FIRST) && (index <= ITEM_SLOT_LAST))
    {
        rpszScope = "LIVE";
        rpszDescription = "YOU = the turtle you control. AI - FOLLOW / FIGHT / GUARD / CATCH UP / RECOVER / JUMP = what the companion is doing. Press A / Enter or an arrow on a companion's row to take control of it.";
    }
    else if (index == ITEM_RESET)
    {
        rpszScope = "LIVE";
        rpszDescription = "Puts every amount in this section back to its default.";
    };
};


/*static*/ void CTurtleCompanions::AdjustItem(int32 index, int32 direction, int32 multiplier)
{
    STATE& tc = Tc();

    if (index == ITEM_ENABLE)
    {
        SetEnabled(!tc.m_bEnabled);
    }
    else if (index == ITEM_INSTANT)
    {
        tc.m_bInstant = !tc.m_bInstant;
    }
    else if (index == ITEM_FILL)
    {
        tc.m_bFillTeam = !tc.m_bFillTeam;
    }
    else if (index == ITEM_EFFECT)
    {
        tc.m_bSwitchEffect = !tc.m_bSwitchEffect;
    }
    else if (index == ITEM_SHURIKEN)
    {
        tc.m_bShuriken = !tc.m_bShuriken;
    }
    else if ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST))
    {
        int32 t = index - ITEM_TUNE_FIRST;
        const TUNEINFO& rInfo = s_aTuneInfo[t];

        // snap to the step grid so repeated presses never drift
        float fSteps = std::floor((tc.m_afTune[t] / rInfo.m_fStep) + 0.5f);
        fSteps += static_cast<float>(direction * Max(multiplier, 1));
        tc.m_afTune[t] = Clamp(fSteps * rInfo.m_fStep, rInfo.m_fMin, rInfo.m_fMax);
    }
    else if ((index >= ITEM_SLOT_FIRST) && (index <= ITEM_SLOT_LAST))
    {
        if (IsActive())
            SwitchTo(index - ITEM_SLOT_FIRST, true);
    }
    else if (index == ITEM_RESET)
    {
        ResetAll();
    };
};


/*static*/ void CTurtleCompanions::ResetItem(int32 index)
{
    STATE& tc = Tc();

    if ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST))
    {
        int32 t = index - ITEM_TUNE_FIRST;
        tc.m_afTune[t] = s_aTuneInfo[t].m_fDefault;
    }
    else if (index == ITEM_INSTANT)
    {
        tc.m_bInstant = true;
    }
    else if (index == ITEM_FILL)
    {
        tc.m_bFillTeam = true;
    }
    else if (index == ITEM_EFFECT)
    {
        tc.m_bSwitchEffect = true;
    }
    else if (index == ITEM_SHURIKEN)
    {
        tc.m_bShuriken = true;
    };
};


/*static*/ void CTurtleCompanions::ResetAll(void)
{
    for (int32 i = ITEM_TUNE_FIRST; i <= ITEM_TUNE_LAST; ++i)
        ResetItem(i);
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
