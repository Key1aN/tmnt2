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

#if defined(TARGET_PC)
#include "System/PC/PCCrashReporter.hpp"
#define SLASHUUR_TRACE(...) CPCCrashReporter::Breadcrumb(__VA_ARGS__)
#else
#define SLASHUUR_TRACE(...) ((void)0)
#endif /* TARGET_PC */


namespace Slashuur
{
    static const float BOSS_GROUND_BLAST_JUMP_SPEED = 10.606602f;
    static const float BOSS_SCYTHE_RADIUS = 3.2f;
    static const int32 BOSS_DRAIN_DAMAGE = 30;
    static const int32 BOSS_MAGIC_BONE_ID = 3;


    static PLAYERTYPES::STATUS BossStatus(STATUS status)
    {
        return static_cast<PLAYERTYPES::STATUS>(status);
    };


    static void PlayScaledEffect(EFFECTID::VALUE idEffect, const RwV3d& position, float scale)
    {
        SLASHUUR_TRACE("EFFECT play id=%d position=(%.3f, %.3f, %.3f) scale=%.3f",
                       static_cast<int32>(idEffect),
                       position.x,
                       position.y,
                       position.z,
                       scale);
        uint32 hEffect = CEffectManager::Play(idEffect, &position);
        SLASHUUR_TRACE("EFFECT result id=%d handle=0x%08X",
                       static_cast<int32>(idEffect),
                       hEffect);
        if (hEffect)
            CEffectManager::SetScale(hEffect, scale);
    };


    static void RegistEnemySphereAttack(CPlayerCharacter& character,
                                        const RwV3d& position,
                                        float radius,
                                        int32 power,
                                        CHitAttackData::STATUS status)
    {
        SLASHUUR_TRACE("ATTACK register radius=%.3f power=%d status=%d position=(%.3f, %.3f, %.3f)",
                       radius,
                       power,
                       static_cast<int32>(status),
                       position.x,
                       position.y,
                       position.z);
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
        SLASHUUR_TRACE("ATTACK registered");
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
        SLASHUUR_TRACE("SLABALL begin downward=%d", (bDownward ? 1 : 0));
        RwV3d position = Math::VECTOR3_ZERO;
        character.GetPosition(&position);

        RwV3d offset = Math::VECTOR3_ZERO;
        /*
         * Boss Slashuur's CHR has a second position record (slot 1) that
         * resolves to bone 3 with a zero local offset. Playable Slashuur's
         * retail CHR has only slot 0, so requesting slot 1 dereferences a null
         * CHitSphereParameter. Both characters use the same model/skeleton;
         * resolve the authored bone directly instead of depending on the
         * boss-only CHR slot.
         */
        character.GetBonePosition(&offset, BOSS_MAGIC_BONE_ID, nullptr);
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

        SLASHUUR_TRACE("SLABALL play position=(%.3f, %.3f, %.3f) direction=(%.3f, %.3f, %.3f)",
                       position.x,
                       position.y,
                       position.z,
                       direction.x,
                       direction.y,
                       direction.z);
        uint32 hMagic = CMagicManager::Play(MAGICID::ID_SLABALL, &parameter);
        SLASHUUR_TRACE("SLABALL result handle=0x%08X", hMagic);
        if (hMagic && bDownward)
        {
            SLASHUUR_TRACE("SLABALL configure downward scale=1.73 speed=50");
            CMagicManager::SetScale(hMagic, 1.73f);
            CMagicManager::SetSpeed(hMagic, 50.0f);
        };

        SLASHUUR_TRACE("SLABALL play sound=4401");
        CGameSound::PlayObjectSE(&character, SDCODE_SE(4401));
        SLASHUUR_TRACE("SLABALL complete");
        return hMagic;
    };


    static int32 DrainNearbyEnemies(CPlayerCharacter& character)
    {
        SLASHUUR_TRACE("DRAIN scan begin");
        RwV3d playerPosition = Math::VECTOR3_ZERO;
        character.GetBodyPosition(&playerPosition);

        int32 drained = 0;
        const float radiusSq = 8.0f * 8.0f;
        int32 enemyMax = CGameProperty::GetEnemyMax();
        SLASHUUR_TRACE("DRAIN enemy count=%d", enemyMax);
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

            SLASHUUR_TRACE("DRAIN apply enemy=%d handle=0x%08X",
                           i,
                           pEnemy->GetHandle());
            enemyCharacter.OnMessageReceivedDamage(BOSS_DRAIN_DAMAGE);
            PlayScaledEffect(EFFECTID::ID_HPSTEAL_LIGHT, enemyPosition, 1.0f);
            ++drained;

            if (drained >= 3)
                break;
        };

        if (drained > 0)
        {
            SLASHUUR_TRACE("DRAIN heal player amount=%d", BOSS_DRAIN_DAMAGE * drained);
            character.OnMessageReceivedDamage(-(BOSS_DRAIN_DAMAGE * drained));
            PlayScaledEffect(EFFECTID::ID_HPSTEAL_LIGHT, playerPosition, 1.5f);
        };

        SLASHUUR_TRACE("DRAIN scan complete drained=%d", drained);
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
        SLASHUUR_TRACE("MOVE teleport attach begin");
        m_step = 0;

        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().ChangeMotion(PLAYERTYPES::MOTIONNAMES::IDLE, true);

        RwV3d position = Math::VECTOR3_ZERO;
        Character().GetFootPosition(&position);
        PlayScaledEffect(EFFECTID::ID_WARP_START, position, 1.5f);
        CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4396));
        SLASHUUR_TRACE("MOVE teleport attach complete");
    };


    void CBossTeleport::OnDetach(void)
    {
        SLASHUUR_TRACE("MOVE teleport detach");
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
                SLASHUUR_TRACE("MOVE teleport step=0 hide");
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
                SLASHUUR_TRACE("MOVE teleport step=1 relocate");
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
            {
                SLASHUUR_TRACE("MOVE teleport step=2 idle");
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            };
            break;

        default:
            break;
        };
    };


    void CBossScythe::OnAttach(void)
    {
        SLASHUUR_TRACE("MOVE scythe attach begin");
        m_step = 0;
        m_fPulseTime = 0.2f;

        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ResetVelocity();
        Character().ResetAcceleration();
        SLASHUUR_TRACE("MOVE scythe change motion=E2 begin");
        Character().ChangeMotion(MOTIONNAMES::SCYTHE_LOOP, true);
        SLASHUUR_TRACE("MOVE scythe change motion=E2 complete");

        RwV3d position = Math::VECTOR3_ZERO;
        Character().GetFootPosition(&position);
        PlayScaledEffect(EFFECTID::ID_WARP_OUT, position, 1.25f);
        SLASHUUR_TRACE("MOVE scythe play sound=4403");
        CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4403));
        SLASHUUR_TRACE("MOVE scythe attach complete");
    };


    void CBossScythe::OnDetach(void)
    {
        SLASHUUR_TRACE("MOVE scythe detach");
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
                SLASHUUR_TRACE("MOVE scythe pulse begin");
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
                SLASHUUR_TRACE("MOVE scythe pulse complete");
            };

            if (StateMachine().GetStatusDuration() >= 2.0f)
            {
                SLASHUUR_TRACE("MOVE scythe change motion=E3 begin");
                Character().ChangeMotion(MOTIONNAMES::SCYTHE_END, true);
                SLASHUUR_TRACE("MOVE scythe change motion=E3 complete");
                ++m_step;
            };
        }
        else if (Character().IsMotionEnd())
        {
            SLASHUUR_TRACE("MOVE scythe idle");
            StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
        };
    };


    void CBossGroundBlast::OnAttach(void)
    {
        SLASHUUR_TRACE("MOVE ground_blast attach begin");
        m_step = 0;
        m_bShot = false;

        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ResetVelocity();
        Character().ResetAcceleration();
        Character().SetCharacterFlag(CHARACTERTYPES::FLAG_FIXED_MODEL_ROTATION);
        SLASHUUR_TRACE("MOVE ground_blast change motion=D1 begin");
        Character().ChangeMotion(MOTIONNAMES::GROUND_BLAST_START, true);
        SLASHUUR_TRACE("MOVE ground_blast change motion=D1 complete");
        CGameSound::PlayAttackSE(m_pPlayerChr);
        SLASHUUR_TRACE("MOVE ground_blast attach complete");
    };


    void CBossGroundBlast::OnDetach(void)
    {
        SLASHUUR_TRACE("MOVE ground_blast detach step=%d shot=%d", m_step, (m_bShot ? 1 : 0));
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
                SLASHUUR_TRACE("MOVE ground_blast step=0 change motion=D2 begin");
                Character().ChangeMotion(MOTIONNAMES::GROUND_BLAST_JUMP, true);
                SLASHUUR_TRACE("MOVE ground_blast step=0 change motion=D2 complete");

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
                    SLASHUUR_TRACE("MOVE ground_blast step=1 apex velocityY=%.3f", velocity.y);
                    SLASHUUR_TRACE("MOVE ground_blast change motion=DAttack begin");
                    Character().ChangeMotion(MOTIONNAMES::GROUND_BLAST_FIRE, true);
                    SLASHUUR_TRACE("MOVE ground_blast change motion=DAttack complete");
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
                    SLASHUUR_TRACE("MOVE ground_blast step=2 landed mapHeight=%.3f", mapHeight);
                    Character().ResetVelocity();
                    Character().ResetAcceleration();
                    Character().ClearPlayerFlag(PLAYERTYPES::FLAG_AERIAL_STATUS);
                    SLASHUUR_TRACE("MOVE ground_blast change motion=D3 begin");
                    Character().ChangeMotion(MOTIONNAMES::GROUND_BLAST_END, true);
                    SLASHUUR_TRACE("MOVE ground_blast change motion=D3 complete");
                    ++m_step;
                };
            };
            break;

        case 3:
            if (Character().IsMotionEnd())
            {
                SLASHUUR_TRACE("MOVE ground_blast idle");
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            };
            break;

        default:
            break;
        };
    };


    void CBossShot::OnAttach(void)
    {
        SLASHUUR_TRACE("MOVE shot attach begin");
        m_bShot = false;

        Character().ResetVelocity();
        Character().ResetAcceleration();
        SLASHUUR_TRACE("MOVE shot change motion=ATTACK_KNIFE begin");
        Character().ChangeMotion(PLAYERTYPES::MOTIONNAMES::ATTACK_KNIFE, true);
        SLASHUUR_TRACE("MOVE shot change motion=ATTACK_KNIFE complete");
        CGameSound::PlayAttackSE(m_pPlayerChr);
        SLASHUUR_TRACE("MOVE shot attach complete");
    };


    void CBossShot::OnDetach(void)
    {
        SLASHUUR_TRACE("MOVE shot detach fired=%d", (m_bShot ? 1 : 0));
    };


    void CBossShot::OnRun(void)
    {
        if (!m_bShot && Character().TestCharacterFlag(CHARACTERTYPES::FLAG_OCCURED_TIMING))
        {
            SLASHUUR_TRACE("MOVE shot timing fired");
            SpawnPurpleBall(Character(), false);
            m_bShot = true;
        };

        if (Character().IsMotionEnd())
        {
            SLASHUUR_TRACE("MOVE shot idle");
            StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
        };
    };


    void CBossDrain::OnAttach(void)
    {
        SLASHUUR_TRACE("MOVE drain attach begin");
        m_step = 0;
        m_hEffect = 0;
        m_bDrained = false;

        Character().SetAttribute(PLAYERTYPES::ATTRIBUTE_INVINCIBILITY);
        Character().ResetVelocity();
        Character().ResetAcceleration();
        SLASHUUR_TRACE("MOVE drain change motion=C1 begin");
        Character().ChangeMotion(MOTIONNAMES::DRAIN_START, true);
        SLASHUUR_TRACE("MOVE drain change motion=C1 complete");
        CGameSound::PlayObjectSE(m_pPlayerChr, SDCODE_SE(4402));
        SLASHUUR_TRACE("MOVE drain attach complete");
    };


    void CBossDrain::OnDetach(void)
    {
        SLASHUUR_TRACE("MOVE drain detach step=%d effect=0x%08X drained=%d",
                       m_step,
                       m_hEffect,
                       (m_bDrained ? 1 : 0));
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
                SLASHUUR_TRACE("MOVE drain step=0 timing effect=SLA_SHADOW begin");
                RwV3d position = Math::VECTOR3_ZERO;
                Character().GetFootPosition(&position);
                position.y += 0.1f;
                m_hEffect = CEffectManager::Play(EFFECTID::ID_SLA_SHADOW, &position);
                SLASHUUR_TRACE("MOVE drain step=0 effect result=0x%08X", m_hEffect);
                if (m_hEffect)
                    CEffectManager::SetScale(m_hEffect, 1.25f);
                ++m_step;
            };
            break;

        case 1:
            if (Character().IsMotionEnd())
            {
                SLASHUUR_TRACE("MOVE drain step=1 change motion=C2 begin");
                Character().ChangeMotion(MOTIONNAMES::DRAIN_START2, true);
                SLASHUUR_TRACE("MOVE drain step=1 change motion=C2 complete");
                ++m_step;
            };
            break;

        case 2:
            if (!m_bDrained && Character().TestCharacterFlag(CHARACTERTYPES::FLAG_OCCURED_TIMING))
            {
                SLASHUUR_TRACE("MOVE drain step=2 timing drain begin");
                DrainNearbyEnemies(Character());
                SLASHUUR_TRACE("MOVE drain step=2 timing drain complete");
                m_bDrained = true;
            };

            if (Character().IsMotionEnd())
            {
                SLASHUUR_TRACE("MOVE drain step=2 change motion=C3 begin");
                Character().ChangeMotion(MOTIONNAMES::DRAIN_LOOP, true);
                SLASHUUR_TRACE("MOVE drain step=2 change motion=C3 complete");
                ++m_step;
            };
            break;

        case 3:
            if (StateMachine().GetStatusDuration() >= 3.0f)
            {
                SLASHUUR_TRACE("MOVE drain step=3 change motion=C4 begin");
                Character().ChangeMotion(MOTIONNAMES::DRAIN_END, true);
                SLASHUUR_TRACE("MOVE drain step=3 change motion=C4 complete");
                ++m_step;
            };
            break;

        case 4:
            if (Character().IsMotionEnd())
            {
                SLASHUUR_TRACE("MOVE drain idle");
                StateMachine().ChangeStatus(PLAYERTYPES::STATUS_IDLE);
            };
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
