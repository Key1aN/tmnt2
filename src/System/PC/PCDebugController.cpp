#include "PCDebugController.hpp"


#if defined(TMNT2_DEBUG_TOOLS)

#include <windows.h>

#include <cstdio>
#include <cstring>


namespace
{
    static const int32 AXIS_CAPTURE_THRESHOLD = 8192;
    static const int32 AXIS_DEADZONE = 6553;

    struct CALIBRATIONPROFILE
    {
        bool m_bValid;
        CPCPhysicalController::DEBUGAXIS m_axisX;
        CPCPhysicalController::DEBUGAXIS m_axisY;
        int32 m_iSignX;
        int32 m_iSignY;
        int32 m_iCenterX;
        int32 m_iCenterY;
    };

    static int32 s_iActivePort = -1;
    static CPCPhysicalController::DEBUGGAMEPADSTATE s_state = {};
    static CPCDebugController::CALIBRATIONSTATE s_calibrationState = CPCDebugController::CALIBRATION_IDLE;
    static CALIBRATIONPROFILE s_profile = {};
    static int32 s_aCenter[CPCPhysicalController::DEBUGAXIS_NUM] = {};
    static int32 s_aRight[CPCPhysicalController::DEBUGAXIS_NUM] = {};
    static int32 s_aUp[CPCPhysicalController::DEBUGAXIS_NUM] = {};
    static char s_szLoadedProfile[96] = {};
    static char s_szDeviceLabel[128] = "No gamepad detected";
    static char s_szProfileLabel[128] = "Uncalibrated";


    static int32 Absolute(int32 value)
    {
        return (value < 0 ? -value : value);
    };


    static int32 ClampAxis(int32 value)
    {
        if (value > TYPEDEF::SINT16_MAX)
            return TYPEDEF::SINT16_MAX;
        if (value < TYPEDEF::SINT16_MIN)
            return TYPEDEF::SINT16_MIN;
        if (Absolute(value) <= AXIS_DEADZONE)
            return 0;
        return value;
    };


    static void MakeProfileKey(const CPCPhysicalController::DEBUGGAMEPADSTATE& state, char* pszKey, size_t capacity)
    {
        std::snprintf(
            pszKey,
            capacity,
            "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            state.m_uProductData1,
            state.m_uProductData2,
            state.m_uProductData3,
            state.m_auProductData4[0],
            state.m_auProductData4[1],
            state.m_auProductData4[2],
            state.m_auProductData4[3],
            state.m_auProductData4[4],
            state.m_auProductData4[5],
            state.m_auProductData4[6],
            state.m_auProductData4[7]
        );
    };


    static bool MakeProfilePath(char* pszPath, size_t capacity, bool bCreateDirectory)
    {
        DWORD length = GetModuleFileNameA(nullptr, pszPath, static_cast<DWORD>(capacity));
        if ((length == 0) || (length >= capacity))
            return false;

        char* pszSlash = std::strrchr(pszPath, '\\');
        if (!pszSlash)
            return false;

        *pszSlash = '\0';
        if (strcat_s(pszPath, capacity, "\\TMNT2-DebugTools") != 0)
            return false;

        if (bCreateDirectory)
        {
            if (!CreateDirectoryA(pszPath, nullptr) && (GetLastError() != ERROR_ALREADY_EXISTS))
                return false;
        };

        return (strcat_s(pszPath, capacity, "\\ControllerProfiles.ini") == 0);
    };


    static void UpdateProfileLabel(void)
    {
        if (!s_profile.m_bValid)
        {
            std::strcpy(s_szProfileLabel, "Uncalibrated - retail fallback active");
            return;
        };

        std::snprintf(
            s_szProfileLabel,
            sizeof(s_szProfileLabel),
            "X=%s %c  Y=%s %c",
            CPCDebugController::GetAxisName(s_profile.m_axisX),
            (s_profile.m_iSignX > 0 ? '+' : '-'),
            CPCDebugController::GetAxisName(s_profile.m_axisY),
            (s_profile.m_iSignY > 0 ? '+' : '-')
        );
    };


    static void LoadProfile(const char* pszProfileKey)
    {
        s_profile = {};
        std::strncpy(s_szLoadedProfile, pszProfileKey, sizeof(s_szLoadedProfile) - 1);
        s_szLoadedProfile[sizeof(s_szLoadedProfile) - 1] = '\0';

        char szPath[MAX_PATH];
        if (!MakeProfilePath(szPath, sizeof(szPath), false))
        {
            UpdateProfileLabel();
            return;
        };

        if (!GetPrivateProfileIntA(pszProfileKey, "Valid", 0, szPath))
        {
            UpdateProfileLabel();
            return;
        };

        int32 axisX = GetPrivateProfileIntA(pszProfileKey, "AxisX", -1, szPath);
        int32 axisY = GetPrivateProfileIntA(pszProfileKey, "AxisY", -1, szPath);
        if ((axisX < 0) || (axisX >= CPCPhysicalController::DEBUGAXIS_NUM) ||
            (axisY < 0) || (axisY >= CPCPhysicalController::DEBUGAXIS_NUM) ||
            (axisX == axisY))
        {
            UpdateProfileLabel();
            return;
        };

        s_profile.m_axisX = static_cast<CPCPhysicalController::DEBUGAXIS>(axisX);
        s_profile.m_axisY = static_cast<CPCPhysicalController::DEBUGAXIS>(axisY);
        s_profile.m_iSignX = (GetPrivateProfileIntA(pszProfileKey, "SignX", 1, szPath) >= 0 ? 1 : -1);
        s_profile.m_iSignY = (GetPrivateProfileIntA(pszProfileKey, "SignY", 1, szPath) >= 0 ? 1 : -1);
        s_profile.m_iCenterX = GetPrivateProfileIntA(pszProfileKey, "CenterX", 0, szPath);
        s_profile.m_iCenterY = GetPrivateProfileIntA(pszProfileKey, "CenterY", 0, szPath);
        s_profile.m_bValid = true;
        UpdateProfileLabel();
    };


    static void WriteProfileValue(const char* pszPath, const char* pszName, int32 value)
    {
        char szValue[32];
        std::snprintf(szValue, sizeof(szValue), "%d", value);
        WritePrivateProfileStringA(s_szLoadedProfile, pszName, szValue, pszPath);
    };


    static void SaveProfile(void)
    {
        char szPath[MAX_PATH];
        if (!s_profile.m_bValid || !MakeProfilePath(szPath, sizeof(szPath), true))
            return;

        WriteProfileValue(szPath, "Valid", 1);
        WriteProfileValue(szPath, "AxisX", static_cast<int32>(s_profile.m_axisX));
        WriteProfileValue(szPath, "AxisY", static_cast<int32>(s_profile.m_axisY));
        WriteProfileValue(szPath, "SignX", s_profile.m_iSignX);
        WriteProfileValue(szPath, "SignY", s_profile.m_iSignY);
        WriteProfileValue(szPath, "CenterX", s_profile.m_iCenterX);
        WriteProfileValue(szPath, "CenterY", s_profile.m_iCenterY);
    };


    static int32 FindDominantAxis(const int32* pSample, const int32* pCenter, int32 iExcludedAxis)
    {
        int32 iBestAxis = -1;
        int32 iBestDelta = 0;
        for (int32 i = 0; i < CPCPhysicalController::DEBUGAXIS_NUM; ++i)
        {
            if (i == iExcludedAxis)
                continue;

            int32 delta = Absolute(pSample[i] - pCenter[i]);
            if (delta > iBestDelta)
            {
                iBestDelta = delta;
                iBestAxis = i;
            };
        };

        return (iBestDelta >= AXIS_CAPTURE_THRESHOLD ? iBestAxis : -1);
    };


    static void CopyRawAxes(int32* pDestination)
    {
        std::memcpy(pDestination, s_state.m_aAxis, sizeof(s_state.m_aAxis));
    };


    static void SelectActiveGamepad(void)
    {
        CPCPhysicalController::DEBUGGAMEPADSTATE state = {};
        if ((s_iActivePort >= 0) && CPCPhysicalController::GetDebugGamepadState(s_iActivePort, &state))
        {
            s_state = state;
            return;
        };

        s_iActivePort = -1;
        for (int32 i = 0; i < CPCPhysicalController::GetDebugGamepadCapacity(); ++i)
        {
            if (CPCPhysicalController::GetDebugGamepadState(i, &state))
            {
                s_iActivePort = i;
                s_state = state;
                break;
            };
        };
    };


    static void EnsureProfileLoaded(void)
    {
        if (s_iActivePort < 0)
        {
            std::strcpy(s_szDeviceLabel, "No gamepad detected");
            return;
        };

        char szProfileKey[96];
        MakeProfileKey(s_state, szProfileKey, sizeof(szProfileKey));
        std::snprintf(
            s_szDeviceLabel,
            sizeof(s_szDeviceLabel),
            "Port %d  Product %08X",
            s_iActivePort,
            s_state.m_uProductData1
        );

        if (std::strcmp(szProfileKey, s_szLoadedProfile))
        {
            s_calibrationState = CPCDebugController::CALIBRATION_IDLE;
            LoadProfile(szProfileKey);
        };
    };
};


/*static*/ void CPCDebugController::Update(void)
{
    SelectActiveGamepad();
    EnsureProfileLoaded();
};


/*static*/ bool CPCDebugController::GetRightStick(int16* pX, int16* pY)
{
    ASSERT(pX);
    ASSERT(pY);

    Update();
    if ((s_iActivePort < 0) || !s_profile.m_bValid)
        return false;

    int32 x = (s_state.m_aAxis[s_profile.m_axisX] - s_profile.m_iCenterX) * s_profile.m_iSignX;
    int32 y = (s_state.m_aAxis[s_profile.m_axisY] - s_profile.m_iCenterY) * s_profile.m_iSignY;
    *pX = static_cast<int16>(ClampAxis(x));
    *pY = static_cast<int16>(ClampAxis(y));
    return true;
};


/*static*/ void CPCDebugController::AdvanceCalibration(void)
{
    Update();
    if (s_iActivePort < 0)
    {
        s_calibrationState = CALIBRATION_ERROR;
        return;
    };

    switch (s_calibrationState)
    {
    case CALIBRATION_IDLE:
    case CALIBRATION_COMPLETE:
    case CALIBRATION_ERROR:
        s_calibrationState = CALIBRATION_CENTER;
        break;

    case CALIBRATION_CENTER:
        CopyRawAxes(s_aCenter);
        s_calibrationState = CALIBRATION_RIGHT;
        break;

    case CALIBRATION_RIGHT:
        CopyRawAxes(s_aRight);
        s_calibrationState = CALIBRATION_UP;
        break;

    case CALIBRATION_UP:
        {
            CopyRawAxes(s_aUp);
            int32 axisX = FindDominantAxis(s_aRight, s_aCenter, -1);
            int32 axisY = FindDominantAxis(s_aUp, s_aCenter, axisX);
            if ((axisX < 0) || (axisY < 0))
            {
                s_calibrationState = CALIBRATION_ERROR;
                break;
            };

            s_profile.m_axisX = static_cast<CPCPhysicalController::DEBUGAXIS>(axisX);
            s_profile.m_axisY = static_cast<CPCPhysicalController::DEBUGAXIS>(axisY);
            s_profile.m_iSignX = ((s_aRight[axisX] - s_aCenter[axisX]) >= 0 ? 1 : -1);
            s_profile.m_iSignY = ((s_aUp[axisY] - s_aCenter[axisY]) >= 0 ? 1 : -1);
            s_profile.m_iCenterX = s_aCenter[axisX];
            s_profile.m_iCenterY = s_aCenter[axisY];
            s_profile.m_bValid = true;
            SaveProfile();
            UpdateProfileLabel();
            s_calibrationState = CALIBRATION_COMPLETE;
        }
        break;

    default:
        break;
    };
};


/*static*/ void CPCDebugController::CancelCalibration(void)
{
    if (IsCalibrationActive())
        s_calibrationState = CALIBRATION_IDLE;
};


/*static*/ void CPCDebugController::ResetCalibration(void)
{
    Update();
    if (s_szLoadedProfile[0] != '\0')
    {
        char szPath[MAX_PATH];
        if (MakeProfilePath(szPath, sizeof(szPath), false))
            WritePrivateProfileStringA(s_szLoadedProfile, nullptr, nullptr, szPath);
    };

    s_profile = {};
    s_calibrationState = CALIBRATION_IDLE;
    UpdateProfileLabel();
};


/*static*/ bool CPCDebugController::IsCalibrationActive(void)
{
    return (s_calibrationState == CALIBRATION_CENTER) ||
           (s_calibrationState == CALIBRATION_RIGHT) ||
           (s_calibrationState == CALIBRATION_UP);
};


/*static*/ bool CPCDebugController::IsProfileValid(void)
{
    Update();
    return s_profile.m_bValid;
};


/*static*/ CPCDebugController::CALIBRATIONSTATE CPCDebugController::GetCalibrationState(void)
{
    return s_calibrationState;
};


/*static*/ const char* CPCDebugController::GetCalibrationStatus(void)
{
    switch (s_calibrationState)
    {
    case CALIBRATION_CENTER:   return "Step 1/3: release both sticks, then press A / Enter";
    case CALIBRATION_RIGHT:    return "Step 2/3: hold right stick fully RIGHT, then press A / Enter";
    case CALIBRATION_UP:       return "Step 3/3: hold right stick fully UP, then press A / Enter";
    case CALIBRATION_COMPLETE: return "Calibration saved for this controller";
    case CALIBRATION_ERROR:    return "Calibration failed: axis movement was too small; press A to retry";
    default:                   return (s_profile.m_bValid ? "Controller profile ready" : "Controller is not calibrated");
    };
};


/*static*/ const char* CPCDebugController::GetCalibrationPrompt(void)
{
    if (IsCalibrationActive())
        return "A / Enter capture step  |  B / Esc cancel";
    return "A / Enter start calibration  |  X / Backspace reset profile";
};


/*static*/ const char* CPCDebugController::GetDeviceLabel(void)
{
    Update();
    return s_szDeviceLabel;
};


/*static*/ const char* CPCDebugController::GetProfileLabel(void)
{
    Update();
    return s_szProfileLabel;
};


/*static*/ int32 CPCDebugController::GetRawAxis(CPCPhysicalController::DEBUGAXIS axis)
{
    Update();
    if ((s_iActivePort < 0) || (axis < 0) || (axis >= CPCPhysicalController::DEBUGAXIS_NUM))
        return 0;
    return s_state.m_aAxis[axis];
};


/*static*/ const char* CPCDebugController::GetAxisName(CPCPhysicalController::DEBUGAXIS axis)
{
    static const char* s_apszAxisName[CPCPhysicalController::DEBUGAXIS_NUM] =
    {
        "X", "Y", "Z", "Rx", "Ry", "Rz", "S0", "S1",
    };

    if ((axis < 0) || (axis >= CPCPhysicalController::DEBUGAXIS_NUM))
        return "?";
    return s_apszAxisName[axis];
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
