#include "ManipulatorKarai.hpp"

#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/Player/ConcretePlayerCharacter/Karai.hpp"
#include "Game/Component/Player/PlayerCharacter.hpp"

#if defined(TARGET_PC)
#include "System/PC/PCCrashReporter.hpp"
#define KARAI_INPUT_TRACE(...) CPCCrashReporter::Breadcrumb(__VA_ARGS__)
#else
#define KARAI_INPUT_TRACE(...) ((void)0)
#endif /* TARGET_PC */


// Guard + B waits this long for A (Guard + A+B = Thunder) before it becomes Wind Press.
static const float GUARD_B_CHORD_WAIT = 0.085f;


CKaraiManipulator::CKaraiManipulator(CPlayerCharacter* pPlayerChr, int32 nControllerNo)
: CManipulator("kar_mp", pPlayerChr, nControllerNo)
, m_fThunderReadyTime(0.0f)
, m_fGuardBPending(-1.0f)
{
    ;
};


CKaraiManipulator::~CKaraiManipulator(void)
{
    ;
};


void CKaraiManipulator::BranchForStatus(PLAYERTYPES::STATUS status)
{
    if (IsBossVariant())
    {
        if (status == PLAYERTYPES::STATUS_ATTACK_B)
        {
            // B, B -> Wind Press. The status starts the slam at the B connect time.
            if ((m_input.m_attack == ATTACK_B) || (m_input.m_attack == ATTACK_C))
                static_cast<CKarai*>(m_pPlayerChr)->RequestWindPressFollowup();
            return;
        };

        // A pending Guard + B is only valid while she can still start a move.
        if ((m_fGuardBPending >= 0.0f) &&
            (status != PLAYERTYPES::STATUS_IDLE) &&
            (status != PLAYERTYPES::STATUS_WALK) &&
            (status != PLAYERTYPES::STATUS_RUN) &&
            (status != PLAYERTYPES::STATUS_GUARD_READY) &&
            (status != PLAYERTYPES::STATUS_GUARD))
        {
            m_fGuardBPending = -1.0f;
        };
    };

    CManipulator::BranchForStatus(status);
};


void CKaraiManipulator::RunGrounding(void)
{
    if (RunBossMoveChord())
        return;

    if (IsBossVariant() &&
        (m_input.m_dash == DASH_ON) &&
        (m_input.m_guard != GUARD_ON) &&
        (m_input.m_attack == ATTACK_OFF) &&
        (m_input.m_knife != KNIFE_ON))
    {
        StartBossMove(KaraiBoss::STATUS_BOSS_DASH_STRIKE);
        return;
    };

    CManipulator::RunGrounding();
};


void CKaraiManipulator::RunGuardReady(void)
{
    if (!RunBossMoveChord())
        CManipulator::RunGuardReady();
};


void CKaraiManipulator::RunGuard(void)
{
    if (!RunBossMoveChord())
        CManipulator::RunGuard();
};


bool CKaraiManipulator::IsBossVariant(void) const
{
    return ((m_pPlayerChr->GetID() == PLAYERID::ID_KAR) &&
            static_cast<const CKarai*>(m_pPlayerChr)->IsBossVariant());
};


bool CKaraiManipulator::StartBossMove(KaraiBoss::STATUS status)
{
    float fNow = CGameProperty::GetTotalElapsedTime();

    if ((status == KaraiBoss::STATUS_BOSS_THUNDER) && (fNow < m_fThunderReadyTime))
        return false;

    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);

    PLAYERTYPES::STATUS requested = static_cast<PLAYERTYPES::STATUS>(status);
    PLAYERTYPES::STATUS result = m_pPlayerChr->ChangeStatus(requested);
    KARAI_INPUT_TRACE("KARAI_BOSS move status=%d result=%d", static_cast<int32>(status), static_cast<int32>(result));

    if ((result == requested) && (status == KaraiBoss::STATUS_BOSS_THUNDER))
        m_fThunderReadyTime = fNow + KaraiBoss::THUNDER_COOLDOWN;

    return (result == requested);
};


// Boss variant only: Guard + A+B / B / Jump / Shuriken / Dash. Guard + B is the
// grab input in retail; on the boss model (no grab data) it becomes Wind Press.
// Guard + A alone does nothing (as in retail guard).
bool CKaraiManipulator::RunBossMoveChord(void)
{
    if (!IsBossVariant())
        return false;

    if (m_fGuardBPending >= 0.0f)
    {
        m_fGuardBPending += CGameProperty::GetElapsedTime();

        if (m_input.m_attack == ATTACK_C)
        {
            m_fGuardBPending = -1.0f;
            StartBossMove(KaraiBoss::STATUS_BOSS_THUNDER); // swallowed while recharging
            return true;
        };

        if ((m_fGuardBPending >= GUARD_B_CHORD_WAIT) || (m_input.m_attackCharge != ATTACK_CHARGE_ON))
        {
            m_fGuardBPending = -1.0f;
            StartBossMove(KaraiBoss::STATUS_BOSS_WIND_PRESS);
        };

        return true;
    };

    if (m_input.m_guard != GUARD_ON)
        return false;

    if (m_input.m_attack == ATTACK_C)
    {
        StartBossMove(KaraiBoss::STATUS_BOSS_THUNDER);
        return true; // also consumes the chord while the special recharges
    };

    if (m_input.m_attack == ATTACK_B)
    {
        m_fGuardBPending = 0.0f;
        return true;
    };

    // Guard + A has no boss move; swallow it so a following B forms Guard + A+B
    // instead of starting the retail A attack on the first guard frame.
    if (m_input.m_attack == ATTACK_A)
        return true;

    KaraiBoss::STATUS status = KaraiBoss::STATUS_BOSS_MAX;

    if (m_input.m_jump == JUMP_ON)
        status = KaraiBoss::STATUS_BOSS_KUNAI_RAIN;
    else if (m_input.m_knife == KNIFE_ON)
        status = KaraiBoss::STATUS_BOSS_COUNTER_SLASH;
    else if (m_input.m_dash == DASH_ON)
        status = KaraiBoss::STATUS_BOSS_BACK_AWAY;

    if (status == KaraiBoss::STATUS_BOSS_MAX)
        return false;

    StartBossMove(status);
    return true;
};
