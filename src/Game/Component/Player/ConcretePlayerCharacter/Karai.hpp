#pragma once

#include "Game/Component/Player/PlayerCharacter.hpp"
#include "Game/Component/Player/PlayerStatus.hpp"
#include "Game/Component/Player/ConcretePlayerStatus/PlayerStatusAttack.hpp"
#include "Game/Component/Player/ConcretePlayerStatus/PlayerStatusJump.hpp"


namespace Karai
{
    const RwV3d CHARGE_ATTACK_LOCAL_POSITION = { 0.0f, 0.025f, 1.0f };

    namespace MOTIONNAMES
    {
        static const char* ATTACK_JUMP = "JAttack";
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
}; /* namespace Karai */


// Karai boss variant (R1 at confirm): boss Karai model, motions, and boss moves.
namespace KaraiBoss
{
    enum STATUS
    {
        STATUS_BOSS_DASH_STRIKE = PLAYERTYPES::NORMALMAX,   // Guard + A
        STATUS_BOSS_WIND_PRESS,                             // Guard + B (replaces grab)
        STATUS_BOSS_THUNDER,                                // Guard + A+B (C)
        STATUS_BOSS_KUNAI_RAIN,                             // Guard + Jump
        STATUS_BOSS_COUNTER_SLASH,                          // Guard + Shuriken
        STATUS_BOSS_BACK_AWAY,                              // Guard + Dash
        STATUS_BOSS_MAX,
    };

    // Seconds between Thunder Special uses.
    static const float THUNDER_COOLDOWN = 6.0f;

    class CBossMoveStatus : public CStatus
    {
    public:
        virtual bool IsEnableChangeStatus(PLAYERTYPES::STATUS status) override;
    };

    class CBossDashStrike : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        int32 m_step;
        float m_fTimer;
        float m_fDashTime;
    };

    class CBossWindPress : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        int32 m_step;
        bool m_bMagic;
    };

    class CBossThunder : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        void FinishEffects(void);

        int32 m_step;
        int32 m_chargeStep;
        float m_fTimer;
        float m_fDashTime;
        uint32 m_hEffectCharge;
        uint32 m_ahEffectElectric[2];
    };

    class CBossKunaiRain : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        int32 m_step;
        float m_fTimer;
        bool m_bShot;
    };

    class CBossCounterSlash : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;
    };

    class CBossBackAway : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;
    };

    class CBossAttackB : public Karai::CAttackB
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnDischargeWave(MAGIC_GENERIC::STEP step) override;

    private:
        bool m_bBossFinisher;
        uint32 m_hEffectElectric;
    };
}; /* namespace KaraiBoss */


class CKarai : public CPlayerCharacter
{
public:
    CKarai(GAMETYPES::COSTUME costume);
    virtual ~CKarai(void);
    virtual void ShootingKnife(void) override;

    bool IsBossVariant(void) const;

private:
    bool m_bBossVariant;
};