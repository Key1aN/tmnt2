#pragma once

#include "System/Common/Controller.hpp"


class CPCPhysicalController
{
public:
    static int32 PHYSICALCONTROLLER_MAX;

#if defined(TMNT2_DEBUG_TOOLS)
    enum DEBUGAXIS
    {
        DEBUGAXIS_X = 0,
        DEBUGAXIS_Y,
        DEBUGAXIS_Z,
        DEBUGAXIS_RX,
        DEBUGAXIS_RY,
        DEBUGAXIS_RZ,
        DEBUGAXIS_SLIDER0,
        DEBUGAXIS_SLIDER1,

        DEBUGAXIS_NUM,
    };

    struct DEBUGGAMEPADSTATE
    {
        bool   m_bConnected;
        int32  m_iPhysicalPort;
        int32  m_aAxis[DEBUGAXIS_NUM];
        uint32 m_uProductData1;
        uint16 m_uProductData2;
        uint16 m_uProductData3;
        uint8  m_auProductData4[8];
    };
#endif /* defined(TMNT2_DEBUG_TOOLS) */

public:
    static bool Initialize(void);
    static void Terminate(void);
    static IPhysicalController* Open(int32 iController);
    static void MapDigital(uint32 btn, int32 iDIKey);
    static void MapDigitalFixed(uint32 btn, int32 iDIKey);
    static void MapAnalog(CController::ANALOG analog, int32 iDIKeyX, int32 iDIKeyY);
    static bool IsKeyDown(int32 iDIKey);
    static bool IsKeyTrigger(int32 iDIKey);
    static bool IsKeyNotFixed(int32 iDIKey);
    static int32 GetDownKey(void);
    static int32 GetPort(void);
#if defined(TMNT2_DEBUG_TOOLS)
    static bool GetDebugGamepadState(int32 iPhysicalPort, DEBUGGAMEPADSTATE* pState);
    static int32 GetDebugGamepadCapacity(void);
#endif /* defined(TMNT2_DEBUG_TOOLS) */
};
