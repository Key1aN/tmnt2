#include "GameStageDebug.hpp"

#if defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS)

/*static*/ bool CGameStageDebug::GODMODE = false;
/*static*/ float CGameStageDebug::CAMERA_ZOOM_SCALE = 1.0f;
/*static*/ float CGameStageDebug::CAMERA_MANUAL_SPEED = 20.0f;
/*static*/ bool CGameStageDebug::CAMERA_MENU_CONTROL = false;
/*static*/ bool CGameStageDebug::CAMERA_SUPPRESS_SWITCH_TRIGGER = false;
/*static*/ int32 CGameStageDebug::STAGE_TICK = 0;
/*static*/ int32 CGameStageDebug::COUNTER = 0;


/*static*/ void CGameStageDebug::Reset(void)
{
    GODMODE = false;
    CAMERA_ZOOM_SCALE = 1.0f;
    CAMERA_MANUAL_SPEED = 20.0f;
    CAMERA_MENU_CONTROL = false;
    CAMERA_SUPPRESS_SWITCH_TRIGGER = false;
    STAGE_TICK = 0;
    COUNTER = 0;
};

#endif /* defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS) */
