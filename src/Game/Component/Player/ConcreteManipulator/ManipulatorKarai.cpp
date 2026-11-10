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
, m_fThunderReadyTime(0.0f)
{
    ;
};


CKaraiManipulator::~CKaraiManipulator(void)
{
    ;
};


void CKaraiManipulator::RunGrounding(void)
{
    if (!RunBossMoveChord())
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


// Boss variant only: Guard + A/B/A+B/Jump/Shuriken/Dash. Guard + B is the grab
// input in retail; on the boss model (no grab data) it becomes Wind Press.
bool CKaraiManipulator::RunBossMoveChord(void)
{
    if ((m_input.m_guard != GUARD_ON) || !IsBossVariant())
        return false;

    KaraiBoss::STATUS status = KaraiBoss::STATUS_BOSS_MAX;

    if (m_input.m_attack == ATTACK_C)
        status = KaraiBoss::STATUS_BOSS_THUNDER;
    else if (m_input.m_attack == ATTACK_A)
        status = KaraiBoss::STATUS_BOSS_DASH_STRIKE;
    else if (m_input.m_attack == ATTACK_B)
        status = KaraiBoss::STATUS_BOSS_WIND_PRESS;
    else if (m_input.m_jump == JUMP_ON)
        status = KaraiBoss::STATUS_BOSS_KUNAI_RAIN;
    else if (m_input.m_knife == KNIFE_ON)
        status = KaraiBoss::STATUS_BOSS_COUNTER_SLASH;
    else if (m_input.m_dash == DASH_ON)
        status = KaraiBoss::STATUS_BOSS_BACK_AWAY;

    if (status == KaraiBoss::STATUS_BOSS_MAX)
        return false;

    float fNow = CGameProperty::GetTotalElapsedTime();
    if (status == KaraiBoss::STATUS_BOSS_THUNDER)
    {
        if (fNow < m_fThunderReadyTime)
            return true; // consume the chord while the special recharges
    };

    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);

    PLAYERTYPES::STATUS requested = static_cast<PLAYERTYPES::STATUS>(status);
    PLAYERTYPES::STATUS result = m_pPlayerChr->ChangeStatus(requested);
    KARAI_INPUT_TRACE("KARAI_BOSS move status=%d result=%d", static_cast<int32>(status), static_cast<int32>(result));

    if ((result == requested) && (status == KaraiBoss::STATUS_BOSS_THUNDER))
        m_fThunderReadyTime = fNow + KaraiBoss::THUNDER_COOLDOWN;

    return true;
};
