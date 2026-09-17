#pragma once


#if defined(TMNT2_DEBUG_TOOLS)

#include "PCPhysicalController.hpp"


class CPCDebugController
{
public:
    enum CALIBRATIONSTATE
    {
        CALIBRATION_IDLE = 0,
        CALIBRATION_CENTER,
        CALIBRATION_RIGHT,
        CALIBRATION_UP,
        CALIBRATION_COMPLETE,
        CALIBRATION_ERROR,
    };

public:
    static void Update(void);
    static bool GetRightStick(int16* pX, int16* pY);
    static void AdvanceCalibration(void);
    static void CancelCalibration(void);
    static void ResetCalibration(void);
    static bool IsCalibrationActive(void);
    static bool IsProfileValid(void);
    static CALIBRATIONSTATE GetCalibrationState(void);
    static const char* GetCalibrationStatus(void);
    static const char* GetCalibrationPrompt(void);
    static const char* GetDeviceLabel(void);
    static const char* GetProfileLabel(void);
    static int32 GetRawAxis(CPCPhysicalController::DEBUGAXIS axis);
    static const char* GetAxisName(CPCPhysicalController::DEBUGAXIS axis);
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
