#include "Karai.hpp"

#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/GameMain/GamePlayer.hpp"
#include "Game/Component/GameMain/GameEvent.hpp"
#include "Game/Component/GameData/GameData.hpp"
#include "Game/Component/Module/AccumulateModule.hpp"
#include "Game/Component/Player/PlayerStateMachine.hpp"
#include "Game/Component/Player/PlayerStatus.hpp"
#include "Game/Component/Player/PlayerTracer.hpp"
#include "Game/Component/Enemy/Enemy.hpp"
#include "Game/Component/Effect/EffectManager.hpp"
#include "Game/Component/Effect/MagicManager.hpp"
#include "Game/System/Hit/HitAttackData.hpp"
#include "Game/System/Map/WorldMap.hpp"
#include "Game/Component/Player/ConcretePlayerStatus/PlayerStatusJump.hpp"
#include "Game/Component/Gimmick/GimmickManager.hpp"
#include "Game/Component/Shot/ShotManager.hpp"
#include "Game/Component/Module/ModuleManager.hpp"
#include "Game/Component/Module/CircleShadowModule.hpp"
#include "Game/System/Model/Model.hpp"
#include "Game/System/Model/ModelManager.hpp"
#include "Game/System/MotionParameter/MotionParameterManager.hpp"
#include "Game/System/Sound/GameSound.hpp"
#include "Game/System/2d/GameFont.hpp"
#include "System/Common/Sprite.hpp"
#include "System/Common/System2D.hpp"

#include <cstdio>

#include <algorithm>

#if defined(TARGET_PC)
#include "System/PC/PCCrashReporter.hpp"
#include "System/PC/PCModFeatures.hpp"
#endif /* defined(TARGET_PC) */


namespace Karai
{
    DEFINE_ENABLED_STATUS_FOR(CAttackJump, { PLAYERTYPES::STATUS_JUMP,
                                             PLAYERTYPES::STATUS_JUMP_2ND,
                                             PLAYERTYPES::STATUS_JUMP_WALL,
                                             PLAYERTYPES::STATUS_AERIAL,
                                             PLAYERTYPES::STATUS_AERIAL_MOVE });


    void CAttackJump::OnAttach(void)
    {
        Character().ChangeMotion(Karai::MOTIONNAMES::ATTACK_JUMP);
        
        RwV3d vVelocity = Math::VECTOR3_ZERO;
        Character().GetVelocity(&vVelocity);
        vVelocity.y = 0.0f;
        Character().SetVelocity(&vVelocity);
        
        CGameSound::PlayAttackSE(m_pPlayerChr);
    };


    void CAttackJump::OnDetach(void)
    {
        ;
    };


    void CAttackJump::OnRun(void)
    {
        ;
    };

    
    //
    // *********************************************************************************
    //


    void CAttackAABBC::OnDischargeWave(void)
    {
        RwV3d vPosition = Math::VECTOR3_ZERO;
        Character().GetBodyPosition(&vPosition);

        RwV3d vPositionLocal = Math::VECTOR3_ZERO;        
        Character().RotateVectorByDirection(&vPositionLocal, &Karai::CHARGE_ATTACK_LOCAL_POSITION);

        Math::Vec3_Add(&vPosition, &vPosition, &vPositionLocal);

        MAGIC_GENERIC::ChargeAttackKarai(&vPosition, Character().GetDirection(), m_pPlayerChr, MAGIC_GENERIC::STEP_THREE);
    };

    
    //
    // *********************************************************************************
    //


    void CAttackB::OnDischargeWave(MAGIC_GENERIC::STEP step)
    {
        RwV3d vPosition = Math::VECTOR3_ZERO;
        Character().GetBodyPosition(&vPosition);

        RwV3d vPositionLocal = Math::VECTOR3_ZERO;
        Character().RotateVectorByDirection(&vPositionLocal, &Karai::CHARGE_ATTACK_LOCAL_POSITION);

        Math::Vec3_Add(&vPosition, &vPosition, &vPositionLocal);

        MAGIC_GENERIC::ChargeAttackKarai(&vPosition, Character().GetDirection(), m_pPlayerChr, step);
    };
}; /* namespace Karai */


namespace KaraiBoss
{
    static const char* MODEL_NAME = "karai_boss";
    static const float ATTACK_POWER_SCALE       = 0.6f;    // base and warning phases
    static const float ATTACK_POWER_SCALE_FINAL = 0.8f;    // final phase (<= 35% HP)
    static const float PHASE_WARNING_HP_RATIO   = 0.70f;
    static const float PHASE_FINAL_HP_RATIO     = 0.35f;

    // Live boss-variant instances, for the HUD countdown.
    static CKarai* s_apBossKarai[GAMETYPES::PLAYERS_MAX] = {};

    // Shared player motions that the boss Karai motion set does not contain.
    // Every name the player code can request maps to a boss motion of similar
    // timing; anything else falls back to Idle in CCharacter::ResolveMotionName.
    static const CCharacter::MOTIONALIAS MOTION_ALIAS[] =
    {
        { "AABB",           "AAA"           },
        { "AABBB",          "AAA"           },
        { "AABC",           "AAC"           },
        { "AABBC",          "AAC"           },
        { "JAttack",        "JAttack2"      },  // JAttack1 is the boss ground wind-up; JAttack2 is the airborne attack
        { "JWall",          "J2"            },
        { "Shuriken",       "A"             },  // ground throw uses the quick slash (see CBossAttackKnife)
        { "JShuriken",      "JKunai"        },
        { "Nage1",          "B"             },
        { "Nage2",          "BB"            },
        { "Nage3",          "AAC"           },
        { "NageBack",       "BackAway"      },
        { "NageIdle",       "Idle"          },
        { "NageWalk",       "Walk"          },
        { "NagerarePose",   "YFrontFuttobi1"},
        { "NagerareTag1",   "YFrontFuttobi2"},
        { "NagerareTag2",   "YFrontFuttobi3"},
        { "Piyori",         "Idle"          },
        { "BiriBiri",       "YFront"        },
    };


    // Boss Karai's jump attack is JAttack1 (ground wind-up) -> JAttack2 (airborne)
    // -> JAttack3 (landing strike). The player jump attack starts in the air, so the
    // variant plays JAttack2 while diving and JAttack3 on landing.
    static const char* MOTION_JUMP_ATTACK_AIR = "JAttack2";
    static const char* MOTION_JUMP_ATTACK_LAND = "JAttack3";
    static const float JUMP_ATTACK_DIVE_SPEED = -9.0f;


    class CBossAttackJump : public Karai::CAttackJump
    {
    public:
        virtual void OnAttach(void) override
        {
            Character().ChangeMotion(MOTION_JUMP_ATTACK_AIR, true);

            RwV3d vVelocity = Math::VECTOR3_ZERO;
            Character().GetVelocity(&vVelocity);
            vVelocity.y = JUMP_ATTACK_DIVE_SPEED;
            Character().SetVelocity(&vVelocity);

            CGameSound::PlayAttackSE(m_pPlayerChr);
        };
    };


    class CBossTouchdown : public PlayerStatus::CTouchdown
    {
    public:
        virtual void OnAttach(void) override
        {
            PlayerStatus::CTouchdown::OnAttach();

            if (StateMachine().PrevStatus() == PLAYERTYPES::STATUS_ATTACK_JUMP)
                Character().ChangeMotion(MOTION_JUMP_ATTACK_LAND, true);
        };
    };


    static bool IsRequested(GAMETYPES::COSTUME costume)
    {
#if defined(TARGET_PC)
        if ((costume != GAMETYPES::COSTUME_SAMURAI) || !CPCModFeatures::IsKaraiBossVariantEnabled())
            return false;

        bool bModel = CModelManager::IsModelLoaded(MODEL_NAME);
        bool bParameter = CMotionParameterManager::IsCharacterParameterLoaded(MODEL_NAME);
        CPCCrashReporter::Breadcrumb("KARAI_BOSS variant requested model=%d parameter=%d",
                                     (bModel ? 1 : 0),
                                     (bParameter ? 1 : 0));
        return (bModel && bParameter);
#else /* defined(TARGET_PC) */
        (void)costume;
        return false;
#endif /* defined(TARGET_PC) */
    };
};


CKarai::CKarai(GAMETYPES::COSTUME costume)
: CPlayerCharacter("karai", PLAYERID::ID_KAR, costume)
, m_bBossVariant(false)
, m_bWindPressFollowup(false)
, m_bWindPressFromCombo(false)
, m_bThunderChargeReady(false)
, m_bThunderFromCharge(false)
, m_fThunderReadyTime(0.0f)
, m_phase(KaraiBoss::PHASE_BASE)
, m_bAuraShown(false)
, m_hAuraWarning(0)
, m_hAuraFinal(0)
, m_bCounterWindow(false)
, m_bCounterHit(false)
{
    // Holding R1 at character confirm selects COSTUME_SAMURAI, which runs Karai on
    // the boss Karai model, skeleton, and motion set (unique "karai_boss" records
    // in the player Karai LPAC). Missing assets fall back to the normal Karai.
    const bool bBossVariant = KaraiBoss::IsRequested(costume);
    m_bBossVariant = bBossVariant;

    //
	//	Model parts:
	//		0 - model
	//		1 - skirt
	//		2 - left sword
	//		3 - right sword
    //
    
    CPlayerCharacter::PARAMETER parameter = {};
    parameter.m_chrparameter.m_bToon            = true;
    parameter.m_chrparameter.m_pszModelName     = (bBossVariant ? KaraiBoss::MODEL_NAME : "karai");
    parameter.m_chrparameter.m_pszMotionSetName = (bBossVariant ? KaraiBoss::MODEL_NAME : "karai");
    parameter.m_feature.m_fWalkMoveSpeed        = 2.0f;
    parameter.m_feature.m_fLiftWalkMoveSpeed    = 3.6f;
    parameter.m_feature.m_fRunMoveSpeed         = 5.2f;
    parameter.m_feature.m_fDashMoveSpeed        = 16.0f;
    parameter.m_feature.m_fDashTime             = 0.2f;
    parameter.m_feature.m_fJumpInitializeSpeed  = 7.5f;
    parameter.m_feature.m_fAerialMoveSpeed      = 5.2f;
    parameter.m_feature.m_fAerialAcceleration   = 12.0f;
    parameter.m_feature.m_nKnifeAttachBoneID    = 3;

    parameter.m_pStateMachine = new CPlayerStateMachine(
        this,
        (bBossVariant ? static_cast<PLAYERTYPES::STATUS>(KaraiBoss::STATUS_BOSS_MAX) : PLAYERTYPES::STATUS::NORMALMAX)
    );

    CStatus::RegistDefaultForStateMachine(*parameter.m_pStateMachine);

    if (bBossVariant)
    {
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_JUMP, new KaraiBoss::CBossAttackJump);
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_TOUCHDOWN,   new KaraiBoss::CBossTouchdown);

        using namespace KaraiBoss;
        parameter.m_pStateMachine->RegistStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_DASH_STRIKE),   new CBossDashStrike);
        parameter.m_pStateMachine->RegistStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_WIND_PRESS),    new CBossWindPress);
        parameter.m_pStateMachine->RegistStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_THUNDER),       new CBossThunder);
        parameter.m_pStateMachine->RegistStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_KUNAI_RAIN),    new CBossKunaiRain);
        parameter.m_pStateMachine->RegistStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_COUNTER_SLASH), new CBossCounterSlash);
        parameter.m_pStateMachine->RegistStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_BACK_AWAY),     new CBossBackAway);
        parameter.m_pStateMachine->RegistStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_AAA),           new CBossAttackAAA);
        parameter.m_pStateMachine->RegistStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_KUNAI_FAN),     new CBossKunaiFan);
        parameter.m_pStateMachine->RegistStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_COUNTER_STANCE),new CBossCounterStance);
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_AA,                              new CBossAttackAA);
    }
    else
    {
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_JUMP, new Karai::CAttackJump);
    };
    parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_AABBC,   new Karai::CAttackAABBC);
    if (bBossVariant)
    {
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_B,        new KaraiBoss::CBossAttackB);
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_B_CHARGE, new KaraiBoss::CBossAttackBCharge);
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_KNIFE,    new KaraiBoss::CBossAttackKnife);
    }
    else
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_B, new Karai::CAttackB);

    Initialize(&parameter);

    if (bBossVariant)
    {
        SetMotionAliasTable(KaraiBoss::MOTION_ALIAS, COUNT_OF(KaraiBoss::MOTION_ALIAS));

        // Boss motion parameters aim their hit spheres at players. Retarget them to
        // enemies/gimmicks and rebalance boss damage for the player (60%).
        SetEnemyAttackDataRemap(true, KaraiBoss::ATTACK_POWER_SCALE);
    };

    m_pModuleMan->Include(CCircleShadowModule::New(this, 1.5f, 1.5f, false));

    if (bBossVariant)
    {
        for (int32 i = 0; i < COUNT_OF(KaraiBoss::s_apBossKarai); ++i)
        {
            if (!KaraiBoss::s_apBossKarai[i])
            {
                KaraiBoss::s_apBossKarai[i] = this;
                break;
            };
        };
    };
};


CKarai::~CKarai(void)
{
    FinishPhaseAura();

    for (int32 i = 0; i < COUNT_OF(KaraiBoss::s_apBossKarai); ++i)
    {
        if (KaraiBoss::s_apBossKarai[i] == this)
            KaraiBoss::s_apBossKarai[i] = nullptr;
    };
};


void CKarai::Run(void)
{
    CPlayerCharacter::Run();

    if (m_bBossVariant)
        UpdateBossPhase();
};


CHARACTERTYPES::ATTACKRESULTTYPE CKarai::OnDamage(CCharacterAttackCalculator& calculator)
{
    if (m_bBossVariant && m_bCounterWindow && !TestAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY))
    {
        // Counter Stance: the hit is nullified; the stance answers it.
        m_bCounterWindow = false;
        m_bCounterHit = true;

        RwV3d vBody = Math::VECTOR3_ZERO;
        GetBodyPosition(&vBody);
        uint32 hEffect = CEffectManager::Play(EFFECTID::ID_ALL_GUARD2, &vBody);
        if (hEffect)
            CEffectManager::SetScale(hEffect, 1.25f);
        CGameSound::PlayObjectSE(this, SDCODE_SE(4122));

#if defined(TARGET_PC)
        CPCCrashReporter::Breadcrumb("KARAI_BOSS counter hit phase=%d", static_cast<int32>(m_phase));
#endif /* defined(TARGET_PC) */
        return CHARACTERTYPES::ATTACKRESULTTYPE_INEFFECTIVE;
    };

    return CPlayerCharacter::OnDamage(calculator);
};


float CKarai::GetBossAttackScale(void) const
{
    return ((m_phase == KaraiBoss::PHASE_FINAL) ? KaraiBoss::ATTACK_POWER_SCALE_FINAL : KaraiBoss::ATTACK_POWER_SCALE);
};


int32 CKarai::GetKunaiRainNum(void) const
{
    return ((m_phase == KaraiBoss::PHASE_BASE) ? 12 : 18);
};


void CKarai::UpdateBossPhase(void)
{
    IGamePlayer* pGamePlayer = CGameProperty::Player(GetPlayerNo());
    if (!pGamePlayer)
        return;

    int32 hpMax = pGamePlayer->GetHPMax();
    float fRatio = ((hpMax > 0) ? (static_cast<float>(pGamePlayer->GetHP()) / static_cast<float>(hpMax)) : 1.0f);

    KaraiBoss::PHASE phase = KaraiBoss::PHASE_BASE;
    if (fRatio <= KaraiBoss::PHASE_FINAL_HP_RATIO)
        phase = KaraiBoss::PHASE_FINAL;
    else if (fRatio <= KaraiBoss::PHASE_WARNING_HP_RATIO)
        phase = KaraiBoss::PHASE_WARNING;

    bool bShow = (IsActive() && (pGamePlayer->GetHP() > 0));

    if ((phase != m_phase) || (bShow != m_bAuraShown))
    {
        bool bFlash = bShow && m_bAuraShown && (phase > m_phase);
        m_phase = phase;
        SetEnemyAttackDataRemap(true, GetBossAttackScale());

        FinishPhaseAura();
        if (bShow)
            SetPhaseAura(phase);
        m_bAuraShown = bShow;

        if (bFlash)
        {
            RwV3d vBody = Math::VECTOR3_ZERO;
            GetBodyPosition(&vBody);
            uint32 hFlash = CEffectManager::Play(EFFECTID::ID_ALL_TAME_FLASH, &vBody);
            if (hFlash)
                CEffectManager::SetScale(hFlash, 2.0f);
        };

#if defined(TARGET_PC)
        CPCCrashReporter::Breadcrumb("KARAI_BOSS phase=%d hp_pct=%d", static_cast<int32>(phase),
                                     static_cast<int32>(fRatio * 100.0f));
#endif /* defined(TARGET_PC) */
    };
};


void CKarai::SetPhaseAura(KaraiBoss::PHASE phase)
{
    // Same effects, offsets, and scales as boss Karai's warning and final modes.
    if (phase == KaraiBoss::PHASE_WARNING)
    {
        RwV3d vOffset = { 0.0f, 0.3f, 0.0f };
        m_hAuraWarning = CEffectManager::PlayTrace(EFFECTID::ID_I_CRYSHINE_R, new CPlayerTracer(this), &vOffset);
        if (m_hAuraWarning)
            CEffectManager::SetScale(m_hAuraWarning, 1.125f);
    }
    else if (phase == KaraiBoss::PHASE_FINAL)
    {
        RwV3d vOffset = { 0.0f, -0.9f, 0.0f };
        m_hAuraFinal = CEffectManager::PlayTrace(EFFECTID::ID_SETUP_ATTACK, new CPlayerTracer(this), &vOffset);
        if (m_hAuraFinal)
            CEffectManager::SetScale(m_hAuraFinal, 1.0f);
    };
};


void CKarai::FinishPhaseAura(void)
{
    if (m_hAuraWarning)
    {
        CEffectManager::Finish(m_hAuraWarning);
        m_hAuraWarning = 0;
    };

    if (m_hAuraFinal)
    {
        CEffectManager::Finish(m_hAuraFinal);
        m_hAuraFinal = 0;
    };
};


bool CKarai::IsBossVariant(void) const
{
    return m_bBossVariant;
};


bool CKarai::IsThunderReady(void) const
{
    return (CGameProperty::GetTotalElapsedTime() >= m_fThunderReadyTime);
};


float CKarai::GetThunderRemaining(void) const
{
    float fRemaining = m_fThunderReadyTime - CGameProperty::GetTotalElapsedTime();
    return ((fRemaining > 0.0f) ? fRemaining : 0.0f);
};


void CKarai::StartThunderCooldown(void)
{
    float fCooldown = KaraiBoss::THUNDER_COOLDOWN_BASE;
    if (m_phase == KaraiBoss::PHASE_WARNING)
        fCooldown = KaraiBoss::THUNDER_COOLDOWN_WARNING;
    else if (m_phase == KaraiBoss::PHASE_FINAL)
        fCooldown = KaraiBoss::THUNDER_COOLDOWN_FINAL;

    m_fThunderReadyTime = CGameProperty::GetTotalElapsedTime() + fCooldown;
};


void CKarai::ShootingKnife(void)
{
    if (m_bBossVariant)
    {
        // Bone 3 is the right sword on the player skeleton only; the boss
        // skeleton numbers its bones differently. Throw from chest height.
        RwV3d vBody = Math::VECTOR3_ZERO;
        GetBodyPosition(&vBody);
        RwV3d vForward = { 0.0f, 0.0f, 0.5f };
        RotateVectorByDirection(&vForward, &vForward);
        Math::Vec3_Add(&vBody, &vBody, &vForward);

        CShotManager::Shot(SHOTID::ID_KUNAI_PLAYER, &vBody, m_fDirection, this, MATH_DEG2RAD(15.0f), 5.0f);

        if (!TestAttribute(PLAYERTYPES::ATTRIBUTE_INNUMERABLE_KNIFE))
            CGameProperty::Player(GetPlayerNo())->AddShurikenNum(-1);
        return;
    };

    RwV3d vPosition = *m_pModel->GetBonePositionFromID(Feature().m_nKnifeAttachBoneID);
    RwV3d vLocalPos = Math::VECTOR3_ZERO;

    Math::Vec3_Sub(&vLocalPos, &vPosition, &m_vPosition);
    float fBuffY = vLocalPos.y;
    vLocalPos.y = 0.0f;

    float fLen = Math::Vec3_Length(&vLocalPos);
    if (fLen > 0.6f)
    {
        vLocalPos.x *= (0.5f / fLen);
        vLocalPos.z *= (0.5f / fLen);
        vLocalPos.y = fBuffY;

        Math::Vec3_Add(&vPosition, &vLocalPos, &m_vPosition);
    };

    CShotManager::Shot(SHOTID::ID_KUNAI_PLAYER, &vPosition, m_fDirection, this, MATH_DEG2RAD(15.0f), 5.0f);

    if (!TestAttribute(PLAYERTYPES::ATTRIBUTE_INNUMERABLE_KNIFE))
        CGameProperty::Player(GetPlayerNo())->AddShurikenNum(-1);
};


//
// *********************************************************************************
//  Karai boss variant: boss moves (milestone 2)
// *********************************************************************************
//


namespace KaraiBoss
{
    namespace MOVE
    {
        static const char* DASH_START       = "Dash1";
        static const char* DASH_RUN         = "Dash2";
        static const char* WIND_PRESS_START = "B";
        static const char* WIND_PRESS_SLAM  = "BB";
        static const char* THUNDER_CHARGE   = "C1";
        static const char* THUNDER_DASH     = "C2";
        static const char* THUNDER_FINISH   = "C3";
        static const char* KUNAI_READY      = "J1";
        static const char* KUNAI_SPIN       = "JKunai";
        static const char* KUNAI_LAND       = "J3";
        static const char* COUNTER          = "D";
        static const char* BACK_AWAY        = "BackAway";
        static const char* ATTACK_AAA       = "AAA";
        static const char* GUARD_START      = "G1";
        static const char* GUARD_HOLD       = "G2";
        static const char* GUARD_END        = "G3";

        static const float DASH_SPEED          = 17.0f;
        static const float DASH_DISTANCE       = 5.0f;
        static const float THUNDER_CHARGE_TIME = 0.75f;
        static const float THUNDER_DASH_SPEED  = 24.0f;
        static const float THUNDER_RANGE       = 12.0f;
        static const float KUNAI_JUMP_SPEED    = 9.0f;
        static const int32 KUNAI_NUM_MAX       = 18;
        static const int32 KUNAI_FAN_NUM       = 6;
        static const float KUNAI_FAN_ARC       = MATH_PI * 0.9f;
        static const float KUNAI_FAN_JUMP      = 6.0f;
        static const float KUNAI_FAN_PITCH     = MATH_DEG2RAD(12.0f);
        static const float COUNTER_WINDOW      = 0.4f;
        static const float KUNAI_PITCH         = MATH_DEG2RAD(22.0f);
        static const float BACK_AWAY_SPEED     = 3.0f;
        static const int32 WIND_PRESS_BOSS_POWER = 10; // scaled by the phase power scale
    };


    static void PlayTraceEffect(CPlayerCharacter* pPlayerChr, EFFECTID::VALUE id, float fScale, const RwV3d* pvOffset = nullptr, uint32* phEffect = nullptr)
    {
        RwV3d vOffset = (pvOffset ? *pvOffset : Math::VECTOR3_ZERO);
        uint32 hEffect = CEffectManager::PlayTrace(id, new CPlayerTracer(pPlayerChr), &vOffset);
        if (hEffect)
            CEffectManager::SetScale(hEffect, fScale);
        if (phEffect)
            *phEffect = hEffect;
    };


    static uint32 PlayBodyEffect(CPlayerCharacter* pPlayerChr, EFFECTID::VALUE id, float fScale)
    {
        RwV3d vBody = Math::VECTOR3_ZERO;
        pPlayerChr->GetBodyPosition(&vBody);
        uint32 hEffect = CEffectManager::Play(id, &vBody);
        if (hEffect)
            CEffectManager::SetScale(hEffect, fScale);
        return hEffect;
    };


    static void FinishEffect(uint32& hEffect)
    {
        if (hEffect)
        {
            CEffectManager::Finish(hEffect);
            hEffect = 0;
        };
    };


    static void SetForwardVelocity(CPlayerCharacter& chr, float fSpeed)
    {
        RwV3d vVelocity = { 0.0f, 0.0f, fSpeed };
        chr.RotateVectorByDirection(&vVelocity, &vVelocity);
        chr.SetVelocity(&vVelocity);
    };


    // Nearest living enemy within range and within 70 degrees of facing; faces it.
    static bool FaceNearestEnemy(CPlayerCharacter& chr, float fRange, float* pfDistance)
    {
        RwV3d vFoot = Math::VECTOR3_ZERO;
        chr.GetFootPosition(&vFoot);

        float fBestSq = fRange * fRange;
        float fBestDir = 0.0f;
        bool bFound = false;

        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!pEnemy)
                continue;

            RwV3d vEnemy = Math::VECTOR3_ZERO;
            pEnemy->GetPosition(&vEnemy);
            float dx = vEnemy.x - vFoot.x;
            float dz = vEnemy.z - vFoot.z;
            float fSq = (dx * dx) + (dz * dz);
            if ((fSq >= fBestSq) || (fSq < 0.01f))
                continue;

            float fDir = Math::ATan2(dx, dz);
            float fDiff = fDir - chr.GetDirection();
            while (fDiff > MATH_PI)  fDiff -= MATH_PI2;
            while (fDiff < -MATH_PI) fDiff += MATH_PI2;
            if (std::fabs(fDiff) > MATH_DEG2RAD(70.0f))
                continue;

            fBestSq = fSq;
            fBestDir = fDir;
            bFound = true;
        };

        if (bFound)
        {
            chr.SetDirection(fBestDir);
            if (pfDistance)
                *pfDistance = std::sqrt(fBestSq);
        };

        return bFound;
    };


    // Jump, wall-jump, and throw statuses leave these flags set when they end;
    // a stale FIXED_DIRECTION/FIXED_MODEL_ROTATION blocks turning and spinning.
    static void ClearDirectionLocks(CPlayerCharacter& chr)
    {
        chr.ClearCharacterFlag(CHARACTERTYPES::FLAG_FIXED_DIRECTION);
        chr.ClearCharacterFlag(CHARACTERTYPES::FLAG_FIXED_MODEL_ROTATION);
    };


    static void BossTrace(const char* pszFormat, int32 a, int32 b)
    {
#if defined(TARGET_PC)
        CPCCrashReporter::Breadcrumb(pszFormat, a, b);
#else /* defined(TARGET_PC) */
        (void)pszFormat; (void)a; (void)b;
#endif /* defined(TARGET_PC) */
    };


    static bool IsLanded(CPlayerCharacter& chr)
    {
        RwV3d vFoot = Math::VECTOR3_ZERO;
        chr.GetFootPosition(&vFoot);
        RwV3d vVelocity = Math::VECTOR3_ZERO;
        chr.GetVelocity(&vVelocity);
        float fMapHeight = CWorldMap::GetMapHeight(&vFoot);
        return ((vVelocity.y <= 0.0f) &&
                (std::fabs(vFoot.y - fMapHeight) <= 0.25f) &&
                (std::fabs(vVelocity.y) <= 0.01f));
    };


    bool CBossMoveStatus::IsEnableChangeStatus(PLAYERTYPES::STATUS status)
    {
        PLAYERTYPES::STATUS aStatusArray[] =
        {
            PLAYERTYPES::STATUS_IDLE,
            PLAYERTYPES::STATUS_WALK,
            PLAYERTYPES::STATUS_RUN,
            PLAYERTYPES::STATUS_GUARD_READY,
            PLAYERTYPES::STATUS_GUARD,
            static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_COUNTER_STANCE), // counter answers
        };

        return IsWithinStatusFromArray(status, aStatusArray, COUNT_OF(aStatusArray));
    };


    // ---- Dash: Dash Strike (Dash1 -> held Dash2 lunge) ----

    void CBossDashStrike::OnAttach(void)
    {
        ClearDirectionLocks(Character());
        m_step = 0;
        m_fTimer = 0.0f;
        m_fDashTime = MOVE::DASH_DISTANCE / MOVE::DASH_SPEED;

        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().ChangeMotion(MOVE::DASH_START, true);
        CGameSound::PlayAttackSE(m_pPlayerChr);
    };


    void CBossDashStrike::OnDetach(void)
    {
        Character().ClearAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().PlayMotion();
    };


    void CBossDashStrike::OnRun(void)
    {
        switch (m_step)
        {
        case 0:
            if (Character().IsMotionEnd())
            {
                Character().ChangeMotion(MOVE::DASH_RUN, true);
                Character().StopMotion();
                SetForwardVelocity(Character(), MOVE::DASH_SPEED);
                Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);

                RwV3d vFootOffset = { 0.0f, -0.6f, 0.0f };
                PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_ALL_DASH1, 1.0f, &vFootOffset);
                RwV3d vTrailOffset = { 0.0f, -0.6f, -0.75f };
                PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_ALL_DASH2, 1.0f, &vTrailOffset);
                CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4122));
                ++m_step;
            };
            break;

        case 1:
            m_fTimer += CGameProperty::GetElapsedTime();
            if ((m_fTimer >= m_fDashTime) || Character().GetCollisionWall()->m_bHit)
            {
                Character().ResetVelocity();
                Character().ClearAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
                Character().PlayMotion();
                ++m_step;
            };
            break;

        default:
            if (Character().IsMotionEnd())
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            break;
        };
    };


    // ---- B, B (or Guard + B): Wind Press (B -> BB with wind_press shockwave) ----

    bool CBossWindPress::IsEnableChangeStatus(PLAYERTYPES::STATUS status)
    {
        if (status == PLAYERTYPES::STATUS_ATTACK_B)
            return true;

        return CBossMoveStatus::IsEnableChangeStatus(status);
    };


    void CBossWindPress::OnAttach(void)
    {
        ClearDirectionLocks(Character());
        CKarai& karai = static_cast<CKarai&>(Character());
        bool bFromCombo = karai.TestWindPressFromCombo();
        karai.SetWindPressFromCombo(false);
        karai.ClearWindPressFollowup();

        m_step = 0;
        m_bMagic = false;

        Character().ResetVelocity();
        Character().ResetAcceleration();

        if (bFromCombo)
        {
            // B already played as the first hit: go straight to the slam.
            Character().ChangeMotion(MOVE::WIND_PRESS_SLAM, true);
            m_step = 1;
        }
        else
        {
            Character().ChangeMotion(MOVE::WIND_PRESS_START, true);
        };

        CGameSound::PlayAttackSE(m_pPlayerChr);
        BossTrace("KARAI_BOSS wind_press combo=%d step=%d", (bFromCombo ? 1 : 0), m_step);
    };


    void CBossWindPress::OnDetach(void)
    {
        ;
    };


    void CBossWindPress::OnRun(void)
    {
        switch (m_step)
        {
        case 0:
            {
                float fConnect = Character().GetNextChainMotionConnectTime();
                if ((Character().GetMotionTime() >= fConnect) || Character().IsMotionEnd())
                {
                    Character().ChangeMotion(MOVE::WIND_PRESS_SLAM, true);
                    ++m_step;
                };
            }
            break;

        default:
            if (!m_bMagic && Character().TestCharacterFlag(CHARACTERTYPES::FLAG_OCCURED_TIMING))
            {
                RwV3d vFoot = Math::VECTOR3_ZERO;
                Character().GetFootPosition(&vFoot);

                // Parent = player: the magic system retargets ATTACK_TO_PLAYER magic to enemies.
                uint32 hMagic = CMagicManager::Play(MAGICID::GetNameFromID(MAGICID::ID_WINDPRESS),
                                                    &vFoot, nullptr, m_pPlayerChr, true);
                if (hMagic)
                {
                    CMagicManager::SetChangeStatus(hMagic, CHitAttackData::STATUS_KNOCK);
                    CMagicManager::SetStatusTime(hMagic, 0.0f);
                    float fScale = static_cast<CKarai&>(Character()).GetBossAttackScale();
                    CMagicManager::SetDamage(hMagic, static_cast<int32>((static_cast<float>(MOVE::WIND_PRESS_BOSS_POWER) * fScale) + 0.5f));
                };
                m_bMagic = true;
            };

            if (Character().IsMotionEnd())
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            break;
        };
    };


    // ---- Guard + A+B: Thunder Special (C1 charge -> C2 dash -> C3 finisher) ----

    void CBossThunder::FinishEffects(void)
    {
        FinishEffect(m_hEffectCharge);
        for (int32 i = 0; i < COUNT_OF(m_ahEffectElectric); ++i)
            FinishEffect(m_ahEffectElectric[i]);
    };


    bool CBossThunder::IsEnableChangeStatus(PLAYERTYPES::STATUS status)
    {
        if (status == PLAYERTYPES::STATUS_ATTACK_B_CHARGE)
            return true;

        return CBossMoveStatus::IsEnableChangeStatus(status);
    };


    void CBossThunder::OnAttach(void)
    {
        CKarai& karai = static_cast<CKarai&>(Character());
        bool bFromCharge = karai.TestThunderFromCharge();
        karai.SetThunderFromCharge(false);
        karai.SetThunderChargeReady(false);
        karai.StartThunderCooldown();

        m_step = 0;
        m_chargeStep = 0;
        m_fTimer = 0.0f;
        m_fDashTime = 0.0f;
        m_hEffectCharge = 0;
        for (int32 i = 0; i < COUNT_OF(m_ahEffectElectric); ++i)
            m_ahEffectElectric[i] = 0;

        ClearDirectionLocks(Character());
        Character().ResetVelocity();
        Character().ResetAcceleration();
        FaceNearestEnemy(Character(), MOVE::THUNDER_RANGE, nullptr);
        BossTrace("KARAI_BOSS thunder from_charge=%d cooldown_ms=%d", (bFromCharge ? 1 : 0),
                  static_cast<int32>(karai.GetThunderRemaining() * 1000.0f));

        if (bFromCharge)
        {
            // Hold B already played the C1 charge: start at the dash.
            m_chargeStep = 2;
            StartDash();
            return;
        };

        Character().ChangeMotion(MOVE::THUNDER_CHARGE, true);

        float fEnd = Character().GetMotionEndTime();
        if (fEnd > 0.0f)
        {
            Character().SetCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
            Character().SetMotionSpeed(fEnd / MOVE::THUNDER_CHARGE_TIME);
        };

        m_hEffectCharge = PlayBodyEffect(m_pPlayerChr, EFFECTID::ID_ALL_TAME1, 1.25f);
        PlayBodyEffect(m_pPlayerChr, EFFECTID::ID_ALL_TAME_FLASH, 1.25f);

        const float afScale[] = { 1.5f, 2.25f };
        for (int32 i = 0; i < COUNT_OF(m_ahEffectElectric); ++i)
            PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_SHRD_ELEC, afScale[i], nullptr, &m_ahEffectElectric[i]);

        CGameSound::PlayAttackSE(m_pPlayerChr);
    };


    void CBossThunder::StartDash(void)
    {
        float fDistance = 6.0f;
        if (FaceNearestEnemy(Character(), MOVE::THUNDER_RANGE, &fDistance))
            fDistance += 1.5f;
        m_fDashTime = fDistance / MOVE::THUNDER_DASH_SPEED;
        m_fTimer = 0.0f;

        Character().ClearCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        Character().ChangeMotion(MOVE::THUNDER_DASH, true);
        FinishEffect(m_hEffectCharge);
        SetForwardVelocity(Character(), MOVE::THUNDER_DASH_SPEED);
        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);

        if (!m_ahEffectElectric[0])
            PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_SHRD_ELEC, 1.5f, nullptr, &m_ahEffectElectric[0]);

        RwV3d vFootOffset = { 0.0f, -0.6f, 0.0f };
        PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_ALL_DASH1, 1.75f, &vFootOffset);
        PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_ALL_DASH2, 2.0f, &vFootOffset);
        CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4411));
        CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(8387));
        m_step = 1;
    };


    void CBossThunder::OnDetach(void)
    {
        FinishEffects();
        Character().ClearCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        Character().ClearAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
    };


    void CBossThunder::OnRun(void)
    {
        switch (m_step)
        {
        case 0:
            {
                m_fTimer += CGameProperty::GetElapsedTime();
                const EFFECTID::VALUE aStepEffect[] = { EFFECTID::ID_ALL_TAME2, EFFECTID::ID_ALL_TAME3 };
                if ((m_chargeStep < 2) &&
                    (m_fTimer >= (MOVE::THUNDER_CHARGE_TIME * static_cast<float>(m_chargeStep + 1) / 3.0f)))
                {
                    FinishEffect(m_hEffectCharge);
                    m_hEffectCharge = PlayBodyEffect(m_pPlayerChr, aStepEffect[m_chargeStep], 1.25f);
                    PlayBodyEffect(m_pPlayerChr, EFFECTID::ID_ALL_TAME_FLASH, 1.25f);
                    ++m_chargeStep;
                };

                if (Character().IsMotionEnd())
                    StartDash();
            }
            break;

        case 1:
            m_fTimer += CGameProperty::GetElapsedTime();
            if ((m_fTimer >= m_fDashTime) || Character().GetCollisionWall()->m_bHit)
            {
                Character().ClearAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
                FinishEffects();
                Character().ResetVelocity();
                Character().ResetAcceleration();
                Character().ChangeMotion(MOVE::THUNDER_FINISH, true);
                PlayBodyEffect(m_pPlayerChr, EFFECTID::ID_ALL_TAME_FLASH, 2.0f);
                ++m_step;
            };
            break;

        default:
            if (Character().IsMotionEnd())
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            break;
        };
    };


    // ---- Guard + Jump: Kunai Rain (J1 -> spinning JKunai volley -> J3) ----

    void CBossKunaiRain::OnAttach(void)
    {
        m_step = 0;
        m_fTimer = 0.0f;
        m_bShot = false;

        ClearDirectionLocks(Character());
        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().ChangeMotion(MOVE::KUNAI_READY, true);
    };


    void CBossKunaiRain::OnDetach(void)
    {
        Character().ClearPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
        Character().ClearCharacterFlag(CHARACTERTYPES::FLAG_FIXED_MODEL_ROTATION);
    };


    void CBossKunaiRain::OnRun(void)
    {
        switch (m_step)
        {
        case 0:
            if (Character().IsMotionEnd())
            {
                Character().ChangeMotion(MOVE::KUNAI_SPIN, true);
                ClearDirectionLocks(Character());
                RwV3d vVelocity = Math::VECTOR3_ZERO;
                vVelocity.y = MOVE::KUNAI_JUMP_SPEED;
                Character().SetVelocity(&vVelocity);
                Character().SetPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
                CGameSound::PlayAttackSE(m_pPlayerChr);
                ++m_step;
            };
            break;

        case 1:
            {
                float fDt = CGameProperty::GetElapsedTime();
                m_fTimer += fDt;
                ClearDirectionLocks(Character());
                Character().SetDirection(Character().GetDirection() + (fDt * (MATH_PI * 5.0f)));

                RwV3d vVelocity = Math::VECTOR3_ZERO;
                Character().GetVelocity(&vVelocity);
                if (!m_bShot && ((vVelocity.y <= 2.0f) || Character().TestCharacterFlag(CHARACTERTYPES::FLAG_OCCURED_TIMING)))
                {
                    RwV3d vBody = Math::VECTOR3_ZERO;
                    Character().GetBodyPosition(&vBody);
                    int32 kunaiNum = std::min(static_cast<CKarai&>(Character()).GetKunaiRainNum(), MOVE::KUNAI_NUM_MAX);
                    float fStep = MATH_PI2 / static_cast<float>(kunaiNum);
                    for (int32 i = 0; i < kunaiNum; ++i)
                    {
                        RwMatrix matRot;
                        RwMatrixSetIdentity(&matRot);
                        Math::Matrix_RotationYawPitchRoll(&matRot,
                                                          Character().GetDirection() + (fStep * static_cast<float>(i)),
                                                          MOVE::KUNAI_PITCH,
                                                          0.0f);
                        RwV3d vDir = Math::VECTOR3_AXIS_Z;
                        RwV3dTransformVector(&vDir, &vDir, &matRot);
                        CShotManager::Shot(SHOTID::ID_KUNAI_PLAYER, &vBody, &vDir, m_pPlayerChr, MATH_DEG2RAD(15.0f), 5.0f);
                    };
                    CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(0x1020));
                    m_bShot = true;
                };

                if ((m_fTimer > 0.2f) && IsLanded(Character()))
                {
                    BossTrace("KARAI_BOSS kunai_rain landed spin_ms=%d shot=%d",
                              static_cast<int32>(m_fTimer * 1000.0f), (m_bShot ? 1 : 0));
                    Character().ResetVelocity();
                    Character().ResetAcceleration();
                    Character().ClearPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
                    Character().ChangeMotion(MOVE::KUNAI_LAND, true);
                    ++m_step;
                };
            }
            break;

        default:
            if (Character().IsMotionEnd())
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            break;
        };
    };


    // ---- Guard + B: Kunai Fan (J1 -> hop with 6 aimed kunai -> J3) ----

    void CBossKunaiFan::OnAttach(void)
    {
        m_step = 0;
        m_fTimer = 0.0f;
        m_bShot = false;

        ClearDirectionLocks(Character());
        Character().ResetVelocity();
        Character().ResetAcceleration();
        FaceNearestEnemy(Character(), MOVE::THUNDER_RANGE, nullptr);
        Character().ChangeMotion(MOVE::KUNAI_READY, true);
    };


    void CBossKunaiFan::OnDetach(void)
    {
        Character().ClearPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
    };


    void CBossKunaiFan::OnRun(void)
    {
        switch (m_step)
        {
        case 0:
            if (Character().IsMotionEnd())
            {
                Character().ChangeMotion(MOVE::KUNAI_SPIN, true);
                RwV3d vVelocity = Math::VECTOR3_ZERO;
                vVelocity.y = MOVE::KUNAI_FAN_JUMP;
                Character().SetVelocity(&vVelocity);
                Character().SetPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
                CGameSound::PlayAttackSE(m_pPlayerChr);
                ++m_step;
            };
            break;

        case 1:
            {
                m_fTimer += CGameProperty::GetElapsedTime();

                RwV3d vVelocity = Math::VECTOR3_ZERO;
                Character().GetVelocity(&vVelocity);
                if (!m_bShot && (vVelocity.y <= 1.5f))
                {
                    FaceNearestEnemy(Character(), MOVE::THUNDER_RANGE, nullptr);

                    RwV3d vBody = Math::VECTOR3_ZERO;
                    Character().GetBodyPosition(&vBody);
                    float fFirst = Character().GetDirection() - (MOVE::KUNAI_FAN_ARC * 0.5f);
                    float fStep = MOVE::KUNAI_FAN_ARC / static_cast<float>(MOVE::KUNAI_FAN_NUM - 1);
                    for (int32 i = 0; i < MOVE::KUNAI_FAN_NUM; ++i)
                    {
                        RwMatrix matRot;
                        RwMatrixSetIdentity(&matRot);
                        Math::Matrix_RotationYawPitchRoll(&matRot,
                                                          fFirst + (fStep * static_cast<float>(i)),
                                                          MOVE::KUNAI_FAN_PITCH,
                                                          0.0f);
                        RwV3d vDir = Math::VECTOR3_AXIS_Z;
                        RwV3dTransformVector(&vDir, &vDir, &matRot);
                        CShotManager::Shot(SHOTID::ID_KUNAI_PLAYER, &vBody, &vDir, m_pPlayerChr, MATH_DEG2RAD(15.0f), 5.0f);
                    };
                    CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(0x1020));
                    m_bShot = true;
                };

                if ((m_fTimer > 0.15f) && IsLanded(Character()))
                {
                    Character().ResetVelocity();
                    Character().ResetAcceleration();
                    Character().ClearPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
                    Character().ChangeMotion(MOVE::KUNAI_LAND, true);
                    ++m_step;
                };
            }
            break;

        default:
            if (Character().IsMotionEnd())
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            break;
        };
    };


    // ---- Guard + A: Counter Stance (G1 -> G2 window -> G3) ----

    void CBossCounterStance::OnAttach(void)
    {
        m_step = 0;
        m_fTimer = 0.0f;

        ClearDirectionLocks(Character());
        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().ChangeMotion(MOVE::GUARD_START, true);
        static_cast<CKarai&>(Character()).SetCounterWindow(true);

        RwV3d vBody = Math::VECTOR3_ZERO;
        Character().GetBodyPosition(&vBody);
        uint32 hEffect = CEffectManager::Play(EFFECTID::ID_ALL_GUARD1, &vBody);
        if (hEffect)
            CEffectManager::SetScale(hEffect, 1.0f);
    };


    void CBossCounterStance::OnDetach(void)
    {
        static_cast<CKarai&>(Character()).SetCounterWindow(false);
    };


    void CBossCounterStance::OnRun(void)
    {
        CKarai& karai = static_cast<CKarai&>(Character());

        if (karai.TestCounterHit())
        {
            karai.ClearCounterHit();
            karai.SetCounterWindow(false);

            if (karai.GetBossPhase() == PHASE_FINAL)
            {
                // Final phase: answer with the Thunder Special, like the boss.
                karai.SetThunderFromCharge(true);
                StateMachine().ChangeStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_THUNDER));
                if (StateMachine().CurrentStatus() == static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_THUNDER))
                    return;
                karai.SetThunderFromCharge(false);
            };

            StateMachine().ChangeStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_COUNTER_SLASH));
            return;
        };

        m_fTimer += CGameProperty::GetElapsedTime();

        switch (m_step)
        {
        case 0:
            if (Character().IsMotionEnd())
            {
                Character().ChangeMotion(MOVE::GUARD_HOLD, true);
                ++m_step;
            };
            if (m_fTimer < MOVE::COUNTER_WINDOW)
                break;
            /* fall through */

        case 1:
            if (m_fTimer >= MOVE::COUNTER_WINDOW)
            {
                karai.SetCounterWindow(false);
                Character().ChangeMotion(MOVE::GUARD_END, true);
                m_step = 2;
            };
            break;

        default:
            if (Character().IsMotionEnd())
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            break;
        };
    };


    // ---- Guard + Shuriken: Counter Slash (D) ----

    void CBossCounterSlash::OnAttach(void)
    {
        ClearDirectionLocks(Character());
        Character().ResetVelocity();
        Character().ResetAcceleration();
        FaceNearestEnemy(Character(), 4.0f, nullptr);
        Character().ChangeMotion(MOVE::COUNTER, true);
        CGameSound::PlayAttackSE(m_pPlayerChr);
    };


    void CBossCounterSlash::OnDetach(void)
    {
        ;
    };


    void CBossCounterSlash::OnRun(void)
    {
        if (Character().IsMotionEnd())
            StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
    };


    // ---- Guard + Dash: Back Away (evasive backflip) ----

    void CBossBackAway::OnAttach(void)
    {
        Character().ChangeMotion(MOVE::BACK_AWAY, true);
        Character().SetCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        Character().SetMotionSpeed(1.125f);
        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);

        RwV3d vVelocity = { 0.0f, 0.0f, -MOVE::BACK_AWAY_SPEED };
        Character().RotateVectorByDirection(&vVelocity, &vVelocity);
        Character().SetVelocity(&vVelocity);

        float fEnd = Character().GetMotionEndTime();
        if (fEnd > 0.0f)
        {
            RwV3d vAccel = vVelocity;
            Math::Vec3_Scale(&vAccel, &vAccel, -(1.125f / fEnd));
            Character().SetAcceleration(&vAccel);
        };

        CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(8388));
    };


    void CBossBackAway::OnDetach(void)
    {
        Character().ClearCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        Character().ClearAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ResetVelocity();
        Character().ResetAcceleration();
    };


    void CBossBackAway::OnRun(void)
    {
        if (Character().IsMotionEnd())
            StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
    };


    // ---- Charged B: boss charge (C1) -> Thunder finisher (C3) ----

    static CAccumulateModule* GetAccumulateModule(CPlayerCharacter& chr)
    {
        return static_cast<CAccumulateModule*>(chr.GetModule(MODULETYPE::ACCUMULATE));
    };


    static void HideRetailChargeGlow(CPlayerCharacter& chr)
    {
        CAccumulateModule* pModule = GetAccumulateModule(chr);
        if (pModule)
        {
            pModule->SetStepZero();
            pModule->SetEffectOff();
            pModule->SetDrawOff();
        };
    };


    void CBossAttackBCharge::FinishEffects(void)
    {
        FinishEffect(m_hEffectCharge);
        FinishEffect(m_hEffectElectric);
    };


    static const float THUNDER_HOLD_START = 0.25f; // B held this long starts the Thunder charge
    static const float THUNDER_HOLD_LIMIT = 1.5f;  // charge time after which it fires by itself


    void CBossAttackBCharge::OnAttach(void)
    {
        CKarai& karai = static_cast<CKarai&>(Character());
        karai.SetThunderChargeReady(false);

        m_bPlainB = !karai.IsThunderReady();
        m_bThunder = false;
        m_fThunderStart = 0.0f;
        m_bHold = false;
        m_glow = 0;
        m_hEffectCharge = 0;
        m_hEffectElectric = 0;

        ClearDirectionLocks(Character());
        Character().ResetVelocity();
        Character().ResetAcceleration();
        HideRetailChargeGlow(Character());

        // Like retail: hold the B windup until release (a tap is a plain B).
        Character().ChangeMotion(MOVE::WIND_PRESS_START, true);
        Character().SetCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        Character().SetMotionSpeed(0.05f);
    };


    void CBossAttackBCharge::OnDetach(void)
    {
        FinishEffects();
        Character().ClearCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        Character().PlayMotion();
        HideRetailChargeGlow(Character());
    };


    void CBossAttackBCharge::OnRun(void)
    {
        CKarai& karai = static_cast<CKarai&>(Character());

        if (m_bPlainB)
        {
            // Thunder is recharging: B is a plain slash (B, B still gives Wind Press).
            StateMachine().ChangeStatus(PLAYERTYPES::STATUS_ATTACK_B);
            return;
        };

        float fHeld = StateMachine().GetStatusDuration();

        if (!m_bThunder)
        {
            if (fHeld < THUNDER_HOLD_START)
                return;

            // Still held: start the Thunder charge (C1 crouch reaches its end when ready).
            m_bThunder = true;
            m_fThunderStart = fHeld;
            Character().ChangeMotion(MOVE::THUNDER_CHARGE, true);
            float fEnd = Character().GetMotionEndTime();
            Character().SetMotionSpeed((fEnd > 0.0f) ? (fEnd / MOVE::THUNDER_CHARGE_TIME) : 1.0f);
        };

        float fDuration = fHeld - m_fThunderStart;

        if (!m_bHold && Character().IsMotionEnd())
        {
            Character().StopMotion();
            m_bHold = true;
        };

        // Three glows across the charge, like the boss.
        const EFFECTID::VALUE aGlow[] = { EFFECTID::ID_ALL_TAME1, EFFECTID::ID_ALL_TAME2, EFFECTID::ID_ALL_TAME3 };
        if ((m_glow < 3) && (fDuration >= (MOVE::THUNDER_CHARGE_TIME * static_cast<float>(m_glow) / 3.0f)))
        {
            FinishEffect(m_hEffectCharge);
            m_hEffectCharge = PlayBodyEffect(m_pPlayerChr, aGlow[m_glow], 1.25f);
            PlayBodyEffect(m_pPlayerChr, EFFECTID::ID_ALL_TAME_FLASH, 1.25f);
            if (!m_hEffectElectric)
                PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_SHRD_ELEC, 1.5f, nullptr, &m_hEffectElectric);
            ++m_glow;
        };

        if (!karai.IsThunderChargeReady() && (fDuration >= MOVE::THUNDER_CHARGE_TIME))
        {
            karai.SetThunderChargeReady(true);
            PlayBodyEffect(m_pPlayerChr, EFFECTID::ID_ALL_TAME_FLASH, 2.0f);
            CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4411));
            BossTrace("KARAI_BOSS thunder charge ready t_ms=%d glow=%d", static_cast<int32>(fDuration * 1000.0f), m_glow);
        };

        if (karai.IsThunderChargeReady() && (fDuration >= THUNDER_HOLD_LIMIT))
        {
            karai.SetThunderFromCharge(true);
            StateMachine().ChangeStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_THUNDER));
        };
    };


    void CBossAttackB::OnAttach(void)
    {
        CKarai& karai = static_cast<CKarai&>(Character());

        // Start the B slash before the base attach: the attack swing sound is
        // chosen from the current motion name (the charge pose had none).
        Character().ChangeMotion(MOVE::WIND_PRESS_START, true);

        Karai::CAttackB::OnAttach();
        HideRetailChargeGlow(Character());
        karai.ClearWindPressFollowup();
        karai.SetThunderChargeReady(false);
    };


    void CBossAttackB::OnRun(void)
    {
        CKarai& karai = static_cast<CKarai&>(Character());
        if (karai.TestWindPressFollowup())
        {
            float fConnect = Character().GetNextChainMotionConnectTime();
            float fEnd = Character().GetMotionEndTime();
            if ((fConnect <= 0.0f) || (fConnect > fEnd))
                fConnect = fEnd * 0.6f;

            if ((Character().GetMotionTime() >= fConnect) || Character().IsMotionEnd())
            {
                karai.ClearWindPressFollowup();
                karai.SetWindPressFromCombo(true);
                StateMachine().ChangeStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_WIND_PRESS));
                if (StateMachine().CurrentStatus() == static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_WIND_PRESS))
                    return;
                karai.SetWindPressFromCombo(false);
            };
        };

        Karai::CAttackB::OnRun();
    };


    void CBossAttackB::OnDischargeWave(MAGIC_GENERIC::STEP step)
    {
        // The variant has no charge wave; Hold B is the Thunder Special.
        (void)step;
    };


    void CBossAttackB::OnDetach(void)
    {
        static_cast<CKarai&>(Character()).ClearWindPressFollowup();
        Karai::CAttackB::OnDetach();
    };


    // ---- A, A, A: boss third hit ----

    void CBossAttackAA::OnRun(void)
    {
        uint32 mask = PLAYERTYPES::FLAG_REQUEST_ATTACK_MASK;
        uint32 result = Character().CheckAttackConnect(static_cast<PLAYERTYPES::FLAG>(mask));

        if (result == PLAYERTYPES::FLAG_REQUEST_ATTACK_A)
        {
            StateMachine().ChangeStatus(static_cast<PLAYERTYPES::STATUS>(STATUS_BOSS_AAA));
        }
        else if (result == PLAYERTYPES::FLAG_REQUEST_ATTACK_B)
        {
            StateMachine().ChangeStatus(PLAYERTYPES::STATUS_ATTACK_AAB);
        }
        else if (result == PLAYERTYPES::FLAG_REQUEST_ATTACK_C)
        {
            if (CGameData::Record().Secret().GetAttackLevel() >= 1)
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_ATTACK_AAC);
        };

        PlayerStatus::CAttackCommon::OnRun();
    };


    bool CBossAttackAAA::IsEnableChangeStatus(PLAYERTYPES::STATUS status)
    {
        return (status == PLAYERTYPES::STATUS_ATTACK_AA);
    };


    void CBossAttackAAA::OnAttach(void)
    {
        Character().ChangeMotion(MOVE::ATTACK_AAA, true);
        PlayerStatus::CAttackCommon::OnAttach();
    };


    void CBossAttackAAA::OnDetach(void)
    {
        ;
    };


    void CBossAttackAAA::OnRun(void)
    {
        PlayerStatus::CAttackCommon::OnRun();
    };


    // ---- Shuriken: throw from the quick slash ----

    static const float KNIFE_THROW_TIME = 0.12f;
    static const float KNIFE_RECOVER_TIME = 0.45f;
    static const float KNIFE_MOTION_SPEED = 1.4f;


    void CBossAttackKnife::OnAttach(void)
    {
        m_fTimer = 0.0f;
        m_bShot = false;

        ClearDirectionLocks(Character());
        Character().ChangeMotion(PLAYERTYPES::MOTIONNAMES::ATTACK_KNIFE, true);
        Character().SetCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        Character().SetMotionSpeed(KNIFE_MOTION_SPEED);

        Character().ResetAcceleration();
        Character().ResetVelocity();
        Character().SetPlayerFlag(PLAYERTYPES::FLAG_DISABLE_THROW_KNIFE);
    };


    void CBossAttackKnife::OnDetach(void)
    {
        Character().ClearCharacterFlag(CHARACTERTYPES::FLAG_MOTION_SPEED_CTRL);
        Character().ClearPlayerFlag(PLAYERTYPES::FLAG_DISABLE_THROW_KNIFE);
    };


    void CBossAttackKnife::OnRun(void)
    {
        m_fTimer += CGameProperty::GetElapsedTime();

        if (!m_bShot && (m_fTimer >= KNIFE_THROW_TIME))
        {
            CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(0x1020));
            Character().ShootingKnife();
            CGameEvent::SetPlayerTechnicalAction(Character().GetPlayerNo(), GAMETYPES::TECACT_KNIFE);
            Character().ClearPlayerFlag(PLAYERTYPES::FLAG_DISABLE_THROW_KNIFE);
            m_bShot = true;
        };

        if ((m_fTimer >= KNIFE_RECOVER_TIME) || Character().IsMotionEnd())
            StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
    };
}; /* namespace KaraiBoss */


//
// *********************************************************************************
//  Karai boss variant: Thunder countdown above her head
// *********************************************************************************
//


namespace KaraiBoss
{
    static const float HUD_READY_SHOW_TIME = 1.5f;
    static const float HUD_HEAD_LIFT       = 0.25f;
    static const float HUD_TEXT_HEIGHT     = 15.0f;

    static float s_afReadyShownAt[GAMETYPES::PLAYERS_MAX] = {};
    static bool s_abWasCooling[GAMETYPES::PLAYERS_MAX] = {};


    static bool ProjectToVirtualScreen(const RwV3d& world, float& rx, float& ry)
    {
        RwMatrix matrixView;
        RwMatrixSetIdentity(&matrixView);
        CGameProperty::GetCameraViewMatrix(&matrixView);

        RwV3d projected = Math::VECTOR3_ZERO;
        RwV3dTransformPoint(&projected, &world, &matrixView);
        if ((projected.z <= 1.0f) || (projected.z > 100.0f))
            return false;

        float nx = projected.x / projected.z;
        float ny = projected.y / projected.z;
        if ((nx < -0.05f) || (nx > 1.05f) || (ny < -0.05f) || (ny > 1.05f))
            return false;

        rx = CSprite::m_fVirtualScreenX + (nx * CSprite::m_fVirtualScreenW);
        ry = CSprite::m_fVirtualScreenY + (ny * CSprite::m_fVirtualScreenH);
        return true;
    };
}; /* namespace KaraiBoss */


/*static*/ void CKarai::DrawBossHud(void)
{
    if (!CGameFont::IsOpen())
        return;

    bool bStatePushed = false;
    float fNow = CGameProperty::GetTotalElapsedTime();

    for (int32 i = 0; i < COUNT_OF(KaraiBoss::s_apBossKarai); ++i)
    {
        CKarai* pKarai = KaraiBoss::s_apBossKarai[i];
        if (!pKarai || !pKarai->IsActive())
            continue;

        float fRemaining = pKarai->GetThunderRemaining();
        bool bCooling = (fRemaining > 0.0f);
        if (!bCooling && KaraiBoss::s_abWasCooling[i])
            KaraiBoss::s_afReadyShownAt[i] = fNow;
        KaraiBoss::s_abWasCooling[i] = bCooling;

        bool bReadyLabel = (!bCooling &&
                            (KaraiBoss::s_afReadyShownAt[i] > 0.0f) &&
                            ((fNow - KaraiBoss::s_afReadyShownAt[i]) < KaraiBoss::HUD_READY_SHOW_TIME));
        if (!bCooling && !bReadyLabel)
            continue;

        RwV3d vHead = Math::VECTOR3_ZERO;
        pKarai->GetPosition(&vHead);
        vHead.y += pKarai->GetCollisionParameter().m_fHeight + KaraiBoss::HUD_HEAD_LIFT;

        float vx = 0.0f;
        float vy = 0.0f;
        if (!KaraiBoss::ProjectToVirtualScreen(vHead, vx, vy))
            continue;

        char szText[32];
        if (bCooling)
            std::snprintf(szText, sizeof(szText), "THUNDER %.1f", fRemaining);
        else
            std::snprintf(szText, sizeof(szText), "THUNDER READY");

        const float fHeight = KaraiBoss::HUD_TEXT_HEIGHT;
        float x = vx - (CGameFont::GetStringWidth(szText, fHeight) * 0.5f);
        float y = vy - fHeight;

        if (!bStatePushed)
        {
            CSystem2D::PushRenderState();
            bStatePushed = true;
        };

        CGameFont::SetHeight(fHeight);
        CGameFont::SetRGBA(0x00, 0x00, 0x00, 0xE0);
        CGameFont::Show(szText, x + 1.0f, y + 1.0f);
        if (bCooling)
            CGameFont::SetRGBA(0xFF, 0xB0, 0x30, 0xFF);
        else
            CGameFont::SetRGBA(0x60, 0xE0, 0xFF, 0xFF);
        CGameFont::Show(szText, x, y);
    };

    if (bStatePushed)
        CSystem2D::PopRenderState();
};
