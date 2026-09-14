#pragma once

#if defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS)

class CHitDebug
{
public:
    static bool SHOW_HIT_BODY;
    static bool SHOW_HIT_ATTACK;
    static bool SHOW_HIT_CATCH;
    static bool SHOW_HIT_CATCH_NO;
};

#endif /* defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS) */
