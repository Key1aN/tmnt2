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
        STATUS_BOSS_DASH_STRIKE = PLAYERTYPES::NORMALMAX,   // Dash
        STATUS_BOSS_WIND_PRESS,                             // B, B (also Guard + B, replaces grab)
        STATUS_BOSS_THUNDER,                                // Hold B
        STATUS_BOSS_KUNAI_RAIN,                             // Guard + Jump
        STATUS_BOSS_COUNTER_SLASH,                          // Guard + Shuriken
        STATUS_BOSS_BACK_AWAY,                              // Guard + Dash
        STATUS_BOSS_AAA,                                    // A, A, A (boss third hit)
        STATUS_BOSS_MAX,
    };

    // Seconds between Thunder Special uses.
    static const float THUNDER_COOLDOWN = 3.0f;

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
        virtual bool IsEnableChangeStatus(PLAYERTYPES::STATUS status) override;
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
        virtual bool IsEnableChangeStatus(PLAYERTYPES::STATUS status) override;
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        void FinishEffects(void);
        void StartDash(void);

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

    // Hold B: the Thunder Special charge (C1 crouch, three glows, electric
    // aura). Released after THUNDER_CHARGE_TIME (or held to the limit) it
    // dashes and finishes; released early it is a plain B. While Thunder is
    // recharging, B is always a plain B.
    class CBossAttackBCharge : public PlayerStatus::CAttackBCharge
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        void FinishEffects(void);

        bool m_bPlainB;
        bool m_bHold;
        int32 m_glow;
        uint32 m_hEffectCharge;
        uint32 m_hEffectElectric;
    };

    class CBossAttackB : public Karai::CAttackB
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;
        virtual void OnDischargeWave(MAGIC_GENERIC::STEP step) override;
    };

    // A, A: A goes to the boss third hit (AAA); B and C keep the retail chain.
    class CBossAttackAA : public PlayerStatus::CAttackAA
    {
    public:
        virtual void OnRun(void) override;
    };

    class CBossAttackAAA : public PlayerStatus::CAttackCommon
    {
    public:
        virtual bool IsEnableChangeStatus(PLAYERTYPES::STATUS status) override;
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;
    };

    // Plain Shuriken: the boss model has no ground throw motion, so she throws
    // from her quick slash (A) at a fixed time instead of a motion timing event.
    class CBossAttackKnife : public PlayerStatus::CAttackKnife
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        float m_fTimer;
        bool m_bShot;
    };
}; /* namespace KaraiBoss */


class CKarai : public CPlayerCharacter
{
public:
    CKarai(GAMETYPES::COSTUME costume);
    virtual ~CKarai(void);
    virtual void ShootingKnife(void) override;

    bool IsBossVariant(void) const;

    // B, B -> Wind Press: the manipulator requests the follow-up during ATTACK_B.
    void RequestWindPressFollowup(void) { m_bWindPressFollowup = true; };
    bool TestWindPressFollowup(void) const { return m_bWindPressFollowup; };
    void ClearWindPressFollowup(void) { m_bWindPressFollowup = false; };
    void SetWindPressFromCombo(bool bSet) { m_bWindPressFromCombo = bSet; };
    bool TestWindPressFromCombo(void) const { return m_bWindPressFromCombo; };

    // Thunder Special (Hold B): cooldown and charge hand-off.
    bool IsThunderReady(void) const;
    void StartThunderCooldown(void);
    void SetThunderChargeReady(bool bSet) { m_bThunderChargeReady = bSet; };
    bool IsThunderChargeReady(void) const { return m_bThunderChargeReady; };
    void SetThunderFromCharge(bool bSet) { m_bThunderFromCharge = bSet; };
    bool TestThunderFromCharge(void) const { return m_bThunderFromCharge; };

private:
    bool m_bBossVariant;
    bool m_bWindPressFollowup;
    bool m_bWindPressFromCombo;
    bool m_bThunderChargeReady;
    bool m_bThunderFromCharge;
    float m_fThunderReadyTime;
};