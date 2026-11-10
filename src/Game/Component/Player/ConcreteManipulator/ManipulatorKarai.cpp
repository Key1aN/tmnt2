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


CKaraiManipulator::CKaraiManipulator(CPlayerCharacter* pPlayerChr, int32 nControllerNo)
: CManipulator("kar_mp", pPlayerChr, nControllerNo)
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

        if (status == static_cast<PLAYERTYPES::STATUS>(KaraiBoss::STATUS_BOSS_AAA))
        {
            RunAttackCommon(); // dash cancel and direction, like the retail combo hits
            return;
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


void CKaraiManipulator::RunAttackBCharge(void)
{
    if (IsBossVariant() && (m_input.m_attackCharge == ATTACK_CHARGE_OFF))
    {
        // Hold B: a release after the Thunder charge is ready fires the special.
        CKarai* pKarai = static_cast<CKarai*>(m_pPlayerChr);
        if (pKarai->IsThunderChargeReady())
        {
            pKarai->SetThunderFromCharge(true);
            m_pPlayerChr->ChangeStatus(static_cast<PLAYERTYPES::STATUS>(KaraiBoss::STATUS_BOSS_THUNDER));
            if (m_pPlayerChr->GetStatus() == static_cast<PLAYERTYPES::STATUS>(KaraiBoss::STATUS_BOSS_THUNDER))
                return;
            pKarai->SetThunderFromCharge(false);
        };
    };

    CManipulator::RunAttackBCharge();
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
    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);

    PLAYERTYPES::STATUS requested = static_cast<PLAYERTYPES::STATUS>(status);
    PLAYERTYPES::STATUS result = m_pPlayerChr->ChangeStatus(requested);
    KARAI_INPUT_TRACE("KARAI_BOSS move status=%d result=%d", static_cast<int32>(status), static_cast<int32>(result));

    return (result == requested);
};


// Boss variant only: Guard + B / Jump / Shuriken / Dash. Guard + B is the grab
// input in retail; on the boss model (no grab data) it becomes Wind Press.
// Thunder Special is Hold B (see CKaraiManipulator::RunAttackBCharge).
bool CKaraiManipulator::RunBossMoveChord(void)
{
    if (!IsBossVariant() || (m_input.m_guard != GUARD_ON))
        return false;

    KaraiBoss::STATUS status = KaraiBoss::STATUS_BOSS_MAX;

    if (m_input.m_attack == ATTACK_B)
        status = KaraiBoss::STATUS_BOSS_WIND_PRESS;
    else if (m_input.m_jump == JUMP_ON)
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
