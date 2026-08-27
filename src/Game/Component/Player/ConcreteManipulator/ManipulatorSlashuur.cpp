#include "ManipulatorSlashuur.hpp"

#include "Game/Component/Player/ConcretePlayerCharacter/Slashuur.hpp"
#include "Game/Component/Player/PlayerCharacter.hpp"

#if defined(TARGET_PC)
#include "System/PC/PCCrashReporter.hpp"
#define SLASHUUR_INPUT_TRACE(...) CPCCrashReporter::Breadcrumb(__VA_ARGS__)
#else
#define SLASHUUR_INPUT_TRACE(...) ((void)0)
#endif /* TARGET_PC */


namespace
{
    static bool IsTeleportCancelStatus(PLAYERTYPES::STATUS status)
    {
        switch (status)
        {
        case PLAYERTYPES::STATUS_ATTACK_A:
        case PLAYERTYPES::STATUS_ATTACK_AA:
        case PLAYERTYPES::STATUS_ATTACK_AAB:
        case PLAYERTYPES::STATUS_ATTACK_AAC:
        case PLAYERTYPES::STATUS_ATTACK_AABB:
        case PLAYERTYPES::STATUS_ATTACK_AABC:
        case PLAYERTYPES::STATUS_ATTACK_AABBB:
        case PLAYERTYPES::STATUS_ATTACK_AABBC:
        case PLAYERTYPES::STATUS_ATTACK_B:
        case PLAYERTYPES::STATUS_ATTACK_B_CHARGE:
            return true;

        default:
            return false;
        };
    };


    static const char* BossMoveName(Slashuur::STATUS status)
    {
        switch (status)
        {
        case Slashuur::STATUS_BOSS_TELEPORT:     return "teleport";
        case Slashuur::STATUS_BOSS_SCYTHE:       return "scythe";
        case Slashuur::STATUS_BOSS_GROUND_BLAST: return "ground_blast";
        case Slashuur::STATUS_BOSS_SHOT:         return "shot";
        case Slashuur::STATUS_BOSS_DRAIN:        return "drain";
        default:                                 return "unknown";
        };
    };
}; /* anonymous namespace */


CSlashuurManipulator::CSlashuurManipulator(CPlayerCharacter* pPlayerChr, int32 nControllerNo)
: CManipulator("sla_mp", pPlayerChr, nControllerNo)
{
    ;
};


CSlashuurManipulator::~CSlashuurManipulator(void)
{
    ;
};


void CSlashuurManipulator::BranchForStatus(PLAYERTYPES::STATUS status)
{
    if (IsTeleportCancelStatus(status) && RunTeleportCancelChord())
        return;

    CManipulator::BranchForStatus(status);
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


bool CSlashuurManipulator::RunTeleportCancelChord(void)
{
    if ((m_input.m_guard != GUARD_ON) || (m_input.m_dash != DASH_ON))
        return false;

    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);

    SLASHUUR_INPUT_TRACE("INPUT teleport cancel status=%d change begin",
                         static_cast<int32>(m_pPlayerChr->GetStatus()));
    PLAYERTYPES::STATUS result = m_pPlayerChr->ChangeStatus(
        static_cast<PLAYERTYPES::STATUS>(Slashuur::STATUS_BOSS_TELEPORT)
    );
    SLASHUUR_INPUT_TRACE("INPUT teleport cancel change returned status=%d",
                         static_cast<int32>(result));
    return (result == static_cast<PLAYERTYPES::STATUS>(Slashuur::STATUS_BOSS_TELEPORT));
};


bool CSlashuurManipulator::RunBossMoveChord(void)
{
    if (m_input.m_guard != GUARD_ON)
        return false;

    Slashuur::STATUS status = Slashuur::STATUS_BOSS_MAX;

    if (m_input.m_attack == ATTACK_A)
        status = Slashuur::STATUS_BOSS_SCYTHE;
    else if ((m_input.m_attack == ATTACK_B) || (m_input.m_attack == ATTACK_C))
        status = Slashuur::STATUS_BOSS_DRAIN;
    else if (m_input.m_jump == JUMP_ON)
        status = Slashuur::STATUS_BOSS_GROUND_BLAST;
    else if (m_input.m_knife == KNIFE_ON)
        status = Slashuur::STATUS_BOSS_SHOT;
    else if (m_input.m_dash == DASH_ON)
        status = Slashuur::STATUS_BOSS_TELEPORT;

    if (status == Slashuur::STATUS_BOSS_MAX)
        return false;

    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);

    SLASHUUR_INPUT_TRACE("INPUT boss move=%s status=%d change begin",
                         BossMoveName(status),
                         static_cast<int32>(status));
    m_pPlayerChr->ChangeStatus(static_cast<PLAYERTYPES::STATUS>(status));
    SLASHUUR_INPUT_TRACE("INPUT boss move=%s status=%d change returned",
                         BossMoveName(status),
                         static_cast<int32>(status));
    return true;
};
