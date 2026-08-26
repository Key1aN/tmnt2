#include "ManipulatorSlashuur.hpp"

#include "Game/Component/Player/ConcretePlayerCharacter/Slashuur.hpp"
#include "Game/Component/Player/PlayerCharacter.hpp"


CSlashuurManipulator::CSlashuurManipulator(CPlayerCharacter* pPlayerChr, int32 nControllerNo)
: CManipulator("sla_mp", pPlayerChr, nControllerNo)
{
    ;
};


CSlashuurManipulator::~CSlashuurManipulator(void)
{
    ;
};


void CSlashuurManipulator::RunGrounding(void)
{
    if (!RunBossMoveChord())
        CManipulator::RunGrounding();
};


void CSlashuurManipulator::RunGuardReady(void)
{
    if (!RunBossMoveChord())
        CManipulator::RunGuardReady();
};


void CSlashuurManipulator::RunGuard(void)
{
    if (!RunBossMoveChord())
        CManipulator::RunGuard();
};


bool CSlashuurManipulator::RunBossMoveChord(void)
{
    if (m_input.m_guard != GUARD_ON)
        return false;

    Slashuur::STATUS status = Slashuur::STATUS_BOSS_MAX;

    if (m_input.m_attack == ATTACK_A)
        status = Slashuur::STATUS_BOSS_TELEPORT;
    else if ((m_input.m_attack == ATTACK_B) || (m_input.m_attack == ATTACK_C))
        status = Slashuur::STATUS_BOSS_SCYTHE;
    else if (m_input.m_jump == JUMP_ON)
        status = Slashuur::STATUS_BOSS_GROUND_BLAST;
    else if (m_input.m_knife == KNIFE_ON)
        status = Slashuur::STATUS_BOSS_SHOT;
    else if (m_input.m_dash == DASH_ON)
        status = Slashuur::STATUS_BOSS_DRAIN;

    if (status == Slashuur::STATUS_BOSS_MAX)
        return false;

    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);

    m_pPlayerChr->ChangeStatus(static_cast<PLAYERTYPES::STATUS>(status));
    return true;
};
