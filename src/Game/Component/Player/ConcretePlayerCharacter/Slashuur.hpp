#pragma once

#include "Game/Component/Player/PlayerCharacter.hpp"
#include "Game/Component/Player/PlayerStatus.hpp"
#include "Game/Component/Player/ConcretePlayerStatus/PlayerStatusAttack.hpp"
#include "Game/Component/Player/ConcretePlayerStatus/PlayerStatusJump.hpp"


namespace Slashuur
{
    const RwV3d CHARGE_ATTACK_LOCAL_POSITION    = { 0.0f, 0.0f, 1.0f };
    const RwV3d BANDANA_OFFSET                  = { -0.02f, 0.237f, -0.038f };

    enum STATUS
    {
        STATUS_BOSS_TELEPORT = PLAYERTYPES::NORMALMAX,
        STATUS_BOSS_SCYTHE,
        STATUS_BOSS_GROUND_BLAST,
        STATUS_BOSS_SHOT,
        STATUS_BOSS_DRAIN,
        STATUS_BOSS_MAX,
    };

    namespace MOTIONNAMES
    {
        static const char* ATTACK_JUMP = "JAttack";

        static const char* DRAIN_START        = "C1";
        static const char* DRAIN_START2       = "C2";
        static const char* DRAIN_LOOP         = "C3";
        static const char* DRAIN_END          = "C4";

        static const char* GROUND_BLAST_START = "D1";
        static const char* GROUND_BLAST_JUMP  = "D2";
        static const char* GROUND_BLAST_FIRE  = "DAttack";
        static const char* GROUND_BLAST_END   = "D3";

        static const char* SCYTHE_LOOP        = "E2";
        static const char* SCYTHE_END         = "E3";
    };

    class CBossMoveStatus : public CStatus
    {
    public:
        virtual bool IsEnableChangeStatus(PLAYERTYPES::STATUS status) override;
    };

    class CBossTeleport : public CBossMoveStatus
    {
    public:
        virtual bool IsEnableChangeStatus(PLAYERTYPES::STATUS status) override;
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        int32 m_step;
    };

    class CBossScythe : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        int32 m_step;
        float m_fPulseTime;
    };

    class CBossGroundBlast : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        int32 m_step;
        bool m_bShot;
    };

    class CBossShot : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        bool m_bShot;
    };

    class CBossDrain : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        int32 m_step;
        uint32 m_hEffect;
        bool m_bDrained;
    };

    class CAttackJump : public CStatus
    {
    public:
        virtual bool IsEnableChangeStatus(PLAYERTYPES::STATUS status) override;
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;
    };

    class CAttackAABBC : public PlayerStatus::CAttackAABBC
    {
    public:
        virtual void OnDischargeWave(void) override;
    };

    class CAttackB : public PlayerStatus::CAttackB
    {
    public:
        virtual void OnDischargeWave(MAGIC_GENERIC::STEP step) override;
    };
}; /* namespace Slashuur */


class CSlashuur : public CPlayerCharacter
{
public:
    CSlashuur(GAMETYPES::COSTUME costume);
    virtual ~CSlashuur(void);
};
