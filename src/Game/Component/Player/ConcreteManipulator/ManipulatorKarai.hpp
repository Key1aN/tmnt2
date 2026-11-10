#pragma once

#include "Game/Component/Player/Manipulator.hpp"
#include "Game/Component/Player/ConcretePlayerCharacter/Karai.hpp"


class CKaraiManipulator : public CManipulator
{
public:
    CKaraiManipulator(CPlayerCharacter* pPlayerChr, int32 nControllerNo);
    virtual ~CKaraiManipulator(void);
    virtual void BranchForStatus(PLAYERTYPES::STATUS status) override;
    virtual void RunGrounding(void) override;
    virtual void RunAttackBCharge(void) override;
    virtual void RunGuardReady(void) override;
    virtual void RunGuard(void) override;

private:
    bool IsBossVariant(void) const;
    bool RunBossMoveChord(void);
    bool StartBossMove(KaraiBoss::STATUS status);
};
