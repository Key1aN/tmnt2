#pragma once

#if defined(TMNT2_DEBUG_TOOLS)

#include "AIThinkOrder.hpp"
#include "EnemyID.hpp"
#include "EnemyTypes.hpp"


class CEnemyCharacter;


//
//  Enemy AI trace (Debug Tools only).
//
//  Records what CBaseGeneralEnemyAIModerator (and the Grappler / Foot Ninja
//  AIs built on it) actually evaluated: every random roll with its threshold,
//  the eligibility checks of the action selector, and the order it produced.
//  Hooks only copy values the AI already computed. They never call the RNG,
//  never call AI functions, and never change control flow, so recording does
//  not change gameplay. With the trace disabled every hook is one branch.
//
class CEnemyAITrace
{
public:
    enum EVENT
    {
        EV_TARGET = 0,      // SearchTarget activity roll   idx=coordinator target, value=ratio
        EV_TARGET_KEEP,     // SearchTarget impulsiveness   value=ratio
        EV_TARGET_RESULT,   // SearchTarget result          idx=new target, value=before target
        EV_PATROL_RUN,      // ActionSelector outside patrol area, ratio of move
        EV_ELIGIBLE,        // ActionSelector checks        idx=ELIGIBLE flags
        EV_SELECT,          // ActionSelector result        idx=ORDER
        EV_FREQ,            // IsSatifyFrequency            idx=frequency index, value=table %
        EV_FALLBACK,        // an action handler refused    idx=ORDER that fell back to WAIT
        EV_WAIT_MOVE,       // OnActionOfWait random walk   value=ratio of move
        EV_WAIT_ACTIVITY,   // OnActionOfWait activity      value=ratio of activity
        EV_WAIT_NOPOINT,    // OnActionOfWait: no reachable random point
        EV_MOVE_ROLL,       // OnActionOfMove approach roll value=ratio of move
        EV_MOVE_ACTIVITY,   // OnActionOfMove walk-instead  value=ratio of activity
        EV_MOVE_FAIL,       // OnActionOfMove refused       idx=reason
        EV_UNDERWAIT,       // OnUnderWait activity roll    value=ratio of activity
        EV_GUARD_REACT,     // OnUnderWait guard check while waiting/moving
        EV_BACKAWAY,        // Foot Ninja back-away         value=cooldown left
        EV_ATTACK_BLOCKED,  // OnActionOfAttack refused     idx=0 no player, 1 unhittable state, 2 player down

        EV_NUM,
    };

    enum ELIGIBLE
    {
        ELIGIBLE_OUTSIDE_PATROL = (1 << 0),
        ELIGIBLE_NO_TARGET      = (1 << 1),
        ELIGIBLE_GUARD          = (1 << 2),
        ELIGIBLE_ATTACK         = (1 << 3),
        ELIGIBLE_RUN_ATTACK     = (1 << 4),
        ELIGIBLE_BACKAWAY       = (1 << 5),
        ELIGIBLE_JUMP_ATTACK    = (1 << 6),
        ELIGIBLE_MOVE_REFUSED   = (1 << 7),
    };

    enum MOVEFAIL
    {
        MOVEFAIL_NO_PLAYER = 0,
        MOVEFAIL_NO_PATH,
        MOVEFAIL_TOO_FAR,
        MOVEFAIL_FACING,
    };

    enum REASON
    {
        REASON_ACCEPT = 0,  // previous order finished (status set ACCEPT)
        REASON_REFUSE,      // previous order refused / idle timed out (status set REFUSE)
        REASON_THINKOVER,   // re-decided during a wait/move check
        REASON_FORCED,      // OnUnderWait forced a guard or back-away

        REASON_NUM,
    };

    // Display categories. These are colours for real ENEMYTYPES statuses,
    // not new AI states. See CategoryOf().
    enum CATEGORY
    {
        CAT_WAIT = 0,       // IDLE, THINKING
        CAT_MOVE,           // TURN, WALK*, RUN, DASH, JUMP*, AERIAL, TOUCHDOWN
        CAT_ATTACK,         // ATTACK_*, FIRE_*, THROW, JUMP_ATTACK
        CAT_GUARD,          // GUARD
        CAT_SPECIAL,        // STATUS_SPECIAL+ (Foot Ninja: back away)
        CAT_HIT,            // KNOCK .. THROWN_BACK, GETUP, STUN, DINDLE, ...
        CAT_OTHER,          // APPEAR, DEATH, HIDE, QUIT, RETREAT, COUNTERACT

        CAT_NUM,
    };

    struct EVENTREC
    {
        uint8 m_kind;
        uint8 m_pass;
        int16 m_idx;
        float m_value;
        float m_roll;
    };

    enum
    {
        DECISION_EVENT_MAX = 20,
        DECISION_HISTORY   = 12,
        SEGMENT_HISTORY    = 96,
        SLOT_MAX           = 16,
    };

    struct DECISION
    {
        float m_fTime;
        uint8 m_reason;
        uint8 m_order;          // CAIThinkOrder::ORDER
        int16 m_typeId;         // AIOT type of the order (move/attack id)
        int16 m_target;         // player number or -1
        float m_fTargetDist;
        int32 m_nEvent;
        bool  m_bEventOverflow;
        EVENTREC m_aEvent[DECISION_EVENT_MAX];

        // Checks made while the previous order ran (OnUnderWait every think
        // tick), summarised instead of logged one by one.
        int32 m_nWaitChecks;
        int32 m_nWaitActivityFail;
        int32 m_nWaitNoTarget;
        float m_fPrevOrderSec;
    };

    struct SEGMENT
    {
        float m_fStart;
        uint8 m_category;
        uint8 m_status;
    };

    struct SLOT
    {
        bool  m_bUsed;
        bool  m_bGeneralAI;
        uint32 m_hEnemy;
        int32 m_iLabel;             // 1-based display number, stable while alive
        ENEMYID::VALUE m_id;

        // live, written by the AI every frame it runs
        int16 m_target;
        float m_fTargetDist;
        float m_fTargetAngle;
        float m_fThinkInterval;
        float m_fSinceThink;
        float m_fBackAwayCooldown;
        bool  m_bHasBackAway;
        float m_fLastAIFrame;

        // sampled
        int32 m_status;
        uint8 m_category;
        float m_fStatusStart;
        bool  m_bInRange;
        int32 m_nAttackStarts;
        float m_afCategoryTime[CAT_NUM];

        // pending (current AI frame)
        DECISION m_pending;
        bool  m_bPendingOpen;

        // summaries since the last committed decision
        int32 m_nWaitChecks;
        int32 m_nWaitActivityFail;
        int32 m_nWaitNoTarget;
        float m_fLastDecisionTime;

        DECISION m_aDecision[DECISION_HISTORY];
        int32 m_nDecision;          // total committed (ring index = n % HISTORY)

        SEGMENT m_aSegment[SEGMENT_HISTORY];
        int32 m_nSegment;
    };

    struct OVERVIEW
    {
        int32 m_nActive;
        int32 m_anCategory[CAT_NUM];
        int32 m_nInRange;           // close enough and facing (attack-range test)
        int32 m_nAttacking;
        int32 m_nMaxAttacking;
        float m_fSpan;              // seconds observed
        float m_afAttackingTime[SLOT_MAX + 1];  // time with N enemies attacking
        int32 m_nAttackStarts;
        int32 m_nAttackStartsOverlapping;       // started while another was attacking
        int32 m_nDecisions;
        int32 m_anDecisionOrder[CAIThinkOrder::ORDER_MAX];
    };

public:
    static void SetEnabled(bool bEnable);
    static bool IsEnabled(void) { return m_bEnabled; };
    static void SetFileRecording(bool bEnable);
    static bool IsFileRecording(void);
    static void SetTimelinePaused(bool bPause);
    static bool IsTimelinePaused(void);
    static void Clear(void);

    // per-frame sampler, called by Debug Tools (gameplay only)
    static void Update(void);
    static void OnStageChanged(void);
    static float Now(void);
    static float TimelineNow(void);

    // AI hooks
    static void RunBegin(const CEnemyCharacter* pChr);
    static void RunEnd(const CEnemyCharacter* pChr, float fDist, float fAngle,
                       float fThinkInterval, float fSinceThink);
    static void Commit(const CEnemyCharacter* pChr, REASON reason, const CAIThinkOrder& rOrder);
    static void Discard(const CEnemyCharacter* pChr);
    static void Event(const CEnemyCharacter* pChr, EVENT kind, int32 idx, float value, float roll, bool bPass);
    static void BackAwayTimer(const CEnemyCharacter* pChr, float fCooldown);

    // queries for the visualizer
    static int32 GetSlotNum(void) { return SLOT_MAX; };
    static const SLOT* GetSlot(int32 index);
    static const SLOT* FindSlot(uint32 hEnemy);
    static const OVERVIEW& Overview(void);
    static const DECISION* GetDecision(const SLOT& rSlot, int32 ago);
    static CATEGORY CategoryOf(int32 status);
    static const char* CategoryName(int32 category);
    static void CategoryColor(int32 category, uint8* rgb);
    static const char* OrderName(int32 order);
    static const char* TypeName(int32 order, int32 typeId);
    static const char* ReasonName(int32 reason);
    static const char* FrequencyName(int32 index);
    static void FormatEvent(const EVENTREC& rEvent, const CEnemyCharacter* pChr, char* buffer, size_t capacity);
    static void WriteSummary(const char* pszReason);

private:
    static SLOT* slotFor(const CEnemyCharacter* pChr, bool bCreate);
    static void writeLine(const char* pszFormat, ...);
    static void writeDecision(const SLOT& rSlot, const DECISION& rDecision);

private:
    static bool m_bEnabled;
};


#define AITRACE(call)                       \
    do                                      \
    {                                       \
        if (CEnemyAITrace::IsEnabled())     \
            CEnemyAITrace::call;            \
    } while (0)

#else /* defined(TMNT2_DEBUG_TOOLS) */

#define AITRACE(call) ((void)0)

#endif /* defined(TMNT2_DEBUG_TOOLS) */
