#pragma once

#include "Game/Component/Player/Manipulator.hpp"


class CSlashuurManipulator : public CManipulator
{
public:
    CSlashuurManipulator(CPlayerCharacter* pPlayerChr, int32 nControllerNo);
    virtual ~CSlashuurManipulator(void);
    virtual void BranchForStatus(PLAYERTYPES::STATUS status) override;
    virtual void RunGrounding(void) override;
    virtual void RunGuardReady(void) override;
    virtual void RunGuard(void) override;

private:
    bool RunTeleportCancelChord(void);
    bool RunBossMoveChord(void);
};
