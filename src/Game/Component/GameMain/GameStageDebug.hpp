#pragma once

#if defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS)

class CGameStageDebug
{
public:
    static bool GODMODE;
    static float CAMERA_ZOOM_SCALE;
    static float CAMERA_MANUAL_SPEED;
    static bool CAMERA_MENU_CONTROL;
    static bool CAMERA_SUPPRESS_SWITCH_TRIGGER;
    static int32 STAGE_TICK;
    static int32 COUNTER;

    static void Reset(void);
};

#endif /* defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS) */
