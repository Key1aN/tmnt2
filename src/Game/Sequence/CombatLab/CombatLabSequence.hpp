#pragma once

#if defined(TMNT2_DEBUG_TOOLS)

#include "Game/Sequence/Stage/StageBaseSequence.hpp"
#include "Game/Sequence/Stage/StageSeqState.hpp"


//
//  Combat Laboratory stage sequence.
//
//  LOAD: retail CLoadStageSeqState for the selected Battle Nexus stage
//        (stcommon_n + player + arena LPAC, exactly like CNexusStageSequence).
//  PLAY: CPlayCombatLabSeqState - gimmicks are created (player start, arena
//        props) but the arena's G_SET enemy generators are never activated,
//        so there are no waves, timers, intro, mission info or results.
//
class CPlayCombatLabSeqState final : public IStageSeqState
{
public:
    CPlayCombatLabSeqState(void);
    virtual void OnAttach(CStageBaseSequence* pSeq, const void* pParam) override;
    virtual void OnDetach(CStageBaseSequence* pSeq) override;
    virtual bool OnMove(CStageBaseSequence* pSeq) override;

private:
    enum STEP
    {
        STEP_PLAY = 0,
        STEP_FADEOUT,
        STEP_FADEOUT_WAIT,
        STEP_END,
    };

    STEP m_step;
};


class CCombatLabSequence final : public CStageBaseSequence
{
public:
    static CProcess* Instance(void);

    CCombatLabSequence(void);
    virtual ~CCombatLabSequence(void);
    virtual bool OnAttach(const void* pParam) override;
    virtual void OnDetach(void) override;
    virtual void OnStateDetached(STATE state) override;
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
