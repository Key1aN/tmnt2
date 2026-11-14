#pragma once

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/Component/Enemy/EnemyTypes.hpp"
#include "Game/Component/Player/PlayerTypes.hpp"
#include "Game/System/Character/CharacterTypes.hpp"


class CCharacter;
class CPlayerCharacter;
class CEnemyCharacter;
class CHitAttackData;
class CCharacterAttackCalculator;


//
//  Combat Overhaul runtime (Debug Tools builds only).
//
//  One master switch (BEFORE = retail, AFTER = overhaul) plus one switch per
//  feature. Every hook asks IsOn(feature) first, so with the master switch on
//  BEFORE, or with a feature switched off, the retail code path runs unchanged.
//  All state lives here (keyed by player number / object handle); no retail
//  class keeps overhaul state that outlives a stage.
//
class CCombatOverhaul
{
public:
    enum FEATURE
    {
        FEATURE_ARROW_DODGE = 0,        //  1 dash goes through arrows/shuriken
        FEATURE_MIC_REFLECT_360,        //  2 Michelangelo reflects shots from any side while guarding
        FEATURE_MIC_FAST_COMBO,         //  3 Michelangelo combo speed
        FEATURE_POSTURE,                //  4 posture gauge + guard break (players and enemies)
        FEATURE_COMBO_BREAK,            //  4 combo break burst (players and enemies)
        FEATURE_ATTACK_INDICATOR,       //  5 marker over an enemy when it commits to an attack
        FEATURE_PARRY_CRYSTAL,          //  6 parry grants a random crystal power
        FEATURE_DON_MOBILE_SHIELD,      //  7 Donatello keeps moving while the barrier is up
        FEATURE_RALLY,                  //  8 regain recent damage by hitting back
        FEATURE_GUARD_RECOVERY,         //  9 act out of guard at once
        FEATURE_QUICK_RISE,             // 10 automatic quick rise after a knockdown
        FEATURE_LOCK_ON,                // 11 manual lock-on
        FEATURE_DASH_STICK,             // 12 dash follows the stick
        FEATURE_DASH_ATTACK_CANCEL,     // 13 attack out of dash, dash out of every attack
        FEATURE_FREE_COMBO_STEER,       // 14 steer freely during combos
        FEATURE_SMART_TARGET,           // 15 attacks and shuriken aim at the best enemy
        FEATURE_GUARD_AUTOFACE,         // 16 guard turns to the attacker (all but 30 degrees behind)
        FEATURE_COMBO_WINDOW,           // 17 late presses still continue the combo

        FEATURE_NUM,
    };

    enum MARKER
    {
        MARKER_ATTACK_LIGHT = 0,    // value = age 0..1
        MARKER_ATTACK_HEAVY,        // value = age 0..1
        MARKER_LOCKON,              // value unused
        MARKER_SOFT_TARGET,         // value unused
        MARKER_ENEMY_GAUGE,         // value = posture 0..1, value2 = combo break 0..1
    };

    struct MARKERINFO
    {
        MARKER m_type;
        RwV3d  m_vPos;      // world position (head, or body for target markers)
        float  m_fValue;
        float  m_fValue2;
    };

    struct PLAYERHUD
    {
        float m_fPosture;       // 0..1
        bool  m_bPostureBroken;
        float m_fBurst;         // 0..1
        bool  m_bBurstReady;
        int32 m_iCrystal;       // -1 none, 0 red, 1 green, 2 white, 3 orange
        float m_fCrystalTime;   // seconds left
        bool  m_bLockOn;
    };

public:
    // master switch and features
    static bool IsEnabled(void);
    static void SetEnabled(bool bEnable);
    static bool IsFeatureSwitchOn(FEATURE feature);
    static void SetFeatureSwitch(FEATURE feature, bool bOn);
    static bool IsOn(FEATURE feature);
    static float GetBadgeTime(void);    // seconds since the master switch last changed
    static void SetManualCamera(bool bManual);  // manual camera owns the right stick: lock-on is off
    static bool IsManualCamera(void);
    static float GetComboLateWindow(void);      // 0 when feature 17 is off
    static bool IsHudEnabled(void);
    static bool IsBadgeAlways(void);

    // Debug Tools page (one row per item)
    static int32 GetItemNum(void);
    static bool IsNumericItem(int32 index);
    static const char* GetItemHeader(int32 index);
    static void FormatItem(int32 index, char* pszBuffer, size_t capacity);
    static void GetItemHelp(int32 index, const char*& rpszScope, const char*& rpszDescription);
    static void AdjustItem(int32 index, int32 direction, int32 multiplier);
    static void ResetItem(int32 index);
    static void ResetAll(void);

    // once per game frame (safe to call from several objects; runs once)
    static void Period(void);

    // player input side (CManipulator)
    static void OnManipulatorInput(CPlayerCharacter& rPlayerChr, bool bMove, float fDirection);
    static void RequestLockOnToggle(int32 nPlayerNo);
    static void RequestLockSwitch(int32 nPlayerNo, int32 side);    // -1 screen-left, +1 screen-right
    static bool IsLockedOn(int32 nPlayerNo);
    static bool TryPlayerBurst(CPlayerCharacter& rPlayerChr);
    static bool SteerAttack(CPlayerCharacter& rPlayerChr, bool bMove, float fDirection);

    // player character side
    static void OnPlayerRun(CPlayerCharacter& rPlayerChr);
    static void OnAttackStart(CPlayerCharacter& rPlayerChr, bool bShot = false);
    static CHARACTERTYPES::DEFENCERSTATUSFLAG AdjustDefenceFlag(
        CPlayerCharacter& rPlayerChr,
        const CHitAttackData& rAttack,
        CHARACTERTYPES::DEFENCERSTATUSFLAG defenceFlag
    );
    static bool IsShotDodged(CPlayerCharacter& rPlayerChr, const CHitAttackData& rAttack);
    static void OnPlayerDamageResult(
        CPlayerCharacter& rPlayerChr,
        CHitAttackData& rAttack,
        CHARACTERTYPES::ATTACKRESULTTYPE attackResult
    );
    static void OnPlayerReceivedDamage(CPlayerCharacter& rPlayerChr, int32 nDamage);
    static float GetAttackMotionSpeed(const CPlayerCharacter& rPlayerChr);
    static float GetGuardBackTime(void);

    // true when a projectile at this position touches a raised Michelangelo guard shield
    static bool TryReflectProjectile(const RwV3d& rvPosition, float fRadius);

    // every hit, after the retail result and damage are known
    static void OnCatchAttack(
        CCharacter& rTarget,
        CHitAttackData& rAttack,
        CHARACTERTYPES::ATTACKRESULTTYPE attackResult,
        int32 iDamage
    );

    // enemy side
    static bool OnEnemyGuard(CEnemyCharacter& rEnemyChr, CHitAttackData& rAttack);
    static void OnEnemyStatus(CEnemyCharacter& rEnemyChr, ENEMYTYPES::STATUS statusNow, ENEMYTYPES::STATUS statusPrev);
    static void OnEnemyRun(CEnemyCharacter& rEnemyChr);

    // HUD data for the Debug Tools overlay
    static int32 GetMarkerNum(void);
    static bool GetMarker(int32 index, MARKERINFO& rInfo);
    static bool GetPlayerHud(int32 nPlayerNo, PLAYERHUD& rHud);

    // 8 rally: team health that can still be won back, drawn on the health gauge
    static int32 GetRallyHP(void);
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
