#include "CombatOverhaul.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/Component/Effect/EffectManager.hpp"
#include "Game/Component/Enemy/CharacterCompositor.hpp"
#include "Game/Component/Enemy/ConcreteAIModerator/BaseGeneralEnemyAI.hpp"
#include "Game/Component/Enemy/Enemy.hpp"
#include "Game/Component/Enemy/EnemyCharacter.hpp"
#include "Game/Component/GameMain/GamePlayer.hpp"
#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/GameMain/GameStage.hpp"
#include "Game/Component/Player/PlayerCharacter.hpp"
#include "Game/System/GameObject/GameObjectManager.hpp"
#include "Game/System/Hit/HitAttackData.hpp"
#include "Game/System/Hit/HitAttackManager.hpp"
#include "Game/Component/Module/BarrierModule.hpp"
#include "Game/System/Misc/Gamepad.hpp"

#if defined(TARGET_PC)
#include "System/PC/PCModFeatures.hpp"
#endif /* defined(TARGET_PC) */

#include <cstdio>
#include <cstring>


namespace
{
    enum
    {
        PLAYER_MAX      = GAMETYPES::PLAYERS_MAX,
        ENEMYSTATE_MAX  = 48,
        INDICATOR_MAX   = 24,
        MARKER_MAX      = 96,
        RALLYHIT_MAX    = 16,
    };

    // ---- tuning (all times in seconds, gauges are 0..1) ----

    // 1 arrow dodge
    static const float DODGE_FINISH_GRACE       = 0.12f;    // part of the dash recovery that still dodges

    // 2 Michelangelo reflect
    static const float MIC_SHIELD_LIVE          = 1.0e6f;   // the shield stays up while he guards

    // 4 posture
    static const float PLR_POSTURE_GUARD_POWER  = 0.70f;    // strong hits (power 5..20) add up to +70%
    static const float PLR_POSTURE_DECAY_WAIT   = 2.0f;
    static const float PLR_POSTURE_DECAY        = 0.20f;    // per second, not broken
    static const float PLR_POSTURE_BROKEN_DECAY = 0.04f;    // per second while broken (safety net)
    static const float PLR_POSTURE_HIT_RECOVER  = 0.20f;    // per landed hit while broken
    static const float PLR_POSTURE_PARRY_RECOVER= 0.25f;
    static const float ENM_POSTURE_DECAY_WAIT   = 3.0f;
    static const float ENM_POSTURE_DECAY        = 0.25f;

    // 4 combo break
    static const float PLR_BURST_DECAY_WAIT     = 4.0f;
    static const float PLR_BURST_DECAY          = 0.15f;
    static const float PLR_BURST_RADIUS         = 3.0f;
    static const float PLR_BURST_INVINCIBLE     = 0.8f;
    static const float ENM_BURST_DECAY_WAIT     = 3.0f;
    static const float ENM_BURST_DECAY          = 0.20f;
    static const float ENM_BURST_RADIUS         = 2.5f;
    static const float ENM_BURST_COOLDOWN       = 6.0f;

    // 5 attack indicator
    static const float INDICATOR_AFTER_TIME     = 0.6f;     // mark stays this long after the wind-up
    static const float WINDUP_MOTION_SPEED      = 0.08f;    // enemy motion speed during the warning

    // 8 rally
    static const float RALLY_DRAIN_TIME         = 0.8f;     // the regainable part empties this fast once the window ends

    // 9 guard recovery
    static const float GUARD_BACK_TIME_RETAIL   = 0.4f;
    static const float GUARD_REAR_HALF_ANGLE    = MATH_DEG2RAD(15.0f);  // 30 degree blind arc behind

    // 11 / 14 / 15 targeting
    static const float LOCK_RANGE               = 16.0f;
    static const float LOCK_BREAK_RANGE         = 24.0f;
    static const float STEER_RATE_RETAIL        = MATH_PI05;        //  90 deg/s
    static const float STEER_RATE_FREE          = MATH_PI * 3.0f;   // 540 deg/s
    static const float STEER_RATE_TARGET        = MATH_PI * 2.0f;   // 360 deg/s


    struct PLAYERSTATE
    {
        // 4 posture / combo break
        float m_fPosture;
        float m_fPostureTime;
        bool  m_bPostureBroken;
        float m_fBurst;
        float m_fBurstTime;
        bool  m_bBurstFire;

        // 6 parry crystal
        int32 m_iCrystal;
        float m_fCrystalUntil;

        // 11 / 15 targeting
        bool   m_bLockOn;
        uint32 m_hLockTarget;       // CEnemy object handle
        uint32 m_hSoftTarget;
        bool   m_bLockToggleRequest;

        // stick input of the last manipulator run
        bool  m_bMove;
        float m_fStickDirection;

        // 3 Michelangelo speed bookkeeping
        bool  m_bMotionSpeedApplied;

        // 11 right stick flick (edge detection for target switching)
        int32 m_iLockSwitchRequest;     // -1 screen-left, +1 screen-right
    };


    struct ENEMYSTATE
    {
        uint32 m_hChr;              // compositor (CCharacter) handle, 0 = free slot
        float  m_fPosture;
        float  m_fPostureTime;
        float  m_fBurst;
        float  m_fBurstTime;
        float  m_fBurstCooldownUntil;
        bool   m_bBurstFire;

        // 5 attack warning: the attack motion is held back while the mark shows
        bool   m_bWindup;
        float  m_fWindupUntil;
        int32  m_iWindupStatus;
    };


    struct INDICATOR
    {
        uint32 m_hOwner;            // CEnemy object handle, 0 = free slot
        float  m_fStart;
        bool   m_bHeavy;
    };


    struct ITEMINFO
    {
        const char* m_pszHeader;    // section header drawn above this row (or nullptr)
        const char* m_pszName;
        const char* m_pszScope;
        const char* m_pszHelp;
    };


    struct OVERHAUL
    {
        bool  m_bEnabled;
        bool  m_abFeature[CCombatOverhaul::FEATURE_NUM];
        float m_afTune[16];
        bool  m_bManualCamera;
        int32 m_iShieldColor;
        bool  m_bHud;
        bool  m_bBadgeAlways;
        float m_fBadgeClock;
        float m_fBadgeStamp;

        float m_fNow;
        float m_fLastPeriod;
        uint32 m_uRand;

        // 8 rally (the team shares one health gauge, so it shares one rally)
        //   every hit keeps its own clock; index 0 is the oldest hit
        float m_fRallyPool;         // health that can still be regained (sum of the hits below)
        int32 m_nRallyHit;
        float m_afRallyHitHP[RALLYHIT_MAX];     // what is left to regain from this hit
        float m_afRallyHitPeak[RALLYHIT_MAX];   // what this hit took
        float m_afRallyHitTime[RALLYHIT_MAX];   // when this hit landed

        PLAYERSTATE m_aPlayer[PLAYER_MAX];
        ENEMYSTATE  m_aEnemy[ENEMYSTATE_MAX];
        INDICATOR   m_aIndicator[INDICATOR_MAX];

        CCombatOverhaul::MARKERINFO m_aMarker[MARKER_MAX];
        int32 m_nMarker;
    };


    static OVERHAUL s_ov;
    static bool s_bOvInitialized = false;


    enum ITEM
    {
        ITEM_MASTER = 0,
        ITEM_FEATURE_FIRST,
        ITEM_FEATURE_LAST = ITEM_FEATURE_FIRST + CCombatOverhaul::FEATURE_NUM - 1,
        ITEM_TUNE_FIRST,
        ITEM_TUNE_LAST = ITEM_TUNE_FIRST + 12,
        ITEM_SHIELD_COLOR,
        ITEM_HUD,
        ITEM_BADGE,

        ITEM_NUM,
    };


    static const ITEMINFO s_aFeatureInfo[CCombatOverhaul::FEATURE_NUM] =
    {
        { "PROJECTILES",
          "1. Dash dodges arrows",
          "PLAYERS - DASH",
          "Arrows, shuriken and other shots pass through you during a dash (and the first moments of its recovery). Retail: a dash can still be shot." },
        { nullptr,
          "2. Mikey 360 guard reflect",
          "MICHELANGELO - GUARD",
          "While Michelangelo guards, an orange barrier surrounds him: arrows, shuriken and energy shots (gunners, mechs) from any side are sent back at the enemy. It only affects projectiles; enemies walk through it unharmed. Retail: only his attack swings reflect." },
        { "COMBOS AND MOVEMENT",
          "3. Mikey faster combos",
          "MICHELANGELO - A/B CHAIN",
          "Plays Michelangelo's ground combo faster (speed below); hit timing and chain windows scale with it. Retail speed is 1.00x." },
        { "POSTURE AND COMBO BREAK",
          "4a. Posture break",
          "PLAYERS AND ENEMIES",
          "Guarding fills a posture gauge. Enemy: full gauge = guard break and stagger (next hit is a counter). You: full gauge = guard stops working until you empty it by landing hits." },
        { nullptr,
          "4b. Combo break",
          "PLAYERS AND ENEMIES",
          "Taking hits in a row fills a break gauge. When yours is full press Guard + Jump to blast every nearby enemy away. Enemies use theirs automatically when you juggle them too long." },
        { "READABILITY",
          "5. Attack indicator",
          "ENEMIES",
          "Shows a mark over an enemy as it commits to an attack (cyan = normal, red = heavy, combo, throw, boss) and holds the attack back for the Attack warning time below, so the mark comes before the hit." },
        { "REWARDS",
          "6. Parry gives a crystal",
          "PLAYERS - GUARD IMPACT",
          "A successful parry (guard just before the hit) grants one random crystal power (red attack, green defence, white shuriken, orange charge) for the time below. Also makes the parry available without the defence upgrade." },
        { "COMBOS AND MOVEMENT",
          "7. Don mobile shield",
          "DONATELLO - CHARGE ATTACK",
          "Donatello's barrier stays up for its full time while he is free to move, attack and combo. Retail: he stands still until the barrier ends." },
        { "REWARDS",
          "8. Rally",
          "PLAYERS",
          "Bloodborne rally: the health a hit takes stays on the health gauge as a faded part. Hit enemies before it drains away to win it back. More hits add to it; each hit has its own window." },
        { "COMBOS AND MOVEMENT",
          "9. Fast guard recovery",
          "PLAYERS - GUARD",
          "Blocked hits push you back for half as long, and you can attack, dash, jump or move straight out of guard without waiting for the guard to lower." },
        { nullptr,
          "10. Automatic quick rise",
          "PLAYERS - KNOCKDOWN",
          "After a knockdown you roll back to your feet at once, as if you had pressed attack/jump on landing. Retail: you lie on the ground unless you press in time." },
        { "TARGETING",
          "11. Lock-on",
          "PLAYERS - R3 / KEY T",
          "Press R3 (right stick click) or T to lock onto the enemy you face; attacks and shuriken aim at it. Flick the right stick left / right to move the lock to the next enemy on that side. Press R3 again to release. Off while Camera > Manual camera is ON." },
        { "COMBOS AND MOVEMENT",
          "12. Dash follows the stick",
          "PLAYERS - DASH",
          "A dash goes where the stick or movement keys point, also when cancelling an attack into a dash. Retail: it goes where the character happens to face." },
        { nullptr,
          "13. Attack out of dash",
          "PLAYERS - DASH",
          "Attack, shuriken, jump and guard work at once after a dash (a press during the dash is remembered). The strong attack and its charge can be dash-cancelled at any time; run attack, shuriken throw and the final combo hit after the strike." },
        { nullptr,
          "14. Free combo steering",
          "PLAYERS - A/B CHAIN",
          "Each combo hit turns to where you hold the stick, and you can steer quickly in between. Retail: 90 degrees per second, so strings run in a straight line." },
        { "TARGETING",
          "15. Smart targeting",
          "PLAYERS - ATTACKS",
          "Each attack turns to the best enemy: in front of you, in reach and closest to where you aim. Shuriken do the same over a much longer range. After a defeat the next hit glides to a new target. The stick always has priority." },
        { nullptr,
          "16. Guard faces the attacker",
          "PLAYERS - GUARD",
          "While you guard, a hit from any side except a 30 degree arc straight behind you is blocked and your character turns to face it. Retail: guard covers the front only." },
        { nullptr,
          "17. Wider combo window",
          "PLAYERS - A/B CHAIN",
          "The next combo press (weak-weak, AAB, AAC...) is still accepted for the Combo window time below after the normal moment. Retail checks one single instant, so a slightly late press is dropped." },
    };


    enum TUNE
    {
        TUNE_ENM_POSTURE_HITS = 0,
        TUNE_PLR_POSTURE_HITS,
        TUNE_PLR_BURST_HITS,
        TUNE_ENM_BURST_HITS,
        TUNE_WARN_TIME,
        TUNE_GUARD_BACK,
        TUNE_SOFT_RANGE,
        TUNE_SHOT_RANGE,
        TUNE_COMBO_WINDOW,
        TUNE_MIC_SPEED,
        TUNE_CRYSTAL_TIME,
        TUNE_RALLY_TIME,
        TUNE_RALLY_REGAIN,

        TUNE_NUM,
    };


    struct TUNEINFO
    {
        const char* m_pszName;
        const char* m_pszFormat;    // printf format of the value
        const char* m_pszHelp;
        float m_fMin;
        float m_fMax;
        float m_fStep;
        float m_fDefault;
    };


    static const TUNEINFO s_aTuneInfo[TUNE_NUM] =
    {
        { "Posture: enemy blocks",    "%.0f hits",
          "4a. How many of your hits an enemy can block before its guard breaks (2 - 10, default 4). Lower = breaks faster.",
          2.0f, 10.0f, 1.0f, 4.0f },
        { "Posture: your blocks",     "%.0f hits",
          "4a. How many normal hits you can block before your posture breaks (3 - 20, default 6). Heavy hits count for more.",
          3.0f, 20.0f, 1.0f, 6.0f },
        { "Combo break: hits taken",  "%.0f hits",
          "4b. Hits you must take in a row before Guard + Jump is ready (2 - 12, default 4).",
          2.0f, 12.0f, 1.0f, 4.0f },
        { "Combo break: enemy hits",  "%.0f hits",
          "4b. Unblocked hits in a row an enemy takes before it blasts free (4 - 30, default 8).",
          4.0f, 30.0f, 1.0f, 8.0f },
        { "Attack warning time",      "%.2f s",
          "5. How long an enemy is held at the start of its attack while the mark shows, so the mark comes before the hit (0.00 - 0.80, default 0.30). 0 = mark only, no delay.",
          0.0f, 0.80f, 0.05f, 0.30f },
        { "Block pushback time",      "%.2f s",
          "9. How long a blocked hit pushes you back before you can act (0.05 - 0.40, retail 0.40, default 0.20).",
          0.05f, 0.40f, 0.05f, 0.20f },
        { "Smart targeting range",    "%.1f",
          "15. How far a melee attack looks for an enemy to turn to (3.0 - 12.0, default 5.5).",
          3.0f, 12.0f, 0.5f, 5.5f },
        { "Shuriken targeting range", "%.0f",
          "15. How far a shuriken throw looks for an enemy to aim at (5 - 40, default 20).",
          5.0f, 40.0f, 1.0f, 20.0f },
        { "Combo window",             "%.2f s",
          "17. How long after the normal moment the next combo press is still accepted (0.00 - 0.60, default 0.30).",
          0.0f, 0.60f, 0.05f, 0.30f },
        { "Mikey combo speed",        "%.2fx",
          "3. Animation speed of Michelangelo's ground combo (1.00 - 1.60, default 1.25).",
          1.0f, 1.60f, 0.05f, 1.25f },
        { "Parry crystal time",       "%.0f s",
          "6. How long the crystal power from a parry lasts (3 - 15, default 6).",
          3.0f, 15.0f, 1.0f, 6.0f },
        { "Rally window",             "%.1f s",
          "8. How long the health lost to a hit stays regainable, counted from that hit, before it drains away (1.0 - 10.0, default 5.0).",
          1.0f, 10.0f, 0.5f, 5.0f },
        { "Rally regain per hit",     "%.0f %%",
          "8. Health won back per hit, as a share of the damage that hit deals (25 - 300, default 100).",
          25.0f, 300.0f, 25.0f, 100.0f },
    };


    struct SHIELDCOLOR
    {
        const char* m_pszName;
        RwRGBA m_color;     // alpha = opacity of the shield (0x80 = 50%)
    };


    static const SHIELDCOLOR s_aShieldColor[] =
    {
        { "Orange", { 0xFF, 0x8C, 0x1E, 0x80 } },
        { "Red",    { 0xFF, 0x30, 0x28, 0x80 } },
        { "Yellow", { 0xFF, 0xE0, 0x30, 0x80 } },
        { "Green",  { 0x40, 0xF0, 0x50, 0x80 } },
        { "Cyan",   { 0x30, 0xE8, 0xFF, 0x80 } },
        { "Blue",   { 0x40, 0x70, 0xFF, 0x80 } },
        { "Purple", { 0xB0, 0x50, 0xFF, 0x80 } },
        { "Pink",   { 0xFF, 0x60, 0xC0, 0x80 } },
        { "White",  { 0xFF, 0xFF, 0xFF, 0x80 } },
    };


    static const ITEMINFO s_aExtraInfo[] =
    {
        { "DISPLAY",
          "Gauges and markers",
          "DISPLAY TOGGLE",
          "Shows posture / combo break gauges, rally and crystal timers, attack indicators and target markers. Off hides the display only; the mechanics keep working." },
        { nullptr,
          "BEFORE / AFTER badge",
          "DISPLAY MODE",
          "Shows which combat is active at the top of the screen: for a few seconds after switching (F6), or always, for side-by-side video." },
    };


    static void ResetPlayerState(PLAYERSTATE& p)
    {
        std::memset(&p, 0x00, sizeof(p));
        p.m_iCrystal = -1;
    };


    // Enemies held in an attack wind-up get their motion speed back before
    // their state is dropped.
    static void ReleaseWindups(void)
    {
        for (int32 i = 0; i < ENEMYSTATE_MAX; ++i)
        {
            ENEMYSTATE& e = s_ov.m_aEnemy[i];
            if (!e.m_hChr || !e.m_bWindup)
                continue;

            e.m_bWindup = false;

            CGameObject* pObj = CGameObjectManager::GetObject(e.m_hChr);
            if (pObj && (pObj->GetType() == GAMEOBJECTTYPE::CHARACTER))
                static_cast<CCharacter*>(pObj)->ClearMotionCtrlTime();
        };
    };


    static void ResetRuntime(void)
    {
        ReleaseWindups();

        for (int32 i = 0; i < PLAYER_MAX; ++i)
            ResetPlayerState(s_ov.m_aPlayer[i]);

        std::memset(s_ov.m_aEnemy, 0x00, sizeof(s_ov.m_aEnemy));
        std::memset(s_ov.m_aIndicator, 0x00, sizeof(s_ov.m_aIndicator));
        s_ov.m_nMarker = 0;
        s_ov.m_fRallyPool = 0.0f;
        s_ov.m_nRallyHit = 0;
    };


    static void SetDefaults(void)
    {
        s_ov.m_bEnabled = true;
#if defined(TARGET_PC)
        // The overhaul ships inside the Debug Tools mod: with that mod
        // switched off in the manager this EXE plays retail combat.
        s_ov.m_bEnabled = CPCModFeatures::IsDebugToolsEnabled();
#endif /* defined(TARGET_PC) */
        for (int32 i = 0; i < CCombatOverhaul::FEATURE_NUM; ++i)
            s_ov.m_abFeature[i] = true;

        for (int32 i = 0; i < TUNE_NUM; ++i)
            s_ov.m_afTune[i] = s_aTuneInfo[i].m_fDefault;
        s_ov.m_bHud         = true;
        s_ov.m_bBadgeAlways = false;
    };


    static OVERHAUL& Ov(void)
    {
        if (!s_bOvInitialized)
        {
            s_bOvInitialized = true;
            std::memset(&s_ov, 0x00, sizeof(s_ov));
            SetDefaults();
            ResetRuntime();
            s_ov.m_uRand = 0x2F6E2B1u;
            s_ov.m_fBadgeStamp = -1000.0f;
        };

        return s_ov;
    };


    static float Now(void)
    {
        return Ov().m_fNow;
    };


    static float Tune(TUNE tune)
    {
        return Ov().m_afTune[tune];
    };


    // "N hits fill the gauge" -> gain per hit.
    static float PerHit(TUNE tune)
    {
        float fHits = Tune(tune);
        return (fHits > 0.5f ? (1.0f / fHits) : 1.0f);
    };


    static float IndicatorTime(void)
    {
        return (Tune(TUNE_WARN_TIME) + INDICATOR_AFTER_TIME);
    };


    // Private generator: never touches the game's random sequence.
    static uint32 OvRand(void)
    {
        OVERHAUL& ov = Ov();
        ov.m_uRand = (ov.m_uRand * 1664525u) + 1013904223u;
        return (ov.m_uRand >> 8);
    };


    static PLAYERSTATE* PlayerState(int32 nPlayerNo)
    {
        if ((nPlayerNo < 0) || (nPlayerNo >= PLAYER_MAX))
            return nullptr;

        return &Ov().m_aPlayer[nPlayerNo];
    };


    static PLAYERSTATE* PlayerState(const CPlayerCharacter& rPlayerChr)
    {
        return PlayerState(rPlayerChr.GetPlayerNo());
    };


    static ENEMYSTATE* EnemyState(uint32 hChr, bool bCreate)
    {
        if (!hChr)
            return nullptr;

        OVERHAUL& ov = Ov();
        ENEMYSTATE* pFree = nullptr;
        for (int32 i = 0; i < ENEMYSTATE_MAX; ++i)
        {
            if (ov.m_aEnemy[i].m_hChr == hChr)
                return &ov.m_aEnemy[i];

            if (!ov.m_aEnemy[i].m_hChr && !pFree)
                pFree = &ov.m_aEnemy[i];
        };

        if (!bCreate || !pFree)
            return nullptr;

        std::memset(pFree, 0x00, sizeof(*pFree));
        pFree->m_hChr = hChr;
        pFree->m_fPostureTime = -1000.0f;
        pFree->m_fBurstTime = -1000.0f;
        pFree->m_fBurstCooldownUntil = -1000.0f;
        return pFree;
    };


    static bool IsBoss(ENEMYID::VALUE id)
    {
        return ((id >= ENEMYID::BOSSBEGIN) && (id < ENEMYID::BOSSEND));
    };


    static CEnemy* EnemyFromHandle(uint32 hOwner)
    {
        if (!hOwner)
            return nullptr;

        CGameObject* pObj = CGameObjectManager::GetObject(hOwner);
        if (!pObj || (pObj->GetType() != GAMEOBJECTTYPE::ENEMY))
            return nullptr;

        return static_cast<CEnemy*>(pObj);
    };


    static bool IsEnemyTargetable(CEnemy* pEnemy)
    {
        if (!pEnemy)
            return false;

        CEnemyCharacter& rEnemyChr = pEnemy->Character();
        if (rEnemyChr.TestFlag(ENEMYTYPES::FLAG_DEATH_STATUS))
            return false;

        ENEMYTYPES::STATUS status = rEnemyChr.GetStatus();
        return ((status != ENEMYTYPES::STATUS_HIDE) &&
                (status != ENEMYTYPES::STATUS_QUIT) &&
                (status != ENEMYTYPES::STATUS_DEATH));
    };


    static void FacingVector(const CCharacter& rChr, RwV3d& rvOut)
    {
        RwV3d vAxisZ = Math::VECTOR3_AXIS_Z;
        rChr.RotateVectorByDirection(&rvOut, &vAxisZ);
        rvOut.y = 0.0f;
    };


    static void DirectionVector(float fDirection, RwV3d& rvOut)
    {
        RwMatrix matrix;
        RwMatrixSetIdentity(&matrix);
        Math::Matrix_RotateY(&matrix, fDirection);

        RwV3d vAxisZ = Math::VECTOR3_AXIS_Z;
        RwV3dTransformVector(&rvOut, &vAxisZ, &matrix);
        rvOut.y = 0.0f;
    };


    // Signed turn (radians) that makes the character face vTo; same maths as
    // CCharacter::SetDirectionFromVector.
    static float SignedAngleTo(const CCharacter& rChr, const RwV3d& rvTo)
    {
        RwV3d vTo = rvTo;
        vTo.y = 0.0f;
        if (Math::Vec3_Length(&vTo) <= 0.0001f)
            return 0.0f;

        Math::Vec3_Normalize(&vTo, &vTo);

        RwMatrix matrix;
        RwMatrixSetIdentity(&matrix);
        Math::Matrix_RotateY(&matrix, rChr.GetDirection());

        RwV3d vFacing = Math::VECTOR3_ZERO;
        Math::Vec3_Normalize(&vFacing, &matrix.at);

        RwV3d vLocal = Math::VECTOR3_ZERO;
        Math::Matrix_RotateY(&matrix, -rChr.GetDirection());
        RwV3dTransformVector(&vLocal, &vTo, &matrix);

        float fAngle = Math::ACos(Math::Vec3_Dot(&vTo, &vFacing));
        if (vLocal.x < 0.0f)
            fAngle = -fAngle;

        return fAngle;
    };


    static void TurnToward(CPlayerCharacter& rPlayerChr, const RwV3d& rvTo, float fRate)
    {
        float fAngle = SignedAngleTo(rPlayerChr, rvTo);
        if (fRate > 0.0f)
        {
            float fMax = fRate * CGameProperty::GetElapsedTime();
            fAngle = Clamp(fAngle, -fMax, fMax);
        };

        if (fAngle != 0.0f)
            rPlayerChr.SetDirection(rPlayerChr.GetDirection() + fAngle);
    };


    // Best enemy around vDesired (unit, XZ). Lower score wins: mostly the angle
    // to the aim direction, then the distance; the current target is sticky.
    static uint32 PickTarget(
        const CPlayerCharacter& rPlayerChr,
        const RwV3d& rvDesired,
        float fRange,
        float fCosCone,
        uint32 hPrefer
    )
    {
        RwV3d vPos = Math::VECTOR3_ZERO;
        rPlayerChr.GetPosition(&vPos);

        uint32 hBest = 0;
        float fBestScore = 1000.0f;

        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!IsEnemyTargetable(pEnemy))
                continue;

            RwV3d vEnemy = Math::VECTOR3_ZERO;
            pEnemy->GetPosition(&vEnemy);

            RwV3d vTo = Math::VECTOR3_ZERO;
            Math::Vec3_Sub(&vTo, &vEnemy, &vPos);
            float fHeightDiff = (vTo.y < 0.0f ? -vTo.y : vTo.y);
            vTo.y = 0.0f;

            float fDist = Math::Vec3_Length(&vTo);
            if ((fDist > fRange) || (fHeightDiff > 3.0f))
                continue;

            float fCos = 1.0f;
            if (fDist > 0.05f)
            {
                Math::Vec3_Scale(&vTo, &vTo, (1.0f / fDist));
                fCos = Math::Vec3_Dot(&vTo, &rvDesired);
            };

            if (fCos < fCosCone)
                continue;

            float fScore = ((1.0f - fCos) * 0.6f) + ((fDist / fRange) * 0.4f);
            if (pEnemy->GetHandle() == hPrefer)
                fScore -= 0.15f;

            if (fScore < fBestScore)
            {
                fBestScore = fScore;
                hBest = pEnemy->GetHandle();
            };
        };

        return hBest;
    };


    static bool VectorToEnemy(const CPlayerCharacter& rPlayerChr, uint32 hOwner, RwV3d& rvTo, float* pfDist)
    {
        CEnemy* pEnemy = EnemyFromHandle(hOwner);
        if (!IsEnemyTargetable(pEnemy))
            return false;

        RwV3d vPos = Math::VECTOR3_ZERO;
        rPlayerChr.GetPosition(&vPos);

        RwV3d vEnemy = Math::VECTOR3_ZERO;
        pEnemy->GetPosition(&vEnemy);

        Math::Vec3_Sub(&rvTo, &vEnemy, &vPos);
        rvTo.y = 0.0f;

        if (pfDist)
            *pfDist = Math::Vec3_Length(&rvTo);

        return true;
    };


    // Enemy next to the locked one on screen: side > 0 = to its right,
    // side < 0 = to its left. With nobody on that side it wraps around to
    // the farthest enemy on the other side.
    static uint32 PickLockNeighbour(const CPlayerCharacter& rPlayerChr, uint32 hCurrent, int32 side)
    {
        CEnemy* pCurrent = EnemyFromHandle(hCurrent);
        if (!pCurrent)
            return 0;

        RwV3d vPlayer = Math::VECTOR3_ZERO;
        rPlayerChr.GetPosition(&vPlayer);

        RwV3d vCurrent = Math::VECTOR3_ZERO;
        pCurrent->GetPosition(&vCurrent);

        // The camera frame's right vector points to screen-left (the same
        // convention the movement stick uses).
        RwMatrix matCamera;
        CGameProperty::GetCameraFrameMatrix(&matCamera);
        RwV3d vScreenRight = matCamera.right;
        vScreenRight.y = 0.0f;
        if (Math::Vec3_Length(&vScreenRight) <= 0.0001f)
            return 0;

        Math::Vec3_Normalize(&vScreenRight, &vScreenRight);
        Math::Vec3_Negate(&vScreenRight, &vScreenRight);

        uint32 hNear = 0;
        float fNear = 1.0e9f;
        uint32 hWrap = 0;
        float fWrap = 0.0f;

        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!IsEnemyTargetable(pEnemy) || (pEnemy == pCurrent))
                continue;

            RwV3d vEnemy = Math::VECTOR3_ZERO;
            pEnemy->GetPosition(&vEnemy);

            RwV3d vFromPlayer = Math::VECTOR3_ZERO;
            Math::Vec3_Sub(&vFromPlayer, &vEnemy, &vPlayer);
            vFromPlayer.y = 0.0f;
            if (Math::Vec3_Length(&vFromPlayer) > LOCK_RANGE)
                continue;

            RwV3d vOffset = Math::VECTOR3_ZERO;
            Math::Vec3_Sub(&vOffset, &vEnemy, &vCurrent);
            float fSide = Math::Vec3_Dot(&vOffset, &vScreenRight) * static_cast<float>(side);

            if (fSide > 0.0f)
            {
                if (fSide < fNear)
                {
                    fNear = fSide;
                    hNear = pEnemy->GetHandle();
                };
            }
            else if (fSide < fWrap)
            {
                fWrap = fSide;
                hWrap = pEnemy->GetHandle();
            };
        };

        return (hNear ? hNear : hWrap);
    };


    static bool IsGroundChainStatus(PLAYERTYPES::STATUS status)
    {
        return ((status >= PLAYERTYPES::STATUS_ATTACK_A) && (status <= PLAYERTYPES::STATUS_ATTACK_AABBC));
    };


    static bool IsGuardStatus(PLAYERTYPES::STATUS status)
    {
        return ((status == PLAYERTYPES::STATUS_GUARD_READY) ||
                (status == PLAYERTYPES::STATUS_GUARD) ||
                (status == PLAYERTYPES::STATUS_GUARD_BACK));
    };


    static bool IsShotAttack(const CHitAttackData& rAttack)
    {
        CGameObject* pAttacker = rAttack.GetObject();
        return (pAttacker && (pAttacker->GetType() == GAMEOBJECTTYPE::SHOT));
    };


    static CPlayerCharacter* PlayerAttacker(const CHitAttackData& rAttack)
    {
        CGameObject* pAttacker = rAttack.GetObject();
        if (!pAttacker || (pAttacker->GetType() != GAMEOBJECTTYPE::CHARACTER))
            return nullptr;

        CCharacter* pAttackerChr = static_cast<CCharacter*>(pAttacker);
        if (pAttackerChr->GetAttackCharacterType() != CCharacter::TYPE_PLAYER)
            return nullptr;

        return static_cast<CPlayerCharacter*>(pAttackerChr);
    };


    static void RegisterBlast(CCharacter& rChr, float fRadius, int32 nPower, CHitAttackData::TARGET target)
    {
        RwV3d vBody = Math::VECTOR3_ZERO;
        rChr.GetBodyPosition(&vBody);

        RwSphere sphere = {};
        sphere.center = vBody;
        sphere.radius = fRadius;

        CHitAttackData hitAttack;
        hitAttack.SetObject(rChr.GetHandle());
        hitAttack.SetObjectPos(&vBody);
        hitAttack.SetShape(CHitAttackData::SHAPE_SPHERE);
        hitAttack.SetSphere(&sphere);
        hitAttack.SetPower(nPower);
        hitAttack.SetTarget(target);
        hitAttack.SetAntiguard(CHitAttackData::ANTIGUARD_INVALID);
        hitAttack.SetMotion("OvBurst");
        hitAttack.SetStatus(CHitAttackData::STATUS_FLYAWAY);

        CHitAttackManager::RegistAttack(&hitAttack);
    };


    static void AddIndicator(uint32 hOwner, bool bHeavy)
    {
        if (!hOwner)
            return;

        OVERHAUL& ov = Ov();
        INDICATOR* pSlot = nullptr;
        for (int32 i = 0; i < INDICATOR_MAX; ++i)
        {
            if (ov.m_aIndicator[i].m_hOwner == hOwner)
            {
                pSlot = &ov.m_aIndicator[i];
                break;
            };

            if (!ov.m_aIndicator[i].m_hOwner && !pSlot)
                pSlot = &ov.m_aIndicator[i];
        };

        if (!pSlot)
            return;

        pSlot->m_hOwner = hOwner;
        pSlot->m_fStart = Now();
        pSlot->m_bHeavy = bHeavy;
    };


    static void PushMarker(CCombatOverhaul::MARKER type, const RwV3d& rvPos, float fValue, float fValue2)
    {
        OVERHAUL& ov = Ov();
        if (ov.m_nMarker >= MARKER_MAX)
            return;

        CCombatOverhaul::MARKERINFO& rInfo = ov.m_aMarker[ov.m_nMarker++];
        rInfo.m_type    = type;
        rInfo.m_vPos    = rvPos;
        rInfo.m_fValue  = fValue;
        rInfo.m_fValue2 = fValue2;
    };


    static void EnemyHeadPosition(CEnemy* pEnemy, RwV3d& rvHead, RwV3d& rvBody)
    {
        pEnemy->GetPosition(&rvHead);
        rvBody = rvHead;

        float fHeight = pEnemy->Character().Compositor().GetCollisionParameter().m_fHeight;
        rvHead.y += fHeight;
        rvBody.y += (fHeight * 0.55f);
    };


    static void BreakPlayerPosture(CPlayerCharacter& rPlayerChr, PLAYERSTATE& p)
    {
        p.m_fPosture = 1.0f;
        p.m_bPostureBroken = true;

        RwV3d vBody = Math::VECTOR3_ZERO;
        rPlayerChr.GetBodyPosition(&vBody);
        CEffectManager::Play("all_guardbreak", &vBody);

        IGamepad::StartVibration(rPlayerChr.GetPadID(), IGamepad::VIBRATIONTYPE_HARD, 0.3f);
    };
}; /* namespace */


// ---------------------------------------------------------------------------
//  master switch, features, menu rows
// ---------------------------------------------------------------------------


/*static*/ bool CCombatOverhaul::IsEnabled(void)
{
    return Ov().m_bEnabled;
};


/*static*/ void CCombatOverhaul::SetEnabled(bool bEnable)
{
    OVERHAUL& ov = Ov();
    if (ov.m_bEnabled == bEnable)
        return;

    ov.m_bEnabled = bEnable;
    ov.m_fBadgeStamp = ov.m_fBadgeClock;

    // Leave no overhaul state behind when going back to retail: gauges,
    // locks and pending bursts are dropped. Crystal powers already granted
    // run out on their retail timers.
    ResetRuntime();
};


/*static*/ bool CCombatOverhaul::IsFeatureSwitchOn(FEATURE feature)
{
    if ((feature < 0) || (feature >= FEATURE_NUM))
        return false;

    return Ov().m_abFeature[feature];
};


/*static*/ void CCombatOverhaul::SetFeatureSwitch(FEATURE feature, bool bOn)
{
    if ((feature < 0) || (feature >= FEATURE_NUM))
        return;

    OVERHAUL& ov = Ov();
    ov.m_abFeature[feature] = bOn;

    if (!bOn)
    {
        for (int32 i = 0; i < PLAYER_MAX; ++i)
        {
            PLAYERSTATE& p = ov.m_aPlayer[i];
            switch (feature)
            {
            case FEATURE_POSTURE:
                p.m_fPosture = 0.0f;
                p.m_bPostureBroken = false;
                break;

            case FEATURE_COMBO_BREAK:
                p.m_fBurst = 0.0f;
                p.m_bBurstFire = false;
                break;

            case FEATURE_RALLY:
                Ov().m_fRallyPool = 0.0f;
                Ov().m_nRallyHit = 0;
                break;

            case FEATURE_LOCK_ON:
                p.m_bLockOn = false;
                p.m_hLockTarget = 0;
                break;

            case FEATURE_SMART_TARGET:
                p.m_hSoftTarget = 0;
                break;

            default:
                break;
            };
        };

        if ((feature == FEATURE_POSTURE) || (feature == FEATURE_COMBO_BREAK))
        {
            for (int32 i = 0; i < ENEMYSTATE_MAX; ++i)
            {
                if (feature == FEATURE_POSTURE)
                    ov.m_aEnemy[i].m_fPosture = 0.0f;
                else
                {
                    ov.m_aEnemy[i].m_fBurst = 0.0f;
                    ov.m_aEnemy[i].m_bBurstFire = false;
                };
            };
        };

        if (feature == FEATURE_ATTACK_INDICATOR)
        {
            ReleaseWindups();
            std::memset(ov.m_aIndicator, 0x00, sizeof(ov.m_aIndicator));
        };
    };
};


/*static*/ bool CCombatOverhaul::IsOn(FEATURE feature)
{
    OVERHAUL& ov = Ov();
    if ((feature == FEATURE_LOCK_ON) && ov.m_bManualCamera)
        return false;

    return (ov.m_bEnabled && (feature >= 0) && (feature < FEATURE_NUM) && ov.m_abFeature[feature]);
};


/*static*/ float CCombatOverhaul::GetBadgeTime(void)
{
    // The badge clock is advanced by the overlay (it also runs while the
    // game is paused under the menu), one call per drawn frame.
    OVERHAUL& ov = Ov();
    ov.m_fBadgeClock += (1.0f / 60.0f);
    return (ov.m_fBadgeClock - ov.m_fBadgeStamp);
};


/*static*/ bool CCombatOverhaul::IsHudEnabled(void)
{
    return Ov().m_bHud;
};


/*static*/ bool CCombatOverhaul::IsBadgeAlways(void)
{
    return Ov().m_bBadgeAlways;
};


/*static*/ int32 CCombatOverhaul::GetItemNum(void)
{
    return ITEM_NUM;
};


static_assert((ITEM_TUNE_LAST - ITEM_TUNE_FIRST + 1) == TUNE_NUM, "menu rows must match the tuning table");
namespace
{
    static float RallySum(const OVERHAUL& ov)
    {
        float fSum = 0.0f;
        for (int32 i = 0; i < ov.m_nRallyHit; ++i)
        {
            if (ov.m_afRallyHitHP[i] > 0.0f)
                fSum += ov.m_afRallyHitHP[i];
        };
        return fSum;
    };


    //  Takes fAmount out of the regainable health, oldest hit first
    static void RallyTrim(OVERHAUL& ov, float fAmount)
    {
        for (int32 i = 0; (i < ov.m_nRallyHit) && (fAmount > 0.0f); ++i)
        {
            if (ov.m_afRallyHitHP[i] <= 0.0f)
                continue;

            float fTake = ((ov.m_afRallyHitHP[i] < fAmount) ? ov.m_afRallyHitHP[i] : fAmount);
            ov.m_afRallyHitHP[i] -= fTake;
            fAmount -= fTake;
        };
    };


    //  Drops the hits that have nothing left
    static void RallyCompact(OVERHAUL& ov)
    {
        int32 n = 0;
        for (int32 i = 0; i < ov.m_nRallyHit; ++i)
        {
            if (ov.m_afRallyHitHP[i] < 0.5f)
                continue;

            ov.m_afRallyHitHP[n]   = ov.m_afRallyHitHP[i];
            ov.m_afRallyHitPeak[n] = ov.m_afRallyHitPeak[i];
            ov.m_afRallyHitTime[n] = ov.m_afRallyHitTime[i];
            ++n;
        };
        ov.m_nRallyHit = n;
    };
};


static_assert(TUNE_NUM <= 16, "OVERHAUL::m_afTune is too small");


/*static*/ bool CCombatOverhaul::IsNumericItem(int32 index)
{
    return ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST));
};


/*static*/ const char* CCombatOverhaul::GetItemHeader(int32 index)
{
    switch (index)
    {
    case ITEM_MASTER:        return "BEFORE / AFTER";
    case ITEM_FEATURE_FIRST: return "FEATURES (NUMBERED AS IN THE IDEA LIST)";
    case ITEM_TUNE_FIRST:    return "AMOUNTS (THE NUMBER IS THE FEATURE IT BELONGS TO)";
    case ITEM_SHIELD_COLOR:  return "DISPLAY";
    default:                 return nullptr;
    };
};


/*static*/ void CCombatOverhaul::FormatItem(int32 index, char* pszBuffer, size_t capacity)
{
    OVERHAUL& ov = Ov();

    // Label and value are separated by two or more spaces and a switch value
    // is exactly ON / OFF: the Debug Tools menu relies on both.
    if (index == ITEM_MASTER)
    {
        std::snprintf(pszBuffer, capacity, "Combat (F6)                    %s",
                      ov.m_bEnabled ? "AFTER - overhaul" : "BEFORE - retail");
    }
    else if ((index >= ITEM_FEATURE_FIRST) && (index <= ITEM_FEATURE_LAST))
    {
        int32 f = index - ITEM_FEATURE_FIRST;
        std::snprintf(pszBuffer, capacity, "%-29s  %s",
                      s_aFeatureInfo[f].m_pszName,
                      ov.m_abFeature[f] ? "ON" : "OFF");
    }
    else if ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST))
    {
        int32 t = index - ITEM_TUNE_FIRST;
        char szValue[32];
        std::snprintf(szValue, sizeof(szValue), s_aTuneInfo[t].m_pszFormat, ov.m_afTune[t]);
        std::snprintf(pszBuffer, capacity, "%-29s  %s", s_aTuneInfo[t].m_pszName, szValue);
    }
    else if (index == ITEM_SHIELD_COLOR)
    {
        std::snprintf(pszBuffer, capacity, "%-29s  %s", "Mikey shield colour", s_aShieldColor[ov.m_iShieldColor].m_pszName);
    }
    else if (index == ITEM_HUD)
    {
        std::snprintf(pszBuffer, capacity, "%-29s  %s", s_aExtraInfo[0].m_pszName, ov.m_bHud ? "ON" : "OFF");
    }
    else if (index == ITEM_BADGE)
    {
        std::snprintf(pszBuffer, capacity, "%-29s  %s", s_aExtraInfo[1].m_pszName,
                      ov.m_bBadgeAlways ? "ALWAYS" : "ON SWITCH");
    }
    else if (capacity > 0)
    {
        pszBuffer[0] = '\0';
    };
};


/*static*/ void CCombatOverhaul::GetItemHelp(int32 index, const char*& rpszScope, const char*& rpszDescription)
{
    if (index == ITEM_MASTER)
    {
        rpszScope = "LIVE - WHOLE GAME";
        rpszDescription = "BEFORE runs the retail combat exactly as shipped. AFTER enables every feature below that is ON. F6 switches at any time, also with this menu closed.";
    }
    else if ((index >= ITEM_FEATURE_FIRST) && (index <= ITEM_FEATURE_LAST))
    {
        rpszScope = s_aFeatureInfo[index - ITEM_FEATURE_FIRST].m_pszScope;
        rpszDescription = s_aFeatureInfo[index - ITEM_FEATURE_FIRST].m_pszHelp;
    }
    else if ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST))
    {
        rpszScope = "LIVE AMOUNT";
        rpszDescription = s_aTuneInfo[index - ITEM_TUNE_FIRST].m_pszHelp;
    }
    else if (index == ITEM_SHIELD_COLOR)
    {
        rpszScope = "LIVE - MICHELANGELO";
        rpszDescription = "2. Colour of Michelangelo's guard shield. Changes at once, also while the shield is up: Orange (default), Red, Yellow, Green, Cyan, Blue, Purple, Pink, White.";
    }
    else if ((index == ITEM_HUD) || (index == ITEM_BADGE))
    {
        rpszScope = s_aExtraInfo[index - ITEM_HUD].m_pszScope;
        rpszDescription = s_aExtraInfo[index - ITEM_HUD].m_pszHelp;
    };
};


/*static*/ void CCombatOverhaul::AdjustItem(int32 index, int32 direction, int32 multiplier)
{
    OVERHAUL& ov = Ov();

    if (index == ITEM_MASTER)
    {
        SetEnabled(!ov.m_bEnabled);
    }
    else if ((index >= ITEM_FEATURE_FIRST) && (index <= ITEM_FEATURE_LAST))
    {
        FEATURE feature = static_cast<FEATURE>(index - ITEM_FEATURE_FIRST);
        SetFeatureSwitch(feature, !ov.m_abFeature[feature]);
    }
    else if ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST))
    {
        int32 t = index - ITEM_TUNE_FIRST;
        const TUNEINFO& rInfo = s_aTuneInfo[t];

        // Snap to the step grid so repeated presses never drift.
        float fValue = ov.m_afTune[t] + (rInfo.m_fStep * static_cast<float>(direction * multiplier));
        float fSteps = (fValue - rInfo.m_fMin) / rInfo.m_fStep;
        fSteps = static_cast<float>(static_cast<int32>(fSteps + (fSteps >= 0.0f ? 0.5f : -0.5f)));
        ov.m_afTune[t] = Clamp(rInfo.m_fMin + (fSteps * rInfo.m_fStep), rInfo.m_fMin, rInfo.m_fMax);
    }
    else if (index == ITEM_SHIELD_COLOR)
    {
        int32 num = static_cast<int32>(sizeof(s_aShieldColor) / sizeof(s_aShieldColor[0]));
        int32 step = (direction < 0 ? -1 : 1);
        ov.m_iShieldColor = ((ov.m_iShieldColor + step) % num + num) % num;
    }
    else if (index == ITEM_HUD)
    {
        ov.m_bHud = !ov.m_bHud;
    }
    else if (index == ITEM_BADGE)
    {
        ov.m_bBadgeAlways = !ov.m_bBadgeAlways;
    };
};


/*static*/ void CCombatOverhaul::ResetItem(int32 index)
{
    OVERHAUL& ov = Ov();

    if (index == ITEM_MASTER)
        SetEnabled(true);
    else if ((index >= ITEM_FEATURE_FIRST) && (index <= ITEM_FEATURE_LAST))
        SetFeatureSwitch(static_cast<FEATURE>(index - ITEM_FEATURE_FIRST), true);
    else if ((index >= ITEM_TUNE_FIRST) && (index <= ITEM_TUNE_LAST))
        ov.m_afTune[index - ITEM_TUNE_FIRST] = s_aTuneInfo[index - ITEM_TUNE_FIRST].m_fDefault;
    else if (index == ITEM_SHIELD_COLOR)
        ov.m_iShieldColor = 0;
    else if (index == ITEM_HUD)
        ov.m_bHud = true;
    else if (index == ITEM_BADGE)
        ov.m_bBadgeAlways = false;
};


/*static*/ void CCombatOverhaul::ResetAll(void)
{
    for (int32 i = 0; i < ITEM_NUM; ++i)
        ResetItem(i);
};


/*static*/ void CCombatOverhaul::SetManualCamera(bool bManual)
{
    OVERHAUL& ov = Ov();
    ov.m_bManualCamera = bManual;

    if (bManual)
    {
        // The right stick belongs to the camera now: drop every lock.
        for (int32 i = 0; i < PLAYER_MAX; ++i)
        {
            ov.m_aPlayer[i].m_bLockOn = false;
            ov.m_aPlayer[i].m_hLockTarget = 0;
            ov.m_aPlayer[i].m_bLockToggleRequest = false;
            ov.m_aPlayer[i].m_iLockSwitchRequest = 0;
        };
    };
};


/*static*/ bool CCombatOverhaul::IsManualCamera(void)
{
    return Ov().m_bManualCamera;
};


/*static*/ float CCombatOverhaul::GetComboLateWindow(void)
{
    return (IsOn(FEATURE_COMBO_WINDOW) ? Tune(TUNE_COMBO_WINDOW) : 0.0f);
};


// ---------------------------------------------------------------------------
//  frame update
// ---------------------------------------------------------------------------


/*static*/ void CCombatOverhaul::Period(void)
{
    OVERHAUL& ov = Ov();

    if (!CGameStage::GetCurrent())
        return;

    float fNow = CGameProperty::GetTotalElapsedTime();
    if (fNow == ov.m_fLastPeriod)
        return;

    // A new stage restarts the stage clock: drop everything that referred
    // to the previous one.
    if (fNow < ov.m_fLastPeriod)
        ResetRuntime();

    ov.m_fLastPeriod = fNow;
    ov.m_fNow = fNow;

    if (!ov.m_bEnabled)
        return;

    float dt = CGameProperty::GetElapsedTime();

    int32 playerNum = CGameProperty::GetPlayerNum();

    // 8 rally: each hit's regainable health holds for the window counted
    // from that hit, then drains away
    if (ov.m_nRallyHit > 0)
    {
        if (!IsOn(FEATURE_RALLY))
        {
            ov.m_nRallyHit = 0;
        }
        else
        {
            float fWindow = Tune(TUNE_RALLY_TIME);
            for (int32 i = 0; i < ov.m_nRallyHit; ++i)
            {
                if (fNow > (ov.m_afRallyHitTime[i] + fWindow))
                    ov.m_afRallyHitHP[i] -= ((ov.m_afRallyHitPeak[i] / RALLY_DRAIN_TIME) * dt);
            };

            // never more than what is actually missing (pizza, revive, death)
            IGamePlayer* pGamePlayer = ((playerNum > 0) ? CGameProperty::Player(0) : nullptr);
            if (pGamePlayer)
            {
                float fMissing = static_cast<float>(pGamePlayer->GetHPMax() - pGamePlayer->GetHP());
                if ((pGamePlayer->GetHP() <= 0) || (fMissing <= 0.0f))
                    ov.m_nRallyHit = 0;
                else
                    RallyTrim(ov, RallySum(ov) - fMissing);
            };
        };

        RallyCompact(ov);
    };
    ov.m_fRallyPool = RallySum(ov);
    for (int32 i = 0; (i < playerNum) && (i < PLAYER_MAX); ++i)
    {
        PLAYERSTATE& p = ov.m_aPlayer[i];

        // posture
        if (p.m_bPostureBroken)
        {
            p.m_fPosture -= (PLR_POSTURE_BROKEN_DECAY * dt);
            if (p.m_fPosture <= 0.0f)
            {
                p.m_fPosture = 0.0f;
                p.m_bPostureBroken = false;
            };
        }
        else if ((p.m_fPosture > 0.0f) && ((fNow - p.m_fPostureTime) > PLR_POSTURE_DECAY_WAIT))
        {
            p.m_fPosture -= (PLR_POSTURE_DECAY * dt);
            if (p.m_fPosture < 0.0f)
                p.m_fPosture = 0.0f;
        };

        // combo break
        if ((p.m_fBurst > 0.0f) && ((fNow - p.m_fBurstTime) > PLR_BURST_DECAY_WAIT))
        {
            p.m_fBurst -= (PLR_BURST_DECAY * dt);
            if (p.m_fBurst < 0.0f)
                p.m_fBurst = 0.0f;
        };

        // crystal timer (display only; the power itself is a retail attribute)
        if ((p.m_iCrystal >= 0) && (fNow > p.m_fCrystalUntil))
            p.m_iCrystal = -1;

        // lock-on
        IGamePlayer* pGamePlayer = CGameProperty::Player(i);
        CPlayerCharacter* pPlayerChr = (pGamePlayer ? pGamePlayer->GetCurrentCharacter() : nullptr);
        if (!pPlayerChr || !IsOn(FEATURE_LOCK_ON))
        {
            p.m_bLockOn = false;
            p.m_hLockTarget = 0;
            p.m_bLockToggleRequest = false;
            p.m_iLockSwitchRequest = 0;
            continue;
        };

        RwV3d vFacing = Math::VECTOR3_ZERO;
        FacingVector(*pPlayerChr, vFacing);

        if (p.m_bLockToggleRequest)
        {
            p.m_bLockToggleRequest = false;

            if (p.m_bLockOn)
            {
                p.m_bLockOn = false;
                p.m_hLockTarget = 0;
            }
            else
            {
                RwV3d vDesired = vFacing;
                if (p.m_bMove)
                    DirectionVector(p.m_fStickDirection, vDesired);

                uint32 hTarget = PickTarget(*pPlayerChr, vDesired, LOCK_RANGE, -0.2f, 0);
                if (!hTarget)
                    hTarget = PickTarget(*pPlayerChr, vDesired, LOCK_RANGE, -1.0f, 0);

                if (hTarget)
                {
                    p.m_bLockOn = true;
                    p.m_hLockTarget = hTarget;
                };
            };
        };

        // Right stick flick: move the lock to the next enemy on that side
        // of the current one, as seen on screen.
        if (p.m_bLockOn && (p.m_iLockSwitchRequest != 0))
        {
            uint32 hNext = PickLockNeighbour(*pPlayerChr, p.m_hLockTarget, p.m_iLockSwitchRequest);
            if (hNext)
                p.m_hLockTarget = hNext;
        };

        p.m_iLockSwitchRequest = 0;

        if (p.m_bLockOn)
        {
            RwV3d vTo = Math::VECTOR3_ZERO;
            float fDist = 0.0f;
            bool bValid = VectorToEnemy(*pPlayerChr, p.m_hLockTarget, vTo, &fDist);
            if (!bValid || (fDist > LOCK_BREAK_RANGE))
            {
                // Smooth retarget after a defeat: keep the lock and move it to
                // the next best enemy around where the player faces.
                uint32 hNext = PickTarget(*pPlayerChr, vFacing, LOCK_RANGE, -1.0f, 0);
                if (hNext)
                {
                    p.m_hLockTarget = hNext;
                }
                else
                {
                    p.m_bLockOn = false;
                    p.m_hLockTarget = 0;
                };
            };
        };
    };

    // enemies
    for (int32 i = 0; i < ENEMYSTATE_MAX; ++i)
    {
        ENEMYSTATE& e = ov.m_aEnemy[i];
        if (!e.m_hChr)
            continue;

        if (!CGameObjectManager::GetObject(e.m_hChr))
        {
            e.m_hChr = 0;
            continue;
        };

        if ((e.m_fPosture > 0.0f) && ((fNow - e.m_fPostureTime) > ENM_POSTURE_DECAY_WAIT))
        {
            e.m_fPosture -= (ENM_POSTURE_DECAY * dt);
            if (e.m_fPosture < 0.0f)
                e.m_fPosture = 0.0f;
        };

        if (!e.m_bBurstFire && (e.m_fBurst > 0.0f) && ((fNow - e.m_fBurstTime) > ENM_BURST_DECAY_WAIT))
        {
            e.m_fBurst -= (ENM_BURST_DECAY * dt);
            if (e.m_fBurst < 0.0f)
                e.m_fBurst = 0.0f;
        };

        if ((e.m_fPosture <= 0.0f) && (e.m_fBurst <= 0.0f) && !e.m_bBurstFire && !e.m_bWindup &&
            (fNow > e.m_fBurstCooldownUntil))
        {
            e.m_hChr = 0;
        };
    };

    // attack indicators
    for (int32 i = 0; i < INDICATOR_MAX; ++i)
    {
        INDICATOR& ind = ov.m_aIndicator[i];
        if (ind.m_hOwner && ((fNow - ind.m_fStart) > IndicatorTime()))
            ind.m_hOwner = 0;
    };
};


// ---------------------------------------------------------------------------
//  player input side
// ---------------------------------------------------------------------------


/*static*/ void CCombatOverhaul::OnManipulatorInput(CPlayerCharacter& rPlayerChr, bool bMove, float fDirection)
{
    Period();

    PLAYERSTATE* p = PlayerState(rPlayerChr);
    if (!p)
        return;

    p->m_bMove = bMove;
    p->m_fStickDirection = fDirection;
};


/*static*/ void CCombatOverhaul::RequestLockOnToggle(int32 nPlayerNo)
{
    if (!IsOn(FEATURE_LOCK_ON))
        return;

    PLAYERSTATE* p = PlayerState(nPlayerNo);
    if (p)
        p->m_bLockToggleRequest = true;
};


/*static*/ void CCombatOverhaul::RequestLockSwitch(int32 nPlayerNo, int32 side)
{
    if (!IsOn(FEATURE_LOCK_ON) || (side == 0))
        return;

    PLAYERSTATE* p = PlayerState(nPlayerNo);
    if (p && p->m_bLockOn)
        p->m_iLockSwitchRequest = (side > 0 ? 1 : -1);
};


/*static*/ bool CCombatOverhaul::IsLockedOn(int32 nPlayerNo)
{
    PLAYERSTATE* p = PlayerState(nPlayerNo);
    return (p && p->m_bLockOn && IsOn(FEATURE_LOCK_ON));
};


/*static*/ bool CCombatOverhaul::TryPlayerBurst(CPlayerCharacter& rPlayerChr)
{
    if (!IsOn(FEATURE_COMBO_BREAK))
        return false;

    PLAYERSTATE* p = PlayerState(rPlayerChr);
    if (!p || (p->m_fBurst < 1.0f))
        return false;

    // Only from the ground: neutral, guard, attacks and hit reactions.
    PLAYERTYPES::STATUS status = rPlayerChr.GetStatus();
    bool bAllowed = false;
    switch (status)
    {
    case PLAYERTYPES::STATUS_IDLE:
    case PLAYERTYPES::STATUS_WALK:
    case PLAYERTYPES::STATUS_RUN:
    case PLAYERTYPES::STATUS_DASH_FINISH:
    case PLAYERTYPES::STATUS_GUARD_READY:
    case PLAYERTYPES::STATUS_GUARD:
    case PLAYERTYPES::STATUS_GUARD_BACK:
    case PLAYERTYPES::STATUS_GUARD_FINISH:
    case PLAYERTYPES::STATUS_KNOCK_FRONT:
    case PLAYERTYPES::STATUS_KNOCK_BACK:
    case PLAYERTYPES::STATUS_DINDLE:
    case PLAYERTYPES::STATUS_STUN:
        bAllowed = true;
        break;

    default:
        bAllowed = (IsGroundChainStatus(status) ||
                    (status == PLAYERTYPES::STATUS_ATTACK_B_CHARGE) ||
                    (status == PLAYERTYPES::STATUS_ATTACK_B));
        break;
    };

    if (!bAllowed)
        return false;

    p->m_fBurst = 0.0f;
    p->m_bBurstFire = true;

    rPlayerChr.ForceIdle();
    rPlayerChr.SetInvincibilityEnable(true, PLR_BURST_INVINCIBLE);

    RwV3d vBody = Math::VECTOR3_ZERO;
    rPlayerChr.GetBodyPosition(&vBody);
    CEffectManager::Play("all_guardbreak", &vBody);
    CEffectManager::Play("all_count", &vBody);

    IGamepad::StartVibration(rPlayerChr.GetPadID(), IGamepad::VIBRATIONTYPE_HARD, 0.25f);

    return true;
};


/*static*/ bool CCombatOverhaul::SteerAttack(CPlayerCharacter& rPlayerChr, bool bMove, float fDirection)
{
    bool bLock  = IsOn(FEATURE_LOCK_ON);
    bool bFree  = IsOn(FEATURE_FREE_COMBO_STEER);
    bool bSmart = IsOn(FEATURE_SMART_TARGET);
    if (!bLock && !bFree && !bSmart)
        return false;

    PLAYERSTATE* p = PlayerState(rPlayerChr);
    if (!p)
        return false;

    // A locked target owns the facing during a string.
    if (bLock && p->m_bLockOn)
    {
        RwV3d vTo = Math::VECTOR3_ZERO;
        if (VectorToEnemy(rPlayerChr, p->m_hLockTarget, vTo, nullptr))
        {
            TurnToward(rPlayerChr, vTo, STEER_RATE_FREE);
            return true;
        };
    };

    // The stick always has priority over soft targeting.
    if (bMove)
    {
        RwV3d vStick = Math::VECTOR3_ZERO;
        DirectionVector(fDirection, vStick);
        TurnToward(rPlayerChr, vStick, (bFree ? STEER_RATE_FREE : STEER_RATE_RETAIL));
        return true;
    };

    // No stick: glide toward the current soft target; when it is gone
    // (defeated, out of reach) pick the next one.
    if (bSmart)
    {
        RwV3d vTo = Math::VECTOR3_ZERO;
        float fDist = 0.0f;
        if (!VectorToEnemy(rPlayerChr, p->m_hSoftTarget, vTo, &fDist) || (fDist > (Tune(TUNE_SOFT_RANGE) + 1.0f)))
        {
            RwV3d vFacing = Math::VECTOR3_ZERO;
            FacingVector(rPlayerChr, vFacing);
            p->m_hSoftTarget = PickTarget(rPlayerChr, vFacing, Tune(TUNE_SOFT_RANGE), 0.0f, 0);
            if (!VectorToEnemy(rPlayerChr, p->m_hSoftTarget, vTo, &fDist))
                return true;
        };

        TurnToward(rPlayerChr, vTo, STEER_RATE_TARGET);
    };

    return true;
};


// ---------------------------------------------------------------------------
//  player character side
// ---------------------------------------------------------------------------


/*static*/ void CCombatOverhaul::OnPlayerRun(CPlayerCharacter& rPlayerChr)
{
    Period();

    OVERHAUL& ov = Ov();
    PLAYERSTATE* p = PlayerState(rPlayerChr);
    if (!p || !rPlayerChr.IsActive())
        return;

    PLAYERTYPES::STATUS status = rPlayerChr.GetStatus();

    // 3 Michelangelo combo speed. The flag is only ever cleared here when
    // this code set it, so charge / jump attack speed control stays retail.
    bool bWantSpeed = (IsOn(FEATURE_MIC_FAST_COMBO) &&
                       (rPlayerChr.GetID() == PLAYERID::ID_MIC) &&
                       IsGroundChainStatus(status));
    if (bWantSpeed)
    {
        rPlayerChr.SetMotionSpeed(Tune(TUNE_MIC_SPEED));
        rPlayerChr.SetCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        p->m_bMotionSpeedApplied = true;
    }
    else if (p->m_bMotionSpeedApplied)
    {
        p->m_bMotionSpeedApplied = false;
        if ((status != PLAYERTYPES::STATUS_ATTACK_B_CHARGE) &&
            (status != PLAYERTYPES::STATUS_ATTACK_JUMP))
        {
            rPlayerChr.ClearCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        };
    };

    // 2 Michelangelo: his guard shows a small orange barrier (the same
    // module as Donatello's, in its shot-reflect-only mode). It is lowered
    // here too when the feature or the overhaul is switched off.
    if (rPlayerChr.GetID() == PLAYERID::ID_MIC)
    {
        CBarrierModule* pShield = static_cast<CBarrierModule*>(rPlayerChr.GetModule(MODULETYPE::BARRIER));
        if (pShield && pShield->IsShotReflectOnly())
        {
            // Debug Tools colour row: recolour as soon as it changes.
            if (pShield->GetShieldColorId() != ov.m_iShieldColor)
                pShield->SetShotReflectOnly(s_aShieldColor[ov.m_iShieldColor].m_color, ov.m_iShieldColor);

            bool bWant = (IsOn(FEATURE_MIC_REFLECT_360) && IsGuardStatus(status));
            CBarrierModule::STATE shieldState = pShield->GetState();

            if (bWant && ((shieldState == CBarrierModule::STATE_SLEEP) || (shieldState == CBarrierModule::STATE_DISAPPEAR)))
            {
                pShield->SetLive(MIC_SHIELD_LIVE);
                pShield->Appear();
            }
            else if (!bWant && ((shieldState == CBarrierModule::STATE_APPEAR) || (shieldState == CBarrierModule::STATE_IDLE)))
            {
                pShield->Disappear();
            };
        };
    };

    if (!ov.m_bEnabled)
        return;

    // 4 combo break: the blast is registered from the character's own Run
    // so it is part of this frame's hit pass.
    if (p->m_bBurstFire)
    {
        p->m_bBurstFire = false;
        RegisterBlast(rPlayerChr, PLR_BURST_RADIUS, 5, CHitAttackData::TARGET_ENEMY);
    };
};


/*static*/ void CCombatOverhaul::OnAttackStart(CPlayerCharacter& rPlayerChr, bool bShot)
{
    bool bLock  = IsOn(FEATURE_LOCK_ON);
    bool bFree  = IsOn(FEATURE_FREE_COMBO_STEER);
    bool bSmart = IsOn(FEATURE_SMART_TARGET);
    if (!bLock && !bFree && !bSmart)
        return;

    PLAYERSTATE* p = PlayerState(rPlayerChr);
    if (!p)
        return;

    RwV3d vTo = Math::VECTOR3_ZERO;

    if (bLock && p->m_bLockOn && VectorToEnemy(rPlayerChr, p->m_hLockTarget, vTo, nullptr))
    {
        rPlayerChr.SetDirectionFromVector(&vTo);
        return;
    };

    RwV3d vDesired = Math::VECTOR3_ZERO;
    if (p->m_bMove)
        DirectionVector(p->m_fStickDirection, vDesired);
    else
        FacingVector(rPlayerChr, vDesired);

    if (bSmart)
    {
        // Stick held: only enemies close to where the stick points (about
        // 50 degrees each side). Neutral stick: anything in the front arc.
        // A shuriken looks much farther, in a narrower cone.
        float fCosCone = (p->m_bMove ? 0.64f : 0.17f);
        float fRange = Tune(TUNE_SOFT_RANGE);
        if (bShot)
        {
            fCosCone = (p->m_bMove ? 0.80f : 0.50f);
            fRange = Tune(TUNE_SHOT_RANGE);
        };

        uint32 hTarget = PickTarget(rPlayerChr, vDesired, fRange, fCosCone, (bShot ? 0 : p->m_hSoftTarget));
        if (!bShot)
            p->m_hSoftTarget = hTarget;

        if (hTarget && VectorToEnemy(rPlayerChr, hTarget, vTo, nullptr))
        {
            rPlayerChr.SetDirectionFromVector(&vTo);
            return;
        };
    };

    if (bFree && p->m_bMove)
        rPlayerChr.SetDirectionFromVector(&vDesired);
};


/*static*/ CHARACTERTYPES::DEFENCERSTATUSFLAG
CCombatOverhaul::AdjustDefenceFlag(
    CPlayerCharacter& rPlayerChr,
    const CHitAttackData& rAttack,
    CHARACTERTYPES::DEFENCERSTATUSFLAG defenceFlag
)
{
    if (!Ov().m_bEnabled)
        return defenceFlag;

    PLAYERTYPES::STATUS status = rPlayerChr.GetStatus();
    PLAYERSTATE* p = PlayerState(rPlayerChr);

    // 6 the parry is part of the reward loop: available without the upgrade
    if (IsOn(FEATURE_PARRY_CRYSTAL) && (status == PLAYERTYPES::STATUS_GUARD_READY))
        defenceFlag |= CHARACTERTYPES::DEFENCERSTATUSFLAG_STATUS_SHIFT;

    // 2 Michelangelo guards shots from every side
    if (IsOn(FEATURE_MIC_REFLECT_360) &&
        (rPlayerChr.GetID() == PLAYERID::ID_MIC) &&
        IsGuardStatus(status) &&
        IsShotAttack(rAttack))
    {
        defenceFlag |= CHARACTERTYPES::DEFENCERSTATUSFLAG_GUARD_ALLRANGE;
    };

    // 16 guard covers every side except a 30 degree arc straight behind,
    // and the character turns to face what it blocks
    if (IsOn(FEATURE_GUARD_AUTOFACE) && IsGuardStatus(status) && rAttack.GetObjectPos())
    {
        RwV3d vPos = Math::VECTOR3_ZERO;
        rPlayerChr.GetPosition(&vPos);

        RwV3d vTo = Math::VECTOR3_ZERO;
        Math::Vec3_Sub(&vTo, rAttack.GetObjectPos(), &vPos);
        vTo.y = 0.0f;

        if (Math::Vec3_Length(&vTo) > 0.05f)
        {
            float fAngle = SignedAngleTo(rPlayerChr, vTo);
            float fAbs = (fAngle < 0.0f ? -fAngle : fAngle);
            if (fAbs <= (MATH_PI - GUARD_REAR_HALF_ANGLE))
            {
                defenceFlag |= CHARACTERTYPES::DEFENCERSTATUSFLAG_GUARD_ALLRANGE;

                // Guard statuses fix the direction; lift that just for this turn.
                bool bFixed = rPlayerChr.TestCharacterFlag(CHARACTERTYPES::FLAG_FIXED_DIRECTION);
                if (bFixed)
                    rPlayerChr.ClearCharacterFlag(CHARACTERTYPES::FLAG_FIXED_DIRECTION);

                rPlayerChr.SetDirection(rPlayerChr.GetDirection() + fAngle);

                if (bFixed)
                    rPlayerChr.SetCharacterFlag(CHARACTERTYPES::FLAG_FIXED_DIRECTION);
            };
        };
    };

    // 4 a broken posture cannot guard or parry
    if (IsOn(FEATURE_POSTURE) && p && p->m_bPostureBroken)
    {
        defenceFlag &= ~(CHARACTERTYPES::DEFENCERSTATUSFLAG_GUARD |
                         CHARACTERTYPES::DEFENCERSTATUSFLAG_GUARD_ALLRANGE |
                         CHARACTERTYPES::DEFENCERSTATUSFLAG_GUARD_IMPACT);
    };

    return defenceFlag;
};


/*static*/ bool CCombatOverhaul::IsShotDodged(CPlayerCharacter& rPlayerChr, const CHitAttackData& rAttack)
{
    if (!IsOn(FEATURE_ARROW_DODGE) || !IsShotAttack(rAttack))
        return false;

    PLAYERTYPES::STATUS status = rPlayerChr.GetStatus();
    if ((status == PLAYERTYPES::STATUS_DASH) || (status == PLAYERTYPES::STATUS_DASH_CANCEL))
        return true;

    return ((status == PLAYERTYPES::STATUS_DASH_FINISH) &&
            (rPlayerChr.GetStatusDuration() < DODGE_FINISH_GRACE));
};


/*static*/ void CCombatOverhaul::OnPlayerDamageResult(
    CPlayerCharacter& rPlayerChr,
    CHitAttackData& rAttack,
    CHARACTERTYPES::ATTACKRESULTTYPE attackResult
)
{
    OVERHAUL& ov = Ov();
    if (!ov.m_bEnabled)
        return;

    PLAYERSTATE* p = PlayerState(rPlayerChr);
    if (!p)
        return;

    float fNow = Now();

    switch (attackResult)
    {
    case CHARACTERTYPES::ATTACKRESULTTYPE_GUARD:
        if (IsOn(FEATURE_POSTURE) && !p->m_bPostureBroken)
        {
            int32 nPower = rAttack.GetPower();
            float fRatio = Clamp(static_cast<float>(nPower - 5) / 15.0f, 0.0f, 1.0f);

            p->m_fPosture += (PerHit(TUNE_PLR_POSTURE_HITS) * (1.0f + (PLR_POSTURE_GUARD_POWER * fRatio)));
            p->m_fPostureTime = fNow;

            if (p->m_fPosture >= 0.999f)
                BreakPlayerPosture(rPlayerChr, *p);
        };
        break;

    case CHARACTERTYPES::ATTACKRESULTTYPE_GUARD_IMPACT:
        if (IsOn(FEATURE_POSTURE) && !p->m_bPostureBroken)
        {
            p->m_fPosture -= PLR_POSTURE_PARRY_RECOVER;
            if (p->m_fPosture < 0.0f)
                p->m_fPosture = 0.0f;
        };

        if (IsOn(FEATURE_PARRY_CRYSTAL))
        {
            int32 iCrystal = static_cast<int32>(OvRand() % 4u);
            rPlayerChr.ApplyParryCrystal(iCrystal, Tune(TUNE_CRYSTAL_TIME));

            p->m_iCrystal = iCrystal;
            p->m_fCrystalUntil = fNow + Tune(TUNE_CRYSTAL_TIME);
        };
        break;

    case CHARACTERTYPES::ATTACKRESULTTYPE_DAMAGE_KNOCK:
    case CHARACTERTYPES::ATTACKRESULTTYPE_DAMAGE_FLYAWAY:
    case CHARACTERTYPES::ATTACKRESULTTYPE_DINDLE:
    case CHARACTERTYPES::ATTACKRESULTTYPE_STUN:
        if (IsOn(FEATURE_COMBO_BREAK))
        {
            p->m_fBurst += PerHit(TUNE_PLR_BURST_HITS);
            if (p->m_fBurst >= 0.999f)
                p->m_fBurst = 1.0f;

            p->m_fBurstTime = fNow;
        };
        break;

    default:
        break;
    };
};


/*static*/ void CCombatOverhaul::OnPlayerReceivedDamage(CPlayerCharacter& rPlayerChr, int32 nDamage)
{
    if (!IsOn(FEATURE_RALLY) || (nDamage <= 0))
        return;

    PLAYERSTATE* p = PlayerState(rPlayerChr);
    IGamePlayer* pGamePlayer = CGameProperty::Player(rPlayerChr.GetPlayerNo());
    if (!p || !pGamePlayer)
        return;

    // The health this hit removes becomes regainable. Hits add up, and each
    // one keeps its own window, counted from the moment it landed.
    int32 nLost = Min(nDamage, pGamePlayer->GetHP());
    if (nLost <= 0)
        return;

    OVERHAUL& ov = Ov();
    RallyCompact(ov);
    if (ov.m_nRallyHit >= RALLYHIT_MAX)
    {
        // full: the two oldest hits share one clock (the older one's)
        ov.m_afRallyHitHP[0]   += ov.m_afRallyHitHP[1];
        ov.m_afRallyHitPeak[0] += ov.m_afRallyHitPeak[1];
        for (int32 i = 1; i < (ov.m_nRallyHit - 1); ++i)
        {
            ov.m_afRallyHitHP[i]   = ov.m_afRallyHitHP[i + 1];
            ov.m_afRallyHitPeak[i] = ov.m_afRallyHitPeak[i + 1];
            ov.m_afRallyHitTime[i] = ov.m_afRallyHitTime[i + 1];
        };
        --ov.m_nRallyHit;
    };

    int32 n = ov.m_nRallyHit++;
    ov.m_afRallyHitHP[n]   = static_cast<float>(nLost);
    ov.m_afRallyHitPeak[n] = static_cast<float>(nLost);
    ov.m_afRallyHitTime[n] = Now();
    ov.m_fRallyPool = RallySum(ov);
};


/*static*/ float CCombatOverhaul::GetAttackMotionSpeed(const CPlayerCharacter& rPlayerChr)
{
    if (IsOn(FEATURE_MIC_FAST_COMBO) &&
        (rPlayerChr.GetID() == PLAYERID::ID_MIC) &&
        IsGroundChainStatus(rPlayerChr.GetStatus()))
    {
        return Tune(TUNE_MIC_SPEED);
    };

    return 1.0f;
};


/*static*/ bool CCombatOverhaul::TryReflectProjectile(const RwV3d& rvPosition, float fRadius)
{
    if (!IsOn(FEATURE_MIC_REFLECT_360) || !CGameStage::GetCurrent())
        return false;

    int32 playerNum = CGameProperty::GetPlayerNum();
    for (int32 i = 0; i < playerNum; ++i)
    {
        IGamePlayer* pGamePlayer = CGameProperty::Player(i);
        CPlayerCharacter* pPlayerChr = (pGamePlayer ? pGamePlayer->GetCurrentCharacter() : nullptr);
        if (!pPlayerChr || (pPlayerChr->GetID() != PLAYERID::ID_MIC))
            continue;

        CBarrierModule* pShield = static_cast<CBarrierModule*>(pPlayerChr->GetModule(MODULETYPE::BARRIER));
        if (!pShield || !pShield->IsShotReflectOnly() || !pShield->IsRaised())
            continue;

        // The shield sphere is centred on the feet, where the module's
        // tracer (CPlayerTracer) puts it.
        RwV3d vCentre = Math::VECTOR3_ZERO;
        pPlayerChr->GetFootPosition(&vCentre);

        RwV3d vTo = Math::VECTOR3_ZERO;
        Math::Vec3_Sub(&vTo, &rvPosition, &vCentre);

        if (Math::Vec3_Length(&vTo) <= (pShield->GetCurrentRadius() + fRadius))
            return true;
    };

    return false;
};


/*static*/ float CCombatOverhaul::GetGuardBackTime(void)
{
    return (IsOn(FEATURE_GUARD_RECOVERY) ? Tune(TUNE_GUARD_BACK) : GUARD_BACK_TIME_RETAIL);
};


// ---------------------------------------------------------------------------
//  every hit
// ---------------------------------------------------------------------------


/*static*/ void CCombatOverhaul::OnCatchAttack(
    CCharacter& rTarget,
    CHitAttackData& rAttack,
    CHARACTERTYPES::ATTACKRESULTTYPE attackResult,
    int32 iDamage
)
{
    OVERHAUL& ov = Ov();
    if (!ov.m_bEnabled)
        return;

    if (rTarget.GetAttackCharacterType() != CCharacter::TYPE_ENEMY)
        return;

    if (attackResult == CHARACTERTYPES::ATTACKRESULTTYPE_INEFFECTIVE)
        return;

    CPlayerCharacter* pAttacker = PlayerAttacker(rAttack);
    if (!pAttacker)
        return;

    float fNow = Now();
    bool bGuarded = ((attackResult == CHARACTERTYPES::ATTACKRESULTTYPE_GUARD) ||
                     (attackResult == CHARACTERTYPES::ATTACKRESULTTYPE_GUARD_IMPACT));
    bool bOwnBlast = !std::strcmp(rAttack.GetMotion(), "OvBurst");

    PLAYERSTATE* p = PlayerState(*pAttacker);
    if (p && !bOwnBlast)
    {
        // 4 a broken posture is repaired by landing hits
        if (IsOn(FEATURE_POSTURE) && p->m_bPostureBroken && !bGuarded)
        {
            p->m_fPosture -= PLR_POSTURE_HIT_RECOVER;
            if (p->m_fPosture <= 0.0f)
            {
                p->m_fPosture = 0.0f;
                p->m_bPostureBroken = false;
            };
        };

        // 8 rally: part of the damage dealt comes back as health
        OVERHAUL& ov = Ov();
        if (IsOn(FEATURE_RALLY) && (ov.m_fRallyPool >= 1.0f) && (iDamage > 0))
        {
            IGamePlayer* pGamePlayer = CGameProperty::Player(pAttacker->GetPlayerNo());
            if (pGamePlayer && pGamePlayer->IsAlive())
            {
                int32 nPool = static_cast<int32>(ov.m_fRallyPool);
                int32 nRegain = static_cast<int32>((static_cast<float>(iDamage) * Tune(TUNE_RALLY_REGAIN) * 0.01f) + 0.5f);
                nRegain = Clamp(nRegain, 1, nPool);

                pGamePlayer->AddHP(nRegain);
                // the oldest hit, the one closest to running out, is won back first
                RallyTrim(ov, static_cast<float>(nRegain));
                RallyCompact(ov);
                ov.m_fRallyPool = RallySum(ov);
            };
        };
    };

    // 4 combo break for enemies: unguarded hits in a row fill their gauge
    if (IsOn(FEATURE_COMBO_BREAK) && !bGuarded && !bOwnBlast &&
        (attackResult != CHARACTERTYPES::ATTACKRESULTTYPE_THROW) &&
        (attackResult != CHARACTERTYPES::ATTACKRESULTTYPE_GUARD_BREAK))
    {
        CEnemyCharacter& rEnemyChr = static_cast<CCharacterCompositor&>(rTarget).EnemyChr();
        if (!rEnemyChr.TestFlag(ENEMYTYPES::FLAG_DEATH_STATUS))
        {
            ENEMYSTATE* e = EnemyState(rTarget.GetHandle(), true);
            if (e && (fNow > e->m_fBurstCooldownUntil) && !e->m_bBurstFire)
            {
                e->m_fBurst += PerHit(TUNE_ENM_BURST_HITS);
                e->m_fBurstTime = fNow;

                if (e->m_fBurst >= 0.999f)
                {
                    e->m_fBurst = 1.0f;
                    e->m_bBurstFire = true;
                };
            };
        };
    };
};


// ---------------------------------------------------------------------------
//  enemy side
// ---------------------------------------------------------------------------


/*static*/ bool CCombatOverhaul::OnEnemyGuard(CEnemyCharacter& rEnemyChr, CHitAttackData& rAttack)
{
    if (!IsOn(FEATURE_POSTURE))
        return false;

    if (!PlayerAttacker(rAttack) && !IsShotAttack(rAttack))
        return false;

    ENEMYSTATE* e = EnemyState(rEnemyChr.Compositor().GetHandle(), true);
    if (!e)
        return false;

    e->m_fPosture += PerHit(TUNE_ENM_POSTURE_HITS);
    e->m_fPostureTime = Now();

    if (e->m_fPosture < 0.999f)
        return false;

    // Posture broken: the caller turns this guard into a guard break
    // (stagger), so the next hit lands as a counter.
    e->m_fPosture = 0.0f;
    return true;
};


/*static*/ void CCombatOverhaul::OnEnemyStatus(
    CEnemyCharacter& rEnemyChr,
    ENEMYTYPES::STATUS statusNow,
    ENEMYTYPES::STATUS statusPrev
)
{
    if (!IsOn(FEATURE_ATTACK_INDICATOR))
        return;

    if (rEnemyChr.TestFlag(ENEMYTYPES::FLAG_DEATH_STATUS))
        return;

    bool bBoss = IsBoss(rEnemyChr.GetID());
    bool bAttackOrder = (rEnemyChr.IsAIModeratorExist() &&
                         (rEnemyChr.AIThinkOrder().GetOrder() == CAIThinkOrder::ORDER_ATTACK));

    bool bShow = false;
    bool bHeavy = bBoss;

    switch (statusNow)
    {
    case ENEMYTYPES::STATUS_ATTACK_A:
        bShow = true;
        if (!bBoss && bAttackOrder)
        {
            // Grunts decide the whole string up front (A, AA or AAA).
            int32 iType = (rEnemyChr.AIThinkOrder().OrderAttack().m_iAttackType & 0xFF);
            if ((iType == AIOT::AttackAA) || (iType == AIOT::AttackAAA))
                bHeavy = true;
        };
        break;

    case ENEMYTYPES::STATUS_ATTACK_AA:
    case ENEMYTYPES::STATUS_ATTACK_AAA:
        // Continuations of a string that was already announced.
        bShow = ((statusPrev != ENEMYTYPES::STATUS_ATTACK_A) &&
                 (statusPrev != ENEMYTYPES::STATUS_ATTACK_AA));
        bHeavy = true;
        break;

    case ENEMYTYPES::STATUS_FIRE_A:
    case ENEMYTYPES::STATUS_FIRE_B:
    case ENEMYTYPES::STATUS_FIRE_C:
        bShow = true;
        break;

    case ENEMYTYPES::STATUS_ATTACK_B:
    case ENEMYTYPES::STATUS_ATTACK_C:
    case ENEMYTYPES::STATUS_ATTACK_RUN:
    case ENEMYTYPES::STATUS_FIRE_AA:
    case ENEMYTYPES::STATUS_FIRE_BB:
    case ENEMYTYPES::STATUS_FIRE_CC:
    case ENEMYTYPES::STATUS_THROW:
    case ENEMYTYPES::STATUS_JUMP_ATTACK:
        bShow = true;
        bHeavy = true;
        break;

    default:
        // Boss / special moves live in the SPECIAL range.
        // Special statuses are also rolls, retreats, clones, taunts and
        // other moves that cannot hurt: only a special whose motion carries
        // attack hit data is announced.
        if ((statusNow >= ENEMYTYPES::STATUS_SPECIAL) && bAttackOrder &&
            (statusPrev != statusNow) &&
            (rEnemyChr.Compositor().GetMotionHitAttackNum() > 0))
        {
            bShow = true;
            bHeavy = true;
        };
        break;
    };

    if (!bShow)
        return;

    AddIndicator(rEnemyChr.GetOwner(), bHeavy);

    // Hold the attack back for the warning time by nearly freezing its
    // motion (hit timing follows motion time). Enemies that drive their own
    // motion speed in this status are left alone.
    float fWarn = Tune(TUNE_WARN_TIME);
    CCharacterCompositor& rCompositor = rEnemyChr.Compositor();
    if ((fWarn > 0.0f) && !rCompositor.TestCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL))
    {
        ENEMYSTATE* e = EnemyState(rCompositor.GetHandle(), true);
        if (e)
        {
            e->m_bWindup = true;
            e->m_fWindupUntil = Now() + fWarn;
            e->m_iWindupStatus = static_cast<int32>(statusNow);
            rCompositor.SetMotionCtrlTime(WINDUP_MOTION_SPEED);
        };
    };
};


/*static*/ void CCombatOverhaul::OnEnemyRun(CEnemyCharacter& rEnemyChr)
{
    Period();

    ENEMYSTATE* e = EnemyState(rEnemyChr.Compositor().GetHandle(), false);
    if (!e)
        return;

    // 5 end of the attack warning: give the motion its speed back when the
    // time is up or the enemy left the announced status (hit, guard break...)
    if (e->m_bWindup &&
        ((Now() >= e->m_fWindupUntil) || (static_cast<int32>(rEnemyChr.GetStatus()) != e->m_iWindupStatus)))
    {
        e->m_bWindup = false;
        rEnemyChr.Compositor().ClearMotionCtrlTime();
    };

    if (!IsOn(FEATURE_COMBO_BREAK) || !e->m_bBurstFire)
        return;

    if (rEnemyChr.TestFlag(ENEMYTYPES::FLAG_DEATH_STATUS))
    {
        e->m_bBurstFire = false;
        e->m_fBurst = 0.0f;
        return;
    };

    // Break out only from a standing hit reaction; while flying, lifted or
    // stunned the enemy has to wait (and loses the chance after the decay).
    ENEMYTYPES::STATUS status = rEnemyChr.GetStatus();
    if ((status != ENEMYTYPES::STATUS_KNOCK) &&
        (status != ENEMYTYPES::STATUS_KNOCK_FRONT) &&
        (status != ENEMYTYPES::STATUS_KNOCK_BACK))
    {
        if ((Now() - e->m_fBurstTime) > ENM_BURST_DECAY_WAIT)
        {
            e->m_bBurstFire = false;
            e->m_fBurst = 0.0f;
        };

        return;
    };

    e->m_bBurstFire = false;
    e->m_fBurst = 0.0f;
    e->m_fBurstCooldownUntil = Now() + ENM_BURST_COOLDOWN;

    if (!rEnemyChr.SetStatus(ENEMYTYPES::STATUS_THINKING))
        rEnemyChr.SetStatus(ENEMYTYPES::STATUS_IDLE);

    CCharacterCompositor& rCompositor = rEnemyChr.Compositor();
    RegisterBlast(rCompositor, ENM_BURST_RADIUS, 5, CHitAttackData::TARGET_PLAYER);

    RwV3d vBody = Math::VECTOR3_ZERO;
    rCompositor.GetBodyPosition(&vBody);
    CEffectManager::Play("all_guardbreak", &vBody);
    CEffectManager::Play("all_count", &vBody);
};


// ---------------------------------------------------------------------------
//  HUD data
// ---------------------------------------------------------------------------


/*static*/ int32 CCombatOverhaul::GetMarkerNum(void)
{
    OVERHAUL& ov = Ov();
    ov.m_nMarker = 0;

    if (!ov.m_bEnabled || !ov.m_bHud || !CGameStage::GetCurrent())
        return 0;

    float fNow = ov.m_fNow;

    // attack indicators
    for (int32 i = 0; i < INDICATOR_MAX; ++i)
    {
        INDICATOR& ind = ov.m_aIndicator[i];
        CEnemy* pEnemy = EnemyFromHandle(ind.m_hOwner);
        if (!IsEnemyTargetable(pEnemy))
            continue;

        RwV3d vHead = Math::VECTOR3_ZERO;
        RwV3d vBody = Math::VECTOR3_ZERO;
        EnemyHeadPosition(pEnemy, vHead, vBody);
        vHead.y += 0.35f;

        float fAge = Clamp((fNow - ind.m_fStart) / IndicatorTime(), 0.0f, 1.0f);
        PushMarker((ind.m_bHeavy ? MARKER_ATTACK_HEAVY : MARKER_ATTACK_LIGHT), vHead, fAge, 0.0f);
    };

    // enemy posture / combo break gauges
    int32 enemyMax = CGameProperty::GetEnemyMax();
    for (int32 i = 0; i < enemyMax; ++i)
    {
        CEnemy* pEnemy = CGameProperty::GetEnemy(i);
        if (!IsEnemyTargetable(pEnemy))
            continue;

        ENEMYSTATE* e = EnemyState(pEnemy->Character().Compositor().GetHandle(), false);
        if (!e || ((e->m_fPosture <= 0.0f) && (e->m_fBurst <= 0.0f)))
            continue;

        RwV3d vHead = Math::VECTOR3_ZERO;
        RwV3d vBody = Math::VECTOR3_ZERO;
        EnemyHeadPosition(pEnemy, vHead, vBody);
        vHead.y += 0.12f;

        PushMarker(MARKER_ENEMY_GAUGE, vHead, e->m_fPosture, e->m_fBurst);
    };

    // targets
    int32 playerNum = CGameProperty::GetPlayerNum();
    for (int32 i = 0; (i < playerNum) && (i < PLAYER_MAX); ++i)
    {
        PLAYERSTATE& p = ov.m_aPlayer[i];

        if (p.m_bLockOn)
        {
            CEnemy* pEnemy = EnemyFromHandle(p.m_hLockTarget);
            if (IsEnemyTargetable(pEnemy))
            {
                RwV3d vHead = Math::VECTOR3_ZERO;
                RwV3d vBody = Math::VECTOR3_ZERO;
                EnemyHeadPosition(pEnemy, vHead, vBody);
                PushMarker(MARKER_LOCKON, vBody, 0.0f, 0.0f);
            };
        }
        else if (p.m_hSoftTarget && IsOn(FEATURE_SMART_TARGET))
        {
            IGamePlayer* pGamePlayer = CGameProperty::Player(i);
            CPlayerCharacter* pPlayerChr = (pGamePlayer ? pGamePlayer->GetCurrentCharacter() : nullptr);
            CEnemy* pEnemy = EnemyFromHandle(p.m_hSoftTarget);

            // Shown only while the player is actually attacking it.
            if (pPlayerChr && IsEnemyTargetable(pEnemy) &&
                (pPlayerChr->GetStatus() >= PLAYERTYPES::STATUS_ATTACK_A) &&
                (pPlayerChr->GetStatus() <= PLAYERTYPES::STATUS_ATTACK_KNIFE_JUMP))
            {
                RwV3d vHead = Math::VECTOR3_ZERO;
                RwV3d vBody = Math::VECTOR3_ZERO;
                EnemyHeadPosition(pEnemy, vHead, vBody);
                PushMarker(MARKER_SOFT_TARGET, vBody, 0.0f, 0.0f);
            };
        };
    };

    return ov.m_nMarker;
};


/*static*/ bool CCombatOverhaul::GetMarker(int32 index, MARKERINFO& rInfo)
{
    OVERHAUL& ov = Ov();
    if ((index < 0) || (index >= ov.m_nMarker))
        return false;

    rInfo = ov.m_aMarker[index];
    return true;
};


/*static*/ bool CCombatOverhaul::GetPlayerHud(int32 nPlayerNo, PLAYERHUD& rHud)
{
    OVERHAUL& ov = Ov();
    if (!ov.m_bEnabled || !ov.m_bHud || !CGameStage::GetCurrent())
        return false;

    if ((nPlayerNo < 0) || (nPlayerNo >= PLAYER_MAX) || (nPlayerNo >= CGameProperty::GetPlayerNum()))
        return false;

    const PLAYERSTATE& p = ov.m_aPlayer[nPlayerNo];
    float fNow = ov.m_fNow;

    rHud.m_fPosture       = (IsOn(FEATURE_POSTURE) ? Clamp(p.m_fPosture, 0.0f, 1.0f) : -1.0f);
    rHud.m_bPostureBroken = p.m_bPostureBroken;
    rHud.m_fBurst         = (IsOn(FEATURE_COMBO_BREAK) ? Clamp(p.m_fBurst, 0.0f, 1.0f) : -1.0f);
    rHud.m_bBurstReady    = (IsOn(FEATURE_COMBO_BREAK) && (p.m_fBurst >= 1.0f));
    rHud.m_iCrystal       = p.m_iCrystal;
    rHud.m_fCrystalTime   = ((p.m_iCrystal >= 0) ? (p.m_fCrystalUntil - fNow) : 0.0f);
    rHud.m_bLockOn        = p.m_bLockOn;

    return true;
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */


/*static*/ int32 CCombatOverhaul::GetRallyHP(void)
{
    OVERHAUL& ov = Ov();
    if (!ov.m_bEnabled || !IsOn(FEATURE_RALLY) || !CGameStage::GetCurrent())
        return 0;

    return static_cast<int32>(ov.m_fRallyPool);
};
