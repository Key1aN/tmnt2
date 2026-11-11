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
        STATUS_BOSS_WIND_PRESS,                             // B, B
        STATUS_BOSS_THUNDER,                                // Hold B
        STATUS_BOSS_KUNAI_RAIN,                             // Guard + Jump
        STATUS_BOSS_COUNTER_SLASH,                          // Counter Stance answer
        STATUS_BOSS_BACK_AWAY,                              // Guard + B
        STATUS_BOSS_AAA,                                    // A, A, A (boss third hit)
        STATUS_BOSS_KUNAI_FAN,                              // Guard + Shuriken
        STATUS_BOSS_COUNTER_STANCE,                         // Guard + A
        STATUS_BOSS_MAX,
    };

    // Boss HP phases (boss Karai: base, warning <= 70% HP, final <= 35% HP).
    enum PHASE
    {
        PHASE_BASE = 0,
        PHASE_WARNING,
        PHASE_FINAL,
    };

    // Seconds between Thunder Special uses, per phase.
    static const float THUNDER_COOLDOWN_BASE    = 5.0f;
    static const float THUNDER_COOLDOWN_WARNING = 3.5f;
    static const float THUNDER_COOLDOWN_FINAL   = 1.5f;

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

    // Guard + B: boss C2 volley, 6 kunai fanned toward the nearest enemy from a hop.
    class CBossKunaiFan : public CBossMoveStatus
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

    // Guard + A: parry window. A hit during it is nullified and answered with
    // Counter Slash (Thunder Special in the final phase, like the boss).
    class CBossCounterStance : public CBossMoveStatus
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        int32 m_step;
        float m_fTimer;
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
        bool m_bThunder;
        bool m_bHold;
        float m_fThunderStart;
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

    // A, A, B, B: the boss BB slam with the Wind Press shockwave.
    class CBossAttackAABB : public PlayerStatus::CAttackAABB
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnRun(void) override;

    private:
        bool m_bMagic;
    };

    // A, A, B, B, B: the boss running attack (RunAttack) with a short lunge.
    class CBossAttackAABBB : public PlayerStatus::CAttackAABBB
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnRun(void) override;

    private:
        float m_fTimer;
    };

    // A, A, B, B, A+B: the Thunder finisher (C3) with an electric burst instead of
    // retail Karai's charge wave.
    class CBossAttackAABBC : public Karai::CAttackAABBC
    {
    public:
        virtual void OnAttach(void) override;
        virtual void OnDetach(void) override;
        virtual void OnDischargeWave(void) override;

    private:
        uint32 m_hEffectElectric;
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
    // Thunder countdown above every boss-variant Karai (called from the HUD draw).
    static void DrawBossHud(void);

    CKarai(GAMETYPES::COSTUME costume);
    virtual ~CKarai(void);
    virtual void Run(void) override;
    virtual CHARACTERTYPES::ATTACKRESULTTYPE OnDamage(CCharacterAttackCalculator& calculator) override;
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
    float GetThunderRemaining(void) const;
    void StartThunderCooldown(void);

    // HP phase (boss rules): damage scale, cooldown, kunai count.
    KaraiBoss::PHASE GetBossPhase(void) const { return m_phase; };
    float GetBossAttackScale(void) const;
    int32 GetKunaiRainNum(void) const;

    // Counter Stance: window flag (set by the status) and hit result (set by OnDamage).
    void SetCounterWindow(bool bSet) { m_bCounterWindow = bSet; if (bSet) m_bCounterHit = false; };
    bool TestCounterHit(void) const { return m_bCounterHit; };
    void ClearCounterHit(void) { m_bCounterHit = false; };
    void SetThunderChargeReady(bool bSet) { m_bThunderChargeReady = bSet; };
    bool IsThunderChargeReady(void) const { return m_bThunderChargeReady; };
    void SetThunderFromCharge(bool bSet) { m_bThunderFromCharge = bSet; };
    bool TestThunderFromCharge(void) const { return m_bThunderFromCharge; };

private:
    bool m_bBossVariant;
    bool m_bWindPressFollowup;
    bool m_bWindPressFromCombo;
    void UpdateBossPhase(void);
    void SetPhaseAura(KaraiBoss::PHASE phase);
    void FinishPhaseAura(void);

    bool m_bThunderChargeReady;
    bool m_bThunderFromCharge;
    float m_fThunderReadyTime;
    KaraiBoss::PHASE m_phase;
    bool m_bAuraShown;
    uint32 m_hAuraWarning;
    uint32 m_hAuraFinal;
    bool m_bCounterWindow;
    bool m_bCounterHit;
};