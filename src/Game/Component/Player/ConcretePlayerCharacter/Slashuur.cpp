#include "Slashuur.hpp"

#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/GameMain/GamePlayer.hpp"
#include "Game/Component/Enemy/Enemy.hpp"
#include "Game/Component/Enemy/EnemyCharacter.hpp"
#include "Game/Component/Effect/EffectManager.hpp"
#include "Game/Component/Effect/MagicManager.hpp"
#include "Game/Component/Effect/MagicTypes.hpp"
#include "Game/Component/Player/PlayerStateMachine.hpp"
#include "Game/Component/Player/PlayerStatus.hpp"
#include "Game/Component/Gimmick/GimmickManager.hpp"
#include "Game/Component/Shot/ShotManager.hpp"
#include "Game/Component/Module/ModuleManager.hpp"
#include "Game/Component/Module/CircleShadowModule.hpp"
#include "Game/Component/Module/BandanaModule.hpp"
#include "Game/System/Model/Model.hpp"
#include "Game/System/Hit/HitAttackData.hpp"
#include "Game/System/Hit/HitAttackManager.hpp"
#include "Game/System/Map/WorldMap.hpp"
#include "Game/System/Sound/GameSound.hpp"


namespace Slashuur
{
    static const float BOSS_GROUND_BLAST_JUMP_SPEED = 10.606602f;
    static const float BOSS_SCYTHE_RADIUS = 3.2f;
    static const int32 BOSS_DRAIN_DAMAGE = 30;


    static PLAYERTYPES::STATUS BossStatus(STATUS status)
    {
        return static_cast<PLAYERTYPES::STATUS>(status);
    };


    static void PlayScaledEffect(EFFECTID::VALUE idEffect, const RwV3d& position, float scale)
    {
        uint32 hEffect = CEffectManager::Play(idEffect, &position);
        if (hEffect)
            CEffectManager::SetScale(hEffect, scale);
    };


    static void RegistEnemySphereAttack(CPlayerCharacter& character,
                                        const RwV3d& position,
                                        float radius,
                                        int32 power,
                                        CHitAttackData::STATUS status)
    {
        RwSphere hitSphere;
        hitSphere.center = position;
        hitSphere.radius = radius;

        CHitAttackData attack;
        attack.SetObject(character.GetHandle());
        attack.SetObjectPos(&position);
        attack.SetTarget(CHitAttackData::TARGET_ENEMY);
        attack.SetAntiguard(CHitAttackData::ANTIGUARD_INVALID);
        attack.SetStatus(status);
        attack.SetPower(power);
        attack.SetShape(CHitAttackData::SHAPE_SPHERE);
        attack.SetSphere(&hitSphere);

        CHitAttackManager::RegistAttack(&attack);
    };


    static void TeleportForward(CPlayerCharacter& character)
    {
        RwV3d position = Math::VECTOR3_ZERO;
        character.GetFootPosition(&position);

        RwV3d offset = { 0.0f, 0.0f, 5.0f };
        character.RotateVectorByDirection(&offset, &offset);

        RwV3d destination = Math::VECTOR3_ZERO;
        Math::Vec3_Add(&destination, &position, &offset);

        RwV3d lineStart = position;
        RwV3d lineEnd = destination;
        lineStart.y += 1.0f;
        lineEnd.y += 1.0f;

        if (CWorldMap::CheckCollisionLine(&lineStart, &lineEnd))
        {
            const CWorldMap::COLLISIONRESULT* pResult = CWorldMap::GetCollisionResult();
            if (pResult)
                destination = pResult->m_vClosestPt;
        };

        destination.y = CWorldMap::GetMapHeight(&destination);
        character.SetPosition(&destination);
    };


    static uint32 SpawnPurpleBall(CPlayerCharacter& character, bool bDownward)
    {
        RwV3d position = Math::VECTOR3_ZERO;
        character.GetPosition(&position);

        RwV3d offset = Math::VECTOR3_ZERO;
        character.GetOffsetPosition(&offset, 1);
        Math::Vec3_Add(&position, &position, &offset);

        RwV3d direction = { 0.0f, -1.0f, 0.0f };
        if (!bDownward)
        {
            direction = { 0.0f, 0.0f, 1.0f };
            character.RotateVectorByDirection(&direction, &direction);
        };

        CMagicManager::CParameter parameter;
        parameter.SetObject(&character);
        parameter.SetPositon(&position);
        parameter.SetDirection(&direction);

        uint32 hMagic = CMagicManager::Play(MAGICID::ID_SLABALL, &parameter);
        if (hMagic && bDownward)
        {
            CMagicManager::SetScale(hMagic, 1.73f);
            CMagicManager::SetSpeed(hMagic, 50.0f);
        };

        CGameSound::PlayObjectSE(&character, SDCODE_SE(4401));
        return hMagic;
    };


    static int32 DrainNearbyEnemies(CPlayerCharacter& character)
    {
        RwV3d playerPosition = Math::VECTOR3_ZERO;
        character.GetBodyPosition(&playerPosition);

        int32 drained = 0;
        const float radiusSq = 8.0f * 8.0f;
        int32 enemyMax = CGameProperty::GetEnemyMax();
        for (int32 i = 0; i < enemyMax; ++i)
        {
            CEnemy* pEnemy = CGameProperty::GetEnemy(i);
            if (!pEnemy || (pEnemy->GetHP() <= 0))
                continue;

            CEnemyCharacter& enemyCharacter = pEnemy->Character();

            RwV3d enemyPosition = Math::VECTOR3_ZERO;
            enemyCharacter.GetBodyPosition(&enemyPosition);

            RwV3d distance = Math::VECTOR3_ZERO;
            Math::Vec3_Sub(&distance, &enemyPosition, &playerPosition);
            if (Math::Vec3_Dot(&distance, &distance) > radiusSq)
                continue;

            enemyCharacter.OnMessageReceivedDamage(BOSS_DRAIN_DAMAGE);
            PlayScaledEffect(EFFECTID::ID_HPSTEAL_LIGHT, enemyPosition, 1.0f);
            ++drained;

            if (drained >= 3)
                break;
        };

        if (drained > 0)
        {
            character.OnMessageReceivedDamage(-(BOSS_DRAIN_DAMAGE * drained));
            PlayScaledEffect(EFFECTID::ID_HPSTEAL_LIGHT, playerPosition, 1.5f);
        };

        return drained;
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


    void CBossTeleport::OnAttach(void)
    {
        m_step = 0;

        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().ChangeMotion(PLAYERTYPES::MOTIONNAMES::IDLE, true);

        RwV3d position = Math::VECTOR3_ZERO;
        Character().GetFootPosition(&position);
        PlayScaledEffect(EFFECTID::ID_WARP_START, position, 1.5f);
        CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4396));
    };


    void CBossTeleport::OnDetach(void)
    {
        Character().GetModel()->SetDrawEnable(true);
        Character().SetEnableBodyHit(true);
        Character().SetEnableCatchHit(true);
        Character().SetEnableBodyHitSelfToOther(true);
        Character().ClearAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
    };


    void CBossTeleport::OnRun(void)
    {
        float duration = StateMachine().GetStatusDuration();
        switch (m_step)
        {
        case 0:
            if (duration >= 0.18f)
            {
                Character().GetModel()->SetDrawEnable(false);
                Character().SetEnableBodyHit(false);
                Character().SetEnableCatchHit(false);
                Character().SetEnableBodyHitSelfToOther(false);
                ++m_step;
            };
            break;

        case 1:
            if (duration >= 0.36f)
            {
                TeleportForward(Character());

                RwV3d position = Math::VECTOR3_ZERO;
                Character().GetFootPosition(&position);
                PlayScaledEffect(EFFECTID::ID_WARP_OUT, position, 1.5f);

                Character().GetModel()->SetDrawEnable(true);
                Character().SetEnableBodyHit(true);
                Character().SetEnableCatchHit(true);
                Character().SetEnableBodyHitSelfToOther(true);

                Character().GetBodyPosition(&position);
                RegistEnemySphereAttack(Character(), position, 2.25f, 15, CHitAttackData::STATUS_KNOCK);
                CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4395));
                ++m_step;
            };
            break;

        case 2:
            if (duration >= 0.58f)
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            break;

        default:
            break;
        };
    };


    void CBossScythe::OnAttach(void)
    {
        m_step = 0;
        m_fPulseTime = 0.2f;

        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().ChangeMotion(MOTIONNAMES::SCYTHE_LOOP, true);

        RwV3d position = Math::VECTOR3_ZERO;
        Character().GetFootPosition(&position);
        PlayScaledEffect(EFFECTID::ID_WARP_OUT, position, 1.25f);
        CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4403));
    };


    void CBossScythe::OnDetach(void)
    {
        Character().ClearAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        CGameSound::FadeOutSE(SDCODE_SE(4403), CGameSound::FADESPEED_NORMAL);
    };


    void CBossScythe::OnRun(void)
    {
        if (m_step == 0)
        {
            m_fPulseTime += CGameProperty::GetElapsedTime();
            if (m_fPulseTime >= 0.45f)
            {
                m_fPulseTime = 0.0f;

                RwV3d center = Math::VECTOR3_ZERO;
                Character().GetBodyPosition(&center);
                RegistEnemySphereAttack(Character(), center, BOSS_SCYTHE_RADIUS, 10, CHitAttackData::STATUS_FLYAWAY);

                for (int32 i = 0; i < 4; ++i)
                {
                    float angle = (MATH_PI2 * static_cast<float>(i)) / 4.0f;
                    RwV3d position = center;
                    position.x += std::sin(angle) * BOSS_SCYTHE_RADIUS;
                    position.z += std::cos(angle) * BOSS_SCYTHE_RADIUS;
                    PlayScaledEffect(EFFECTID::ID_SICKLE_WARP, position, 0.8f);
                };
            };

            if (StateMachine().GetStatusDuration() >= 2.0f)
            {
                Character().ChangeMotion(MOTIONNAMES::SCYTHE_END, true);
                ++m_step;
            };
        }
        else if (Character().IsMotionEnd())
        {
            StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
        };
    };


    void CBossGroundBlast::OnAttach(void)
    {
        m_step = 0;
        m_bShot = false;

        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().SetCharacterFlag(CHARACTERTYPES::FLAG_FIXED_MODEL_ROTATION);
        Character().ChangeMotion(MOTIONNAMES::GROUND_BLAST_START, true);
        CGameSound::PlayAttackSE(m_pPlayerChr);
    };


    void CBossGroundBlast::OnDetach(void)
    {
        Character().ClearAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ClearPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
        Character().ClearCharacterFlag(CHARACTERTYPES::FLAG_FIXED_MODEL_ROTATION |
                                       CHARACTERTYPES::FLAG_CLAMP_VELOCITY_XZ);
    };


    void CBossGroundBlast::OnRun(void)
    {
        switch (m_step)
        {
        case 0:
            if (Character().IsMotionEnd())
            {
                Character().ChangeMotion(MOTIONNAMES::GROUND_BLAST_JUMP, true);

                RwV3d velocity = Math::VECTOR3_ZERO;
                velocity.y = BOSS_GROUND_BLAST_JUMP_SPEED;
                Character().SetVelocity(&velocity);
                Character().SetPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
                Character().SetCharacterFlag(CHARACTERTYPES::FLAG_CLAMP_VELOCITY_XZ);
                ++m_step;
            };
            break;

        case 1:
            {
                RwV3d velocity = Math::VECTOR3_ZERO;
                Character().GetVelocity(&velocity);
                if (velocity.y <= 0.0f)
                {
                    Character().ChangeMotion(MOTIONNAMES::GROUND_BLAST_FIRE, true);
                    SpawnPurpleBall(Character(), true);
                    m_bShot = true;
                    ++m_step;
                };
            }
            break;

        case 2:
            if (m_bShot)
            {
                RwV3d footPosition = Math::VECTOR3_ZERO;
                Character().GetFootPosition(&footPosition);

                RwV3d velocity = Math::VECTOR3_ZERO;
                Character().GetVelocity(&velocity);

                float mapHeight = CWorldMap::GetMapHeight(&footPosition);
                if ((std::fabs(footPosition.y - mapHeight) <= 0.25f) &&
                    (std::fabs(velocity.y) <= 0.01f))
                {
                    Character().ResetVelocity();
                    Character().ResetAcceleration();
                    Character().ClearPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
                    Character().ChangeMotion(MOTIONNAMES::GROUND_BLAST_END, true);
                    ++m_step;
                };
            };
            break;

        case 3:
            if (Character().IsMotionEnd())
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            break;

        default:
            break;
        };
    };


    void CBossShot::OnAttach(void)
    {
        m_bShot = false;

        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().ChangeMotion(PLAYERTYPES::MOTIONNAMES::ATTACK_KNIFE, true);
        CGameSound::PlayAttackSE(m_pPlayerChr);
    };


    void CBossShot::OnDetach(void)
    {
        ;
    };


    void CBossShot::OnRun(void)
    {
        if (!m_bShot && Character().TestCharacterFlag(CHARACTERTYPES::FLAG_OCCURED_TIMING))
        {
            SpawnPurpleBall(Character(), false);
            m_bShot = true;
        };

        if (Character().IsMotionEnd())
            StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
    };


    void CBossDrain::OnAttach(void)
    {
        m_step = 0;
        m_hEffect = 0;
        m_bDrained = false;

        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().ChangeMotion(MOTIONNAMES::DRAIN_START, true);
        CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4402));
    };


    void CBossDrain::OnDetach(void)
    {
        Character().ClearAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        if (m_hEffect)
        {
            CEffectManager::Finish(m_hEffect);
            m_hEffect = 0;
        };

        CGameSound::FadeOutSE(SDCODE_SE(4402), CGameSound::FADESPEED_NORMAL);
    };


    void CBossDrain::OnRun(void)
    {
        switch (m_step)
        {
        case 0:
            if (Character().TestCharacterFlag(CHARACTERTYPES::FLAG_OCCURED_TIMING))
            {
                RwV3d position = Math::VECTOR3_ZERO;
                Character().GetFootPosition(&position);
                position.y += 0.1f;
                m_hEffect = CEffectManager::Play(EFFECTID::ID_SLA_SHADOW, &position);
                if (m_hEffect)
                    CEffectManager::SetScale(m_hEffect, 1.25f);
                ++m_step;
            };
            break;

        case 1:
            if (Character().IsMotionEnd())
            {
                Character().ChangeMotion(MOTIONNAMES::DRAIN_START2, true);
                ++m_step;
            };
            break;

        case 2:
            if (!m_bDrained && Character().TestCharacterFlag(CHARACTERTYPES::FLAG_OCCURED_TIMING))
            {
                DrainNearbyEnemies(Character());
                m_bDrained = true;
            };

            if (Character().IsMotionEnd())
            {
                Character().ChangeMotion(MOTIONNAMES::DRAIN_LOOP, true);
                ++m_step;
            };
            break;

        case 3:
            if (StateMachine().GetStatusDuration() >= 3.0f)
            {
                Character().ChangeMotion(MOTIONNAMES::DRAIN_END, true);
                ++m_step;
            };
            break;

        case 4:
            if (Character().IsMotionEnd())
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            break;

        default:
            break;
        };
    };


    DEFINE_ENABLED_STATUS_FOR(CAttackJump, { PLAYERTYPES::STATUS_JUMP,
                                             PLAYERTYPES::STATUS_JUMP_2ND,
                                             PLAYERTYPES::STATUS_JUMP_WALL,
                                             PLAYERTYPES::STATUS_AERIAL,
                                             PLAYERTYPES::STATUS_AERIAL_MOVE });


    void CAttackJump::OnAttach(void)
    {
        Character().ChangeMotion(Slashuur::MOTIONNAMES::ATTACK_JUMP);

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
        Character().RotateVectorByDirection(&vPositionLocal, &Slashuur::CHARGE_ATTACK_LOCAL_POSITION);

        Math::Vec3_Add(&vPosition, &vPosition, &vPositionLocal);

        MAGIC_GENERIC::ChargeAttackSlashuur(&vPosition, Character().GetDirection(), m_pPlayerChr, MAGIC_GENERIC::STEP_THREE);
    };

    
    //
    // *********************************************************************************
    //


    void CAttackB::OnDischargeWave(MAGIC_GENERIC::STEP step)
    {
        RwV3d vPosition = Math::VECTOR3_ZERO;
        Character().GetBodyPosition(&vPosition);

        RwV3d vPositionLocal = Math::VECTOR3_ZERO;
        Character().RotateVectorByDirection(&vPositionLocal, &Slashuur::CHARGE_ATTACK_LOCAL_POSITION);

        Math::Vec3_Add(&vPosition, &vPosition, &vPositionLocal);

        MAGIC_GENERIC::ChargeAttackSlashuur(&vPosition, Character().GetDirection(), m_pPlayerChr, step);
    };
}; /* namespace Slashuur */


CSlashuur::CSlashuur(GAMETYPES::COSTUME costume)
: CPlayerCharacter("slashuur", PLAYERID::ID_SLA, costume)
{
	//
	//	Model parts:
	//		0 - model
	//		1 - chest
	//		2 - back
	//		3 - weapon
	//

    CPlayerCharacter::PARAMETER parameter = {};
    parameter.m_chrparameter.m_bToon            = true;
    parameter.m_chrparameter.m_pszModelName     = "slashuur";
    parameter.m_chrparameter.m_pszMotionSetName = "slashuur";
    parameter.m_feature.m_fWalkMoveSpeed        = 2.0f;
    parameter.m_feature.m_fLiftWalkMoveSpeed    = 3.6f;
    parameter.m_feature.m_fRunMoveSpeed         = 5.2f;
    parameter.m_feature.m_fDashMoveSpeed        = 16.0f;
    parameter.m_feature.m_fDashTime             = 0.2f;
    parameter.m_feature.m_fJumpInitializeSpeed  = 7.5f;
    parameter.m_feature.m_fAerialMoveSpeed      = 5.2f;
    parameter.m_feature.m_fAerialAcceleration   = 12.0f;
    parameter.m_feature.m_nKnifeAttachBoneID    = 4;
    
    parameter.m_pStateMachine = new CPlayerStateMachine(this, Slashuur::STATUS_BOSS_MAX);

    CStatus::RegistDefaultForStateMachine(*parameter.m_pStateMachine);

    parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_JUMP,    new Slashuur::CAttackJump);
    parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_AABBC,   new Slashuur::CAttackAABBC);
    parameter.m_pStateMachine->RegistStatus(PLAYERTYPES::STATUS_ATTACK_B,       new Slashuur::CAttackB);
    parameter.m_pStateMachine->RegistStatus(Slashuur::BossStatus(Slashuur::STATUS_BOSS_TELEPORT),    new Slashuur::CBossTeleport);
    parameter.m_pStateMachine->RegistStatus(Slashuur::BossStatus(Slashuur::STATUS_BOSS_SCYTHE),      new Slashuur::CBossScythe);
    parameter.m_pStateMachine->RegistStatus(Slashuur::BossStatus(Slashuur::STATUS_BOSS_GROUND_BLAST),new Slashuur::CBossGroundBlast);
    parameter.m_pStateMachine->RegistStatus(Slashuur::BossStatus(Slashuur::STATUS_BOSS_SHOT),        new Slashuur::CBossShot);
    parameter.m_pStateMachine->RegistStatus(Slashuur::BossStatus(Slashuur::STATUS_BOSS_DRAIN),       new Slashuur::CBossDrain);

    Initialize(&parameter);

    m_pModuleMan->Include(CCircleShadowModule::New(this, 1.5f, 1.5f, false));

    m_pModuleMan->Include(new CBandanaModule(this,
                                             m_pModel,
                                             10,
                                             &Slashuur::BANDANA_OFFSET,
                                             CBandanaModule::BANDANACOLOR_SLASHUURGREY));
};


CSlashuur::~CSlashuur(void)
{
    ;
};
