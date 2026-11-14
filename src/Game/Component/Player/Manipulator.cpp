#include "Manipulator.hpp"
#include "PlayerCharacter.hpp"

#include "ConcreteManipulator/ManipulatorLeonardo.hpp"
#include "ConcreteManipulator/ManipulatorRaphael.hpp"
#include "ConcreteManipulator/ManipulatorMichelangero.hpp"
#include "ConcreteManipulator/ManipulatorDonatello.hpp"
#include "ConcreteManipulator/ManipulatorSlashuur.hpp"
#include "ConcreteManipulator/ManipulatorCasey.hpp"
#include "ConcreteManipulator/ManipulatorKarai.hpp"
#include "ConcreteManipulator/ManipulatorSplinter.hpp"

#include "Game/Component/GameData/GameData.hpp"
#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/System/Misc/Gamepad.hpp"

#include <algorithm>

#if defined(TMNT2_DEBUG_TOOLS)
#include "Game/Component/CombatOverhaul/CombatOverhaul.hpp"
#endif /* defined(TMNT2_DEBUG_TOOLS) */

#if defined(TARGET_PC)
#include "System/PC/PCModFeatures.hpp"
#if defined(TMNT2_DEBUG_TOOLS)
#include "System/PC/PCPhysicalController.hpp"
#endif /* defined(TMNT2_DEBUG_TOOLS) */
#endif /* defined(TARGET_PC) */


/*static*/ CManipulator* CManipulator::New(CPlayerCharacter* pPlayerChr, int32 nControllerNo)
{
    switch (pPlayerChr->GetID())
    {
    case PLAYERID::ID_LEO:
        return new CLeonardoManipulator(pPlayerChr, nControllerNo);

    case PLAYERID::ID_RAP:
        return new CRaphaelManipulator(pPlayerChr, nControllerNo);

    case PLAYERID::ID_MIC:
        return new CMichelangeroManipulator(pPlayerChr, nControllerNo);

    case PLAYERID::ID_DON:
        return new CDonatelloManipulator(pPlayerChr, nControllerNo);

    case PLAYERID::ID_SLA:
        return new CSlashuurManipulator(pPlayerChr, nControllerNo);

    case PLAYERID::ID_CAS:
        return new CCaseyManipulator(pPlayerChr, nControllerNo);

    case PLAYERID::ID_KAR:
        return new CKaraiManipulator(pPlayerChr, nControllerNo);

    case PLAYERID::ID_SPL:
        return new CSplinterManipulator(pPlayerChr, nControllerNo);

    default:
        ASSERT(false);
        return nullptr;
    };
};


/*static*/ void CManipulator::Delete(CManipulator* pManip)
{
    ASSERT(pManip);    
    delete pManip;
};


CManipulator::CManipulator(const char* pszName, CPlayerCharacter* pPlayerChr, int32 nControllerNo)
: CGameObject(pszName, GAMEOBJECTTYPE::MANIPULATOR)
, m_pPlayerChr(pPlayerChr)
, m_input()
, m_nControllerNo(nControllerNo)
#if defined(TMNT2_DEBUG_TOOLS)
, m_ovBufferedAttack(ATTACK_OFF)
, m_fOvBufferedTime(0.0f)
, m_iOvRightStickSide(0)
#endif /* defined(TMNT2_DEBUG_TOOLS) */
#if defined(TARGET_PC)
, m_fSincePressA(1.0f)
, m_fSincePressB(1.0f)
#endif /* defined(TARGET_PC) */
{
	ClearInput();

    if (CPadStreamSwitch::m_mode)
    {
        STAGEID::VALUE stageId = CGameData::PlayParam().GetStage();

        bool bResult = m_padstream.Open(CPadStreamSwitch::m_mode, stageId, m_nControllerNo);
        ASSERT(bResult);
    };
};


CManipulator::~CManipulator(void)
{
    m_padstream.Close();
};


void CManipulator::Run(void)
{
    AnalyzeInputDevice();
    SetConfusionInput();
    SetSpecificAbilityFlag();
#if defined(TMNT2_DEBUG_TOOLS)
    if (RunCombatOverhaul(m_pPlayerChr->GetStatus()))
        return;
#endif /* defined(TMNT2_DEBUG_TOOLS) */
    BranchForStatus(m_pPlayerChr->GetStatus());
};


#if defined(TMNT2_DEBUG_TOOLS)
bool CManipulator::RunCombatOverhaul(PLAYERTYPES::STATUS status)
{
    CCombatOverhaul::OnManipulatorInput(*m_pPlayerChr, (m_input.m_move != MOVE_IDLE), m_input.m_fDirection);

    // Recorded demos (pad stream record / playback) keep retail input rules.
    if (!CCombatOverhaul::IsEnabled() || CPadStreamSwitch::m_mode)
    {
        m_ovBufferedAttack = ATTACK_OFF;
        return false;
    };

    uint32 uDigitalTrigger = m_padstream.GetDigitalTrigger(m_nControllerNo);

    // 11 lock-on: R3 (right stick click). The keyboard key is read by Debug Tools.
    if (uDigitalTrigger & CController::DIGITAL_R3)
        CCombatOverhaul::RequestLockOnToggle(m_pPlayerChr->GetPlayerNo());

    // 11 while locked, a right stick flick left / right moves the lock to
    // the next enemy on that side. One switch per flick: the stick has to
    // come back to the centre first.
    {
        int16 rightX = CController::GetAnalog(m_nControllerNo, CController::ANALOG_RSTICK_X);
#if defined(TARGET_PC)
        // On DirectInput pads the horizontal right stick axis is the one the
        // Debug Camera already reads (see CPCPhysicalController).
        rightX = CPCPhysicalController::GetDebugRightStickHorizontal();
#endif /* defined(TARGET_PC) */

        const int32 FLICK_ON = (TYPEDEF::SINT16_MAX * 6) / 10;
        const int32 FLICK_OFF = (TYPEDEF::SINT16_MAX * 3) / 10;

        int32 side = m_iOvRightStickSide;
        if ((rightX > -FLICK_OFF) && (rightX < FLICK_OFF))
            side = 0;
        else if (rightX >= FLICK_ON)
            side = 1;
        else if (rightX <= -FLICK_ON)
            side = -1;

        if ((side != 0) && (side != m_iOvRightStickSide))
            CCombatOverhaul::RequestLockSwitch(m_pPlayerChr->GetPlayerNo(), side);

        m_iOvRightStickSide = side;
    };

    // 4 combo break: Guard held + Jump pressed while the gauge is full.
    if ((m_input.m_guard == GUARD_ON) &&
        (m_input.m_jump == JUMP_ON) &&
        CCombatOverhaul::TryPlayerBurst(*m_pPlayerChr))
    {
        return true;
    };

    bool bDashRules = CCombatOverhaul::IsOn(CCombatOverhaul::FEATURE_DASH_ATTACK_CANCEL);
    bool bGuardRules = CCombatOverhaul::IsOn(CCombatOverhaul::FEATURE_GUARD_RECOVERY);

    m_fOvBufferedTime += CGameProperty::GetElapsedTime();

    switch (status)
    {
    case PLAYERTYPES::STATUS_DASH_CANCEL:
    case PLAYERTYPES::STATUS_DASH:
        // 13 remember an attack pressed during the dash itself
        if (bDashRules && (m_input.m_attack != ATTACK_OFF))
        {
            m_ovBufferedAttack = m_input.m_attack;
            m_fOvBufferedTime = 0.0f;
        };
        break;

    case PLAYERTYPES::STATUS_DASH_FINISH:
        if (bDashRules)
        {
            if ((m_input.m_attack == ATTACK_OFF) &&
                (m_ovBufferedAttack != ATTACK_OFF) &&
                (m_fOvBufferedTime <= 0.35f))
            {
                m_input.m_attack = m_ovBufferedAttack;
            };

            m_ovBufferedAttack = ATTACK_OFF;

            // 13 act straight out of the dash recovery
            bool bKnife = ((m_input.m_knife == KNIFE_ON) && m_pPlayerChr->IsEnableAttackKnife());
            if ((m_input.m_attack != ATTACK_OFF) ||
                bKnife ||
                (m_input.m_jump == JUMP_ON) ||
                (m_input.m_guard == GUARD_ON))
            {
                m_pPlayerChr->ForceIdle();
                RunGrounding();
                return true;
            };
        };
        break;

    case PLAYERTYPES::STATUS_ATTACK_B_CHARGE:
    case PLAYERTYPES::STATUS_ATTACK_B:
    case PLAYERTYPES::STATUS_ATTACK_AABBC:
    case PLAYERTYPES::STATUS_ATTACK_RUN:
    case PLAYERTYPES::STATUS_ATTACK_KNIFE:
        // 13 the strong attack (charging or released) can be dash-cancelled
        // at any time; the others, which have no retail dash cancel, once the
        // strike part of the motion is over.
        m_ovBufferedAttack = ATTACK_OFF;
        if (bDashRules &&
            (m_input.m_dash == DASH_ON) &&
            ((status == PLAYERTYPES::STATUS_ATTACK_B_CHARGE) ||
             (status == PLAYERTYPES::STATUS_ATTACK_B) ||
             (m_pPlayerChr->GetMotionTime() >= (m_pPlayerChr->GetMotionEndTime() * 0.45f))))
        {
            m_pPlayerChr->ForceIdle();

            if ((m_input.m_move != MOVE_IDLE) &&
                CCombatOverhaul::IsOn(CCombatOverhaul::FEATURE_DASH_STICK))
            {
                m_pPlayerChr->SetDirection(m_input.m_fDirection);
            };

            m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_DASH);
            return true;
        };
        break;

    case PLAYERTYPES::STATUS_GUARD:
        // 9 attack, dash or jump straight out of a held guard (Guard + B stays the lift)
        m_ovBufferedAttack = ATTACK_OFF;
        if (bGuardRules &&
            (m_input.m_lift == LIFT_OFF) &&
            ((m_input.m_attack == ATTACK_A) || (m_input.m_dash == DASH_ON) || (m_input.m_jump == JUMP_ON)))
        {
            m_pPlayerChr->ForceIdle();
            m_input.m_guard = GUARD_OFF;
            RunGrounding();
            return true;
        };
        break;

    case PLAYERTYPES::STATUS_GUARD_FINISH:
        // 9 the guard-lowering motion no longer locks the player
        m_ovBufferedAttack = ATTACK_OFF;
        if (bGuardRules &&
            ((m_input.m_attack != ATTACK_OFF) ||
             (m_input.m_knife == KNIFE_ON) ||
             (m_input.m_dash == DASH_ON) ||
             (m_input.m_jump == JUMP_ON) ||
             (m_input.m_guard == GUARD_ON) ||
             (m_input.m_move != MOVE_IDLE)))
        {
            m_pPlayerChr->ForceIdle();
            RunGrounding();
            return true;
        };
        break;

    default:
        m_ovBufferedAttack = ATTACK_OFF;
        break;
    };

    return false;
};
#endif /* defined(TMNT2_DEBUG_TOOLS) */


void CManipulator::BranchForStatus(PLAYERTYPES::STATUS status)
{
    switch (status)
    {
    case PLAYERTYPES::STATUS_IDLE:
        RunIdle();
        break;

    case PLAYERTYPES::STATUS_WALK:
        RunWalk();
        break;

    case PLAYERTYPES::STATUS_RUN:
        RunRun();
        break;

    case PLAYERTYPES::STATUS_JUMP:
        RunJump();
        break;

    case PLAYERTYPES::STATUS_JUMP_2ND:
        RunJump2nd();
        break;

    case PLAYERTYPES::STATUS_JUMP_WALL:
        RunJumpWall();
        break;

    case PLAYERTYPES::STATUS_AERIAL:
        RunAerial();
        break;

    case PLAYERTYPES::STATUS_AERIAL_MOVE:
        RunAerialMove();
        break;

    case PLAYERTYPES::STATUS_GUARD_READY:
        RunGuardReady();
        break;

    case PLAYERTYPES::STATUS_GUARD:
        RunGuard();
        break;

    case PLAYERTYPES::STATUS_ATTACK_A:
        RunAttackA();
        break;

    case PLAYERTYPES::STATUS_ATTACK_AA:
        RunAttackAA();
        break;

    case PLAYERTYPES::STATUS_ATTACK_AAB:
        RunAttackAAB();
        break;

    case PLAYERTYPES::STATUS_ATTACK_AAC:
        RunAttackAAC();
        break;

    case PLAYERTYPES::STATUS_ATTACK_AABB:
        RunAttackAABB();
        break;
        
    case PLAYERTYPES::STATUS_ATTACK_AABC:
        RunAttackAABC();
        break;

    case PLAYERTYPES::STATUS_ATTACK_AABBB:
        RunAttackAABBB();
        break;

    case PLAYERTYPES::STATUS_ATTACK_AABBC:
        RunAttackAABBC();        
        break;

    case PLAYERTYPES::STATUS_ATTACK_B_CHARGE:
        RunAttackBCharge();
        break;

    case PLAYERTYPES::STATUS_ATTACK_KNIFE:
        RunAttackKnife();
        break;

    case PLAYERTYPES::STATUS_ATTACK_KNIFE_JUMP:
        RunAttackKnifeJump();
        break;

    case PLAYERTYPES::STATUS_LIFT:
        RunLift();
        break;

    case PLAYERTYPES::STATUS_LIFT_WALK:
        RunLiftWalk();
        break;

    case PLAYERTYPES::STATUS_THROWN_COMBINATION:
        RunThrownCombination();
        break;

    case PLAYERTYPES::STATUS_FLYAWAY_BOUND_FRONT:
    case PLAYERTYPES::STATUS_FLYAWAY_BOUND_BACK:
        RunFlyawayBound();
        break;
            
    case PLAYERTYPES::STATUS_DINDLE:
    case PLAYERTYPES::STATUS_STUN:
    case PLAYERTYPES::STATUS_BIND:
        RunStatusDamage();
        break;

    case PLAYERTYPES::STATUS_PUSH:
        RunPush();
        break;

    default:
        break;
    };
};


void CManipulator::RunIdle(void)
{
    RunGrounding();
};


void CManipulator::RunWalk(void)
{
    RunGrounding();
};


void CManipulator::RunRun(void)
{
    RunGrounding();
};


void CManipulator::RunGrounding(void)
{
    PLAYERTYPES::STATUS status = PLAYERTYPES::STATUS_IDLE;
	bool bChangeStatus = true;

    if (m_input.m_attack == ATTACK_A)
    {
        status = PLAYERTYPES::STATUS_ATTACK_A;
    }
    else if (m_input.m_attack == ATTACK_B)
    {
        status = PLAYERTYPES::STATUS_ATTACK_B;
    }
    else if ((m_input.m_knife == KNIFE_ON) &&
              m_pPlayerChr->IsEnableAttackKnife())
    {
        status = PLAYERTYPES::STATUS_ATTACK_KNIFE;
    }
    else if (m_input.m_guard == GUARD_ON)
    {
        status = PLAYERTYPES::STATUS_GUARD_READY;
    }
    else if (m_input.m_dash == DASH_ON)
    {
        status = PLAYERTYPES::STATUS_DASH;
    }
    else if (m_input.m_jump == JUMP_ON)
    {
        status = PLAYERTYPES::STATUS_JUMP;
    }
    else if (m_input.m_move == MOVE_WALK)
    {
		if (m_pPlayerChr->GetStatus() == PLAYERTYPES::STATUS_WALK)
			bChangeStatus = false;

		status = PLAYERTYPES::STATUS_WALK;
    }
    else if (m_input.m_move == MOVE_RUN)
    {
		if (m_pPlayerChr->GetStatus() == PLAYERTYPES::STATUS_RUN)
			bChangeStatus = false;

        status = PLAYERTYPES::STATUS_RUN;
    }
	else
	{
        if (m_pPlayerChr->GetStatus() == PLAYERTYPES::STATUS_IDLE)
            bChangeStatus = false;

        status = PLAYERTYPES::STATUS_IDLE;
    };

#if defined(TMNT2_DEBUG_TOOLS)
    // Combat Overhaul 12: turn to the stick before the dash fixes its direction.
    if ((status == PLAYERTYPES::STATUS_DASH) &&
        (m_input.m_move != MOVE_IDLE) &&
        CCombatOverhaul::IsOn(CCombatOverhaul::FEATURE_DASH_STICK))
    {
        m_pPlayerChr->SetDirection(m_input.m_fDirection);
    };
#endif /* defined(TMNT2_DEBUG_TOOLS) */

    if (bChangeStatus)
		m_pPlayerChr->ChangeStatus(status);

    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);
};


void CManipulator::RunJump(void)
{
    if (!RunAerialCommon() && m_input.m_move)
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_AERIAL_MOVE);
};


void CManipulator::RunJump2nd(void)
{
    if (!RunAerialCommon() && m_input.m_move)
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_AERIAL_MOVE);
};


void CManipulator::RunJumpWall(void)
{
    RunAerialCommon();
};


void CManipulator::RunAerial(void)
{
    if (!RunAerialCommon() && m_input.m_move)
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_AERIAL_MOVE);
};


void CManipulator::RunAerialMove(void)
{
    if (!RunAerialCommon())
    {
        if (m_input.m_move)
            m_pPlayerChr->SetDirection(m_input.m_fDirection);
        else
            m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_AERIAL);
    };
};


bool CManipulator::RunAerialCommon(void)
{
    bool bResult = true;

    if (m_input.m_attack &&
        m_pPlayerChr->IsEnableAttackJump())
    {
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_ATTACK_JUMP);
    }
    else if ((m_input.m_knife == KNIFE_ON) &&
              m_pPlayerChr->IsEnableAttackKnifeJump())
    {
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_ATTACK_KNIFE_JUMP);
    }
    else if ((m_input.m_jump == JUMP_ON) &&
              m_pPlayerChr->IsEnableJumpWall())
    {
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_JUMP_WALL);
    }
    else if ((m_input.m_jump == JUMP_ON) &&
              m_pPlayerChr->IsEnableJump2nd())
    {
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_JUMP_2ND);
    }
    else
    {
        bResult = false;
    };
    
    return bResult;
};


void CManipulator::RunGuardReady(void)
{
    RunGuardCommon();
};


void CManipulator::RunGuard(void)
{
    if (m_input.m_guard)
        RunGuardCommon();
    else
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_IDLE);
};


void CManipulator::RunGuardCommon(void)
{
    if (m_input.m_lift == LIFT_ON)
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_LIFT);
};


void CManipulator::RunAttackA(void)
{
    RunAttackCommon();
};


void CManipulator::RunAttackAA(void)
{
    RunAttackCommon();
};


void CManipulator::RunAttackAAB(void)
{
    RunAttackCommon();
};


void CManipulator::RunAttackAAC(void)
{
    RunAttackCommon();
};


void CManipulator::RunAttackAABB(void)
{
    RunAttackCommon();
};


void CManipulator::RunAttackAABC(void)
{
    RunAttackCommon();
};


void CManipulator::RunAttackAABBB(void)
{
    RunAttackCommon();
};


void CManipulator::RunAttackAABBC(void)
{
    ;
};


void CManipulator::RunAttackCommon(void)
{
    if (m_input.m_dash == DASH_ON)
    {
#if defined(TMNT2_DEBUG_TOOLS)
        // Combat Overhaul 12: a dash cancel also goes where the stick points.
        if ((m_input.m_move != MOVE_IDLE) &&
            CCombatOverhaul::IsOn(CCombatOverhaul::FEATURE_DASH_STICK) &&
            m_pPlayerChr->IsEnableChangeStatus(PLAYERTYPES::STATUS_DASH_CANCEL))
        {
            m_pPlayerChr->SetDirection(m_input.m_fDirection);
        };
#endif /* defined(TMNT2_DEBUG_TOOLS) */
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_DASH_CANCEL);        
    }
    else
    {
        switch (m_input.m_attack)
        {
        case ATTACK_A:
            m_pPlayerChr->RequestAttackA();
            break;

        case ATTACK_B:
            m_pPlayerChr->RequestAttackB();
            break;

        case ATTACK_C:
            m_pPlayerChr->RequestAttackC();
            break;

        default:
            break;
        };

#if defined(TMNT2_DEBUG_TOOLS)
        // Combat Overhaul 11 / 14 / 15: lock-on, free steering and soft
        // targeting replace the retail 90 deg/s turn while any of them is on.
        if (CCombatOverhaul::SteerAttack(*m_pPlayerChr, (m_input.m_move != MOVE_IDLE), m_input.m_fDirection))
            return;
#endif /* defined(TMNT2_DEBUG_TOOLS) */

        if (m_input.m_move)
            m_pPlayerChr->RotateDirection(m_input.m_fDirection);
    };
};


void CManipulator::RunAttackBCharge(void)
{
    if (m_input.m_attackCharge == ATTACK_CHARGE_OFF)
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_ATTACK_B);
};


void CManipulator::RunAttackKnife(void)
{
    if ((m_input.m_knife == KNIFE_ON) && m_pPlayerChr->IsEnableAttackKnife())
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_ATTACK_KNIFE);
};


void CManipulator::RunAttackKnifeJump(void)
{
    if ((m_input.m_knife == KNIFE_ON) && m_pPlayerChr->IsEnableAttackKnifeJump())
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_ATTACK_KNIFE_JUMP);
    else
        m_pPlayerChr->SetDirection(m_input.m_fDirection);
};


void CManipulator::RunLift(void)
{
    RunLiftCommon();
};


void CManipulator::RunLiftWalk(void)
{
    RunLiftCommon();
};


void CManipulator::RunLiftCommon(void)
{
    m_pPlayerChr->LiftInfo().m_bMissThrow = true;
    
    if (m_input.m_attack == ATTACK_B)
    {
        m_pPlayerChr->LiftInfo().m_bMissThrow = false;
        m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_THROW);
    }
    else
    {
		if (m_input.m_move)
		{
            if (((m_input.m_move > MOVE_IDLE) && (m_input.m_move <= MOVE_RUN)) &&
                (m_pPlayerChr->GetStatus() != PLAYERTYPES::STATUS_LIFT_WALK))
            {
                m_pPlayerChr->LiftInfo().m_bMissThrow = false;
                m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_LIFT_WALK);
            };
		}
        else if (m_pPlayerChr->GetStatus() != PLAYERTYPES::STATUS_LIFT)
        {
            m_pPlayerChr->LiftInfo().m_bMissThrow = false;
            m_pPlayerChr->ChangeStatus(PLAYERTYPES::STATUS_LIFT);
        };
    };

    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);
};


void CManipulator::RunThrownCombination(void)
{
    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);
};


void CManipulator::RunFlyawayBound(void)
{
    if (m_input.m_passive == PASSIVE_ON)
        m_pPlayerChr->SetPlayerFlag(PLAYERTYPES::FLAG_REQUEST_PASSIVE);
};


void CManipulator::RunStatusDamage(void)
{
    if (m_input.m_recover == RECOVER_ON)
        m_pPlayerChr->SetPlayerFlag(PLAYERTYPES::FLAG_REQUEST_RECOVER);
};


void CManipulator::RunPush(void)
{
    if (m_input.m_move)
        m_pPlayerChr->SetDirection(m_input.m_fDirection);
};


void CManipulator::AnalyzeInputDevice(void)
{
    ClearInput();

    m_padstream.GetPadData(m_nControllerNo);

    RwV3d vInputVector = Math::VECTOR3_ZERO;
    float fInputVectorLen = 0.0f;

    AnalyzeInputVector(vInputVector, fInputVectorLen);

    float fInputVecScale = 1.0f;
#ifdef TARGET_WEB
    fInputVecScale = 0.7f;
#endif /* TARGET_WEB */

    if (fInputVectorLen <= (0.4f * fInputVecScale))
    {
        m_input.m_move = MOVE_IDLE;
        m_input.m_fDirection = 0.0f;
    }
    else
    {
        m_input.m_move = (fInputVectorLen >= (0.9f * fInputVecScale) ? MOVE_RUN : MOVE_WALK);

        RwMatrix modeling;
        CGameProperty::GetCameraFrameMatrix(&modeling);

        RwV3d vecRight = Math::VECTOR3_ZERO;
        Math::Vec3_Normalize(&vecRight, &modeling.right);
        Math::Vec3_Scale(&vecRight, &vecRight, vInputVector.x);

        RwV3d vecAt = Math::VECTOR3_ZERO;
        Math::Vec3_Normalize(&vecAt, &modeling.at);
        Math::Vec3_Scale(&vecAt, &vecAt, vInputVector.z);

        RwV3d vecTarget = Math::VECTOR3_ZERO;
        Math::Vec3_Add(&vecTarget, &vecAt, &vecRight);
        vecTarget.y = 0.0f;
        
        Math::Vec3_Normalize(&vecTarget, &vecTarget);

        m_input.m_fDirection = Math::ATan2(-vecTarget.x, -vecTarget.z);
    };

    uint32 uDigital = m_padstream.GetDigital(m_nControllerNo);
    uint32 uDigitalTrigger = m_padstream.GetDigitalTrigger(m_nControllerNo);

    if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_DASH))
        m_input.m_dash = DASH_ON;

    if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_JUMP))
        m_input.m_jump = JUMP_ON;
    
    if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_A) &&
        IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_B))
    {
        m_input.m_attack = ATTACK_C;
    }
#if defined(TARGET_PC)
    // Retail only reads A+B as C when both presses land on the same frame (16 ms).
    // Also accept the second press up to CHORD_WINDOW after the first while the
    // first button is still held. The first press is never delayed: C replaces
    // its queued combo request, so A then B (or B then A) still forms C.
    else if (IsChordWithHeldButton(uDigital, uDigitalTrigger))
    {
        m_input.m_attack = ATTACK_C;
    }
#endif /* defined(TARGET_PC) */
    else if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_B))
    {
        m_input.m_attack = ATTACK_B;
    }
    else if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_A))
    {
        m_input.m_attack = ATTACK_A;
    };

#if defined(TARGET_PC)
    UpdateChordTimers(uDigitalTrigger);
#endif /* defined(TARGET_PC) */

    if (IGamepad::CheckFunction(uDigital, IGamepad::FUNCTION_ATTACK_B))
        m_input.m_attackCharge = ATTACK_CHARGE_ON;
    
    if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_SHOT))
        m_input.m_knife = KNIFE_ON;

    if (IGamepad::CheckFunction(uDigital, IGamepad::FUNCTION_GUARD))
        m_input.m_guard = GUARD_ON;

    if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_B) &&
        IGamepad::CheckFunction(uDigital, IGamepad::FUNCTION_GUARD))
        m_input.m_lift = LIFT_ON;

    uint32 uRecorverMask = CController::DIGITAL_RUP
                         | CController::DIGITAL_RDOWN
                         | CController::DIGITAL_RLEFT
                         | CController::DIGITAL_RRIGHT;
    if (uDigitalTrigger & uRecorverMask)
        m_input.m_recover = RECOVER_ON;

    if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_A) ||
        IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_B) ||
        IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_JUMP))
        m_input.m_passive = PASSIVE_ON;

    m_padstream.GetInput(&m_input, sizeof(m_input));
};


void CManipulator::AnalyzeInputVector(RwV3d& rvInputVector, float& rfInputVectorLength)
{
    float x =  static_cast<float>(m_padstream.GetAnalogX(m_nControllerNo));
    float y = -static_cast<float>(m_padstream.GetAnalogY(m_nControllerNo));

    x = (x >= 0.0f ? (x / static_cast<float>(TYPEDEF::SINT16_MAX)) : -(x / static_cast<float>(TYPEDEF::SINT16_MIN)));
    y = (y >= 0.0f ? (y / static_cast<float>(TYPEDEF::SINT16_MAX)) : -(y / static_cast<float>(TYPEDEF::SINT16_MIN)));

    rvInputVector.x = x;
    rvInputVector.y = 0.0f;
    rvInputVector.z = y;
    
    rfInputVectorLength = Math::Vec3_Length(&rvInputVector);

#if defined(TARGET_PC) || defined(TARGET_WEB)
    uint32 digitalMovementMask = CController::DIGITAL_LUP
                               | CController::DIGITAL_LDOWN
                               | CController::DIGITAL_LLEFT
                               | CController::DIGITAL_LRIGHT;

    uint32 digital = m_padstream.GetDigital(m_nControllerNo);
    if (!(digital & digitalMovementMask))
        return;
    
    float xDigital = 0.0f;
    float yDigital = 0.0f;

    if (digital & CController::DIGITAL_LLEFT)
        xDigital = -1.0f;
    else if (digital & CController::DIGITAL_LRIGHT)
        xDigital = 1.0f;

    if (digital & CController::DIGITAL_LUP)
        yDigital = -1.0f;
    else if (digital & CController::DIGITAL_LDOWN)
        yDigital = 1.0f;
    
    rvInputVector.x = xDigital;
    rvInputVector.y = 0.0f;
    rvInputVector.z = yDigital;
    rfInputVectorLength = 1.0f;
#endif /* defined(TARGET_PC) || defined(TARGET_WEB) */
};


void CManipulator::SetSpecificAbilityFlag(void)
{
    /* push flag ctrl */
    if (m_input.m_move == MOVE_RUN)
        m_pPlayerChr->SetPlayerFlag(PLAYERTYPES::FLAG_PUSH);
    else
        m_pPlayerChr->ClearPlayerFlag(PLAYERTYPES::FLAG_PUSH);

    /* console flag ctrl */
    if (m_input.m_attack)
        m_pPlayerChr->SetPlayerFlag(PLAYERTYPES::FLAG_CONSOLE);
    else
        m_pPlayerChr->ClearPlayerFlag(PLAYERTYPES::FLAG_CONSOLE);
};


void CManipulator::SetConfusionInput(void)
{
	if (m_pPlayerChr->TestAttribute(PLAYERTYPES::ATTRIBUTE_CONFUSION))
	{
		m_input.m_fDirection += MATH_PI;
		if (m_input.m_fDirection > MATH_PI2)
			m_input.m_fDirection -= MATH_PI2;
	};
};


void CManipulator::ClearInput(void)
{
    m_input.m_move          = MOVE_IDLE;
    m_input.m_dash          = DASH_OFF;
    m_input.m_jump          = JUMP_OFF;
    m_input.m_attack        = ATTACK_OFF;
    m_input.m_attackCharge  = ATTACK_CHARGE_OFF;
    m_input.m_knife         = KNIFE_OFF;
    m_input.m_guard         = GUARD_OFF;
    m_input.m_lift          = LIFT_OFF;
    m_input.m_recover       = RECOVER_OFF;
    m_input.m_passive       = PASSIVE_OFF;
    m_input.m_fDirection    = 0.0f;
};

#if defined(TARGET_PC)
static const float CHORD_WINDOW = 0.085f; // ~5 frames at 60 fps


bool CManipulator::IsChordWithHeldButton(uint32 uDigital, uint32 uDigitalTrigger) const
{
    if (!CPCModFeatures::IsComboAssistEnabled())
        return false;

    bool bTrigA = IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_A);
    bool bTrigB = IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_B);
    bool bHoldA = IGamepad::CheckFunction(uDigital, IGamepad::FUNCTION_ATTACK_A);
    bool bHoldB = IGamepad::CheckFunction(uDigital, IGamepad::FUNCTION_ATTACK_B);

    float fElapsed = CGameProperty::GetElapsedTime();

    if (bTrigB && bHoldA && ((m_fSincePressA + fElapsed) <= CHORD_WINDOW))
        return true;

    if (bTrigA && bHoldB && ((m_fSincePressB + fElapsed) <= CHORD_WINDOW))
        return true;

    return false;
};


void CManipulator::UpdateChordTimers(uint32 uDigitalTrigger)
{
    float fElapsed = CGameProperty::GetElapsedTime();
    m_fSincePressA = std::min(m_fSincePressA + fElapsed, 1.0f);
    m_fSincePressB = std::min(m_fSincePressB + fElapsed, 1.0f);

    if (m_input.m_attack == ATTACK_C)
    {
        // A chord consumes both presses so a third press cannot chord again.
        m_fSincePressA = 1.0f;
        m_fSincePressB = 1.0f;
        return;
    };

    if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_A))
        m_fSincePressA = 0.0f;

    if (IGamepad::CheckFunction(uDigitalTrigger, IGamepad::FUNCTION_ATTACK_B))
        m_fSincePressB = 0.0f;
};
#endif /* defined(TARGET_PC) */
