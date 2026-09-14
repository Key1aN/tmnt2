#pragma once

#if defined(TMNT2_DEBUG_TOOLS)

#include "System/Common/Process/Process.hpp"


class CDebugToolsProcess final : public CProcess
{
public:
    static CProcess* Instance(void);
    static void Initialize(CProcess* pSender);
    static void Terminate(CProcess* pSender);

    CDebugToolsProcess(void);
    virtual ~CDebugToolsProcess(void);
    virtual bool Attach(void) override;
    virtual void Detach(void) override;
    virtual void Move(void) override;
    virtual void Draw(void) const override;

private:
    class CImpl;
    CImpl* m_pImpl;
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
