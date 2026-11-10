#include "Karai.hpp"

#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/GameMain/GamePlayer.hpp"
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
    static const float ATTACK_POWER_SCALE = 0.6f;

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
        { "Shuriken",       "D"             },
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
    }
    else
    {
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_JUMP, new Karai::CAttackJump);
    };
    parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_AABBC,   new Karai::CAttackAABBC);
    if (bBossVariant)
        parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_B, new KaraiBoss::CBossAttackB);
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
};


CKarai::~CKarai(void)
{
    ;
};


bool CKarai::IsBossVariant(void) const
{
    return m_bBossVariant;
};


void CKarai::ShootingKnife(void)
{
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

        static const float DASH_SPEED          = 17.0f;
        static const float DASH_DISTANCE       = 5.0f;
        static const float THUNDER_CHARGE_TIME = 0.75f;
        static const float THUNDER_DASH_SPEED  = 24.0f;
        static const float THUNDER_RANGE       = 12.0f;
        static const float KUNAI_JUMP_SPEED    = 9.0f;
        static const int32 KUNAI_NUM           = 12;
        static const float KUNAI_PITCH         = MATH_DEG2RAD(22.0f);
        static const float BACK_AWAY_SPEED     = 3.0f;
        static const int32 WIND_PRESS_POWER    = 6;    // boss 10 x 0.6
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
        };

        return IsWithinStatusFromArray(status, aStatusArray, COUNT_OF(aStatusArray));
    };


    // ---- Guard + A: Dash Strike (Dash1 -> held Dash2 lunge) ----

    void CBossDashStrike::OnAttach(void)
    {
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


    // ---- Guard + B: Wind Press (B -> BB with wind_press shockwave) ----

    void CBossWindPress::OnAttach(void)
    {
        m_step = 0;
        m_bMagic = false;

        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().ChangeMotion(MOVE::WIND_PRESS_START, true);
        CGameSound::PlayAttackSE(m_pPlayerChr);
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
                    CMagicManager::SetDamage(hMagic, MOVE::WIND_PRESS_POWER);
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


    void CBossThunder::OnAttach(void)
    {
        m_step = 0;
        m_chargeStep = 0;
        m_fTimer = 0.0f;
        m_fDashTime = 0.0f;
        m_hEffectCharge = 0;
        for (int32 i = 0; i < COUNT_OF(m_ahEffectElectric); ++i)
            m_ahEffectElectric[i] = 0;

        Character().ResetVelocity();
        Character().ResetAcceleration();
        FaceNearestEnemy(Character(), MOVE::THUNDER_RANGE, nullptr);
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

                    RwV3d vFootOffset = { 0.0f, -0.6f, 0.0f };
                    PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_ALL_DASH1, 1.75f, &vFootOffset);
                    PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_ALL_DASH2, 2.0f, &vFootOffset);
                    CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4411));
                    CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(8387));
                    ++m_step;
                };
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
                Character().SetDirection(Character().GetDirection() + (fDt * (MATH_PI * 5.0f)));

                RwV3d vVelocity = Math::VECTOR3_ZERO;
                Character().GetVelocity(&vVelocity);
                if (!m_bShot && ((vVelocity.y <= 2.0f) || Character().TestCharacterFlag(CHARACTERTYPES::FLAG_OCCURED_TIMING)))
                {
                    RwV3d vBody = Math::VECTOR3_ZERO;
                    Character().GetBodyPosition(&vBody);
                    float fStep = MATH_PI2 / static_cast<float>(MOVE::KUNAI_NUM);
                    for (int32 i = 0; i < MOVE::KUNAI_NUM; ++i)
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


    // ---- Guard + Shuriken: Counter Slash (D) ----

    void CBossCounterSlash::OnAttach(void)
    {
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


    // ---- Charged B: boss Thunder finisher (C3) instead of the charge wave ----

    void CBossAttackB::OnAttach(void)
    {
        Karai::CAttackB::OnAttach();
        m_hEffectElectric = 0;

        m_bBossFinisher = ((Character().GetChargePhase() == PLAYERTYPES::CHARGEPHASE_1ST) ||
                           (Character().GetChargePhase() == PLAYERTYPES::CHARGEPHASE_2ND) ||
                           (Character().GetChargePhase() == PLAYERTYPES::CHARGEPHASE_3RD));
        if (m_bBossFinisher)
        {
            Character().ChangeMotion(MOVE::THUNDER_FINISH, true);
            PlayBodyEffect(m_pPlayerChr, EFFECTID::ID_ALL_TAME_FLASH, 1.5f);
        };
    };


    void CBossAttackB::OnDischargeWave(MAGIC_GENERIC::STEP step)
    {
        if (!m_bBossFinisher)
        {
            Karai::CAttackB::OnDischargeWave(step);
            return;
        };

        // The C3 hit data (retargeted to enemies, charge-scaled power) is the attack;
        // the electric burst is visual.
        float fScale = ((step == MAGIC_GENERIC::STEP_THREE) ? 2.5f : ((step == MAGIC_GENERIC::STEP_TWO) ? 2.0f : 1.5f));
        FinishEffect(m_hEffectElectric);
        PlayTraceEffect(m_pPlayerChr, EFFECTID::ID_SHRD_ELEC, fScale, nullptr, &m_hEffectElectric);
        PlayBodyEffect(m_pPlayerChr, EFFECTID::ID_ALL_TAME_FLASH, fScale);
    };


    void CBossAttackB::OnDetach(void)
    {
        FinishEffect(m_hEffectElectric);
        Karai::CAttackB::OnDetach();
    };
}; /* namespace KaraiBoss */
