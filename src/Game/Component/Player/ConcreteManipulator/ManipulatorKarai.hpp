#pragma once

#include "Game/Component/Player/Manipulator.hpp"


class CKaraiManipulator : public CManipulator
{
public:
    CKaraiManipulator(CPlayerCharacter* pPlayerChr, int32 nControllerNo);
    virtual ~CKaraiManipulator(void);
    virtual void RunGrounding(void) override;
    virtual void RunGuardReady(void) override;
    virtual void RunGuard(void) override;

private:
    bool IsBossVariant(void) const;
    bool RunBossMoveChord(void);

    float m_fThunderReadyTime;
};
