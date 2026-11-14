#include "MapCamera.hpp"

#if defined(TARGET_PC)
#include "System/PC/PCModFeatures.hpp"
#include "System/PC/PCPhysicalController.hpp"
#endif /* defined(TARGET_PC) */
#include "WorldMap.hpp"
#include "CameraDataMan.hpp"

#include "Game/Component/Effect/WaterSplash.hpp"
#include "Game/Component/Effect/Snow.hpp"
#include "Game/Component/GameData/GameData.hpp"
#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/System/Misc/Gamepad.hpp"
#include "System/Common/Camera.hpp"
#include "System/Common/Configure.hpp"

#if defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS)
#include "Game/Component/GameMain/GameStageDebug.hpp"
#endif /* defined(_DEBUG) || defined(TMNT2_DEBUG_TOOLS) */


/*
 *  Original camera uses fixed per-update lerp factors that were authored for 60 updates/sec.
 *  Update rate of the game follows display refresh rate (see CScreen), so with enhanced camera
 *  the factor is converted to its time based equivalent - at 60hz result is identical to original.
 */
static inline float LerpFactor(float t)
{
    if (!CMapCamera::IsEnhanced())
        return t;

    const float fAuthoredRate = 60.0f;

    float dt = Clamp(CGameProperty::GetElapsedTime(), 0.0f, 0.1f);

    t = (1.0f - std::pow((1.0f - t), (dt * fAuthoredRate)));
    t = Clamp(t, 0.0f, 1.0f);

    return t;
};


#if defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS)
static int16 GetDebugControllerAnalog(CController::ANALOG analog)
{
    int16 locked = CController::GetAnalog(CController::CONTROLLER_LOCKED_ON_VIRTUAL, analog);
    int16 unlocked = CController::GetAnalog(CController::CONTROLLER_UNLOCKED_ON_VIRTUAL, analog);
    int32 lockedMagnitude = (locked < 0 ? -static_cast<int32>(locked) : static_cast<int32>(locked));
    int32 unlockedMagnitude = (unlocked < 0 ? -static_cast<int32>(unlocked) : static_cast<int32>(unlocked));
    return (lockedMagnitude >= unlockedMagnitude ? locked : unlocked);
};


static float NormalizeDebugControllerAnalog(int16 value)
{
    return (value >= 0 ?
        static_cast<float>(value) / static_cast<float>(TYPEDEF::SINT16_MAX) :
        -static_cast<float>(value) / static_cast<float>(TYPEDEF::SINT16_MIN));
};
#endif /* defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS) */


static inline float ExpFactor(float fSpeed)
{
    float dt = Clamp(CGameProperty::GetElapsedTime(), 0.0f, 0.1f);

    return Clamp((1.0f - std::exp(-fSpeed * dt)), 0.0f, 1.0f);
};


struct CMapCamera::FRAMEBASIS
{
    RwV3d m_vAt;
    RwV3d m_vRight;
    RwV3d m_vUp;
    RwV3d m_vFront;
    float m_fDist;  /* authored (not zoomed) distance from eye to lookat */
    float m_fViewX; /* view window half size at unit distance            */
    float m_fViewY;
};


class CMapCamera::CIntroduction
{
private:
    static const int32 DEMONUM = 3;
	static const float DEMODURATION;

    struct DEMOCAMERAINFO
    {
        float m_fPathTime;
        float m_fTimer;
        int32 m_nPathEyeID;
        int32 m_nPathLookatID;
    };
    
public:
    CIntroduction(void);
    inline ~CIntroduction(void) {};
    void Skip(void);
    bool Update(RwV3d* pvEye, RwV3d* pvAt);

private:
    int32 m_nNumDemo;
    DEMOCAMERAINFO m_aDemoCameraInfo[DEMONUM];
    int32 m_nDemoID;
    bool m_bEnd;
};


class CMapCamera::CVibration
{
public:
    CVibration(float fPower, float fTime, int32 nFreq);
    inline ~CVibration(void) {};
    void Period(void);
    void Update(RwV3d* pvEye, RwV3d* pvAt);
    
    inline bool IsVibrating(void) const { return (m_fTimer > 0.0f); };

private:
    float m_fVibTime;
    float m_fVibPower;
    float m_fTimer;
    float m_fTheta;
    int32 m_nFreq;
    RwV3d m_vVibration;
};


//
// *********************************************************************************
//


/*static*/ const float CMapCamera::CIntroduction::DEMODURATION = 2.0f;


CMapCamera::CIntroduction::CIntroduction(void)
: m_nNumDemo(0)
, m_nDemoID(0)
, m_bEnd(false)
{
    for (int32 i = 0; i < COUNT_OF(m_aDemoCameraInfo); ++i)
    {
        char szPathName[16];

        szPathName[0] = '\0';
        std::sprintf(szPathName, "d%d", i);

        int32 nPathEyeID = CCameraDataManager::GetPathIDFromName(szPathName);
        if (nPathEyeID < 0)
            break;

        szPathName[0] = '\0';
        std::sprintf(szPathName, "d%d_t", i);

        int32 nPathLookatID = CCameraDataManager::GetPathIDFromName(szPathName);
        if (nPathLookatID < 0)
            break;

        m_aDemoCameraInfo[i].m_nPathEyeID = nPathEyeID;
        m_aDemoCameraInfo[i].m_nPathLookatID = nPathLookatID;
        m_aDemoCameraInfo[i].m_fTimer = 0.0f;
        m_aDemoCameraInfo[i].m_fPathTime = 0.0f;

        ++m_nNumDemo;
    };

    if (!m_nNumDemo)
        m_bEnd = true;
};


void CMapCamera::CIntroduction::Skip(void)
{
    if (m_nNumDemo)
    {
        const float fDemoDurationTotal = (DEMONUM * DEMODURATION);
        
        m_nDemoID = m_nNumDemo - 1;
        m_aDemoCameraInfo[m_nNumDemo - 1].m_fPathTime = 1.0f;
        m_aDemoCameraInfo[m_nNumDemo - 1].m_fTimer = fDemoDurationTotal / m_nNumDemo;
    };
};


bool CMapCamera::CIntroduction::Update(RwV3d* pvEye, RwV3d* pvAt)
{
    if (m_bEnd)
        return false;

    ASSERT(m_nDemoID < m_nNumDemo);
    
    DEMOCAMERAINFO* pCameraInfo = &m_aDemoCameraInfo[m_nDemoID];

    float fDemoDurationTotal = (DEMONUM * DEMODURATION);
    float fSplineT = pCameraInfo->m_fTimer / (fDemoDurationTotal / float(m_nNumDemo));

    CCameraDataManager::GetSplinePos(pvEye, pCameraInfo->m_nPathEyeID, fSplineT);
    CCameraDataManager::GetSplinePos(pvAt, pCameraInfo->m_nPathLookatID, fSplineT);

    pCameraInfo->m_fTimer += CGameProperty::GetElapsedTime();

    if (pCameraInfo->m_fTimer > (fDemoDurationTotal / float(m_nNumDemo)))
		m_bEnd = (++m_nDemoID > (m_nNumDemo - 1));   

    return true;
};


//
// *********************************************************************************
//


CMapCamera::CVibration::CVibration(float fPower, float fTime, int32 nFreq)
: m_fVibTime(fTime)
, m_fVibPower(fPower)
, m_fTimer(fTime)
, m_fTheta(0.0f)
, m_nFreq(nFreq)
, m_vVibration(Math::VECTOR3_ZERO)
{
    ;
};


void CMapCamera::CVibration::Period(void)
{
    if (IsVibrating())
    {
        float fDltRadian = Math::Sin(m_fTheta);

        m_vVibration = Math::VECTOR3_ZERO;
        m_vVibration.y = (fDltRadian * m_fVibPower);

        m_fTheta += (CGameProperty::GetElapsedTime() * float(m_nFreq) * MATH_PI2);
        m_fTimer -= CGameProperty::GetElapsedTime();
    };
};


void CMapCamera::CVibration::Update(RwV3d* pvEye, RwV3d* pvAt)
{
    ASSERT(pvEye);
    ASSERT(pvAt);
    
    if (IsVibrating())
    {
        Math::Vec3_Add(pvEye, pvEye, &m_vVibration);
        Math::Vec3_Add(pvAt, pvAt, &m_vVibration);
    };
};


//
// *********************************************************************************
//


/*static*/ float CMapCamera::m_fDefaultViewSize = 0.5f;
/*static*/ float CMapCamera::m_fDefaultClipFar = 800.0f;
/*static*/ float CMapCamera::m_fDefaultClipNear = 0.1f;
/*static*/ float CMapCamera::m_fDefaultFogDist = 5.0f;
/*static*/ RwV3d CMapCamera::m_vDefaultLookat = { 0.0f, 0.0f, 0.0f };
/*static*/ RwV3d CMapCamera::m_vDefaultEye = { 0.0f, 5.0f, -8.0f };

static const CMapCamera::TUNING s_tuningDefault =
{
    true,   /* m_bEnable            */
    0.80f,  /* m_fSafeMargin        */
    0.66f,  /* m_fReturnMargin      */
    0.92f,  /* m_fSecondaryMargin   */
    1.6f,   /* m_fZoomMax           */
    1.25f,  /* m_fFovZoomMax        */
    6.0f,   /* m_fZoomOutSpeed      */
    1.5f,   /* m_fZoomInSpeed       */
    0.5f,   /* m_fZoomInDelay       */
    1.0f,   /* m_fTargetHalfHeight  */
    1.2f,   /* m_fJumpWindow        */
    10.0f,  /* m_fGroundFollowSpeed */
    0.8f,   /* m_fLookAheadDist     */
    0.25f,  /* m_fLookAheadTime     */
    2.5f,   /* m_fLookAheadSpeed    */
    0.45f,  /* m_fBossWeightMax     */
    13.0f,  /* m_fBossRange         */
};


/*static*/ CMapCamera::TUNING CMapCamera::m_tuning = s_tuningDefault;


/*static*/ bool CMapCamera::IsEnhanced(void)
{
#if defined(TARGET_PC)
    /* with Debug Tools mod switched off in the manager this EXE runs retail camera */
    if (!CPCModFeatures::IsDebugToolsEnabled())
        return false;
#endif /* defined(TARGET_PC) */

    return m_tuning.m_bEnable;
};


/*static*/ const CMapCamera::TUNING& CMapCamera::DefaultTuning(void)
{
    return s_tuningDefault;
};


/*static*/ void CMapCamera::ResetTuning(void)
{
    /* master switch is not a tuning value and is kept as is */
    bool bEnable = m_tuning.m_bEnable;
    m_tuning = s_tuningDefault;
    m_tuning.m_bEnable = bEnable;
};


CMapCamera::CMapCamera(void)
: m_pCamera(nullptr)
, m_vEye(Math::VECTOR3_ZERO)
, m_vAt(Math::VECTOR3_ZERO)
, m_fDist(0.0f)
, m_fViewSize(m_fDefaultViewSize)
, m_fRotY(0.0f)
, m_fHeight(0.0f)
, m_fLookatOffsetY(0.0f)
, m_fPathTime(0.0f)
, m_fPrePathTime(0.0f)
, m_fLookatViewAreaRadius(0.0f)
, m_fChangeTimer(0.0f)
, m_fRequestZoom(0.0f)
, m_mode(MODE_AUTOCHANGE)
, m_pathmode(PATHMODE_SINGLEPLAYER)
, m_bChangeMoment(false)
, m_pIntroduction(nullptr)
, m_pVibration(nullptr)
, m_nNumFrameTarget(0)
, m_bFramingRequest(false)
, m_fFrameZoom(1.0f)
, m_fFrameFovZoom(1.0f)
, m_fFrameZoomHold(0.0f)
, m_fFrameReach(0.0f)
{
    std::memset(m_aFrameTarget, 0, sizeof(m_aFrameTarget));
    std::memset(&m_frameDiag, 0, sizeof(m_frameDiag));
    m_frameDiag.m_fZoom = 1.0f;
    m_frameDiag.m_fZoomRequest = 1.0f;
    m_frameDiag.m_fFovZoom = 1.0f;

    static bool s_bTuningInit = false;
    if (!s_bTuningInit)
    {
        s_bTuningInit = true;

        /* run arg "-origcam" forces original camera behavior (A/B testing in release builds) */
        if (CConfigure::CheckArg("origcam"))
            m_tuning.m_bEnable = false;
    };

    m_pCamera = CCamera::GetCamera();
    ASSERT(m_pCamera);

    const CWorldMap::MAPINFO* pMapInfo = CWorldMap::GetMapInfo();
    
    SetFarClipPlane(pMapInfo ? pMapInfo->m_fFarClipDist : m_fDefaultClipFar);
    SetNearClipPlane(pMapInfo ? pMapInfo->m_fNearClipDist : m_fDefaultClipNear);
    SetFogDistance(pMapInfo ? pMapInfo->m_foginfo.m_fDist : m_fDefaultFogDist);

    SetViewWindow(m_fDefaultViewSize);
    SetLookat(&m_vDefaultEye, &m_vDefaultLookat);
    m_fRequestZoom = 1.0f;
    m_szCameraAreaName[0] = '\0';

    IntroductionCreate();
};


CMapCamera::~CMapCamera(void)
{
    if (m_pIntroduction)
    {
        delete m_pIntroduction;
        m_pIntroduction = nullptr;
    };

    if (m_pVibration)
    {
        delete m_pVibration;
        m_pVibration = nullptr;
    };

    ASSERT(m_pCamera);
    m_pCamera->Release();
    m_pCamera = nullptr;
};


bool CMapCamera::BeginScene(void)
{
#ifdef _DEBUG
    /* TODO renderware assertion failed at AREA32 - matrix is not orhonormal */
    CDebug::SupressRwDebugAssert(true);
#endif /* _DEBUG */    

    bool bResult = m_pCamera->BeginScene();

#ifdef _DEBUG
    CDebug::SupressRwDebugAssert(false);
#endif /* _DEBUG */    

    return bResult;
};


void CMapCamera::EndScene(void)
{
    m_pCamera->EndScene();
};


void CMapCamera::WorldAddCamera(RpWorld* pWorld)
{
    ASSERT(pWorld);
    ASSERT(m_pCamera);

    if (pWorld && m_pCamera)
        RpWorldAddCamera(pWorld, m_pCamera->GetRwCamera());
};


void CMapCamera::WorldRemoveCamera(RpWorld* pWorld)
{
    ASSERT(pWorld);
    ASSERT(m_pCamera);

    if (pWorld && m_pCamera)
        RpWorldRemoveCamera(pWorld, m_pCamera->GetRwCamera());
};


void CMapCamera::Update(const RwV3d* pvAt)
{
    Update(pvAt, 1.0f);
};


void CMapCamera::Update(const RwV3d* pvAt, float fZoom)
{
    VibrationUpdate();

    if (!CGameData::Attribute().IsPlayDemoMode())
    {
        uint32 digitalTrigger = 0;
        digitalTrigger |= IGamepad::GetDigitalTrigger(IGamepad::CONTROLLER_UNLOCKED_ON_VIRTUAL);
        digitalTrigger |= IGamepad::GetDigitalTrigger(IGamepad::CONTROLLER_LOCKED_ON_VIRTUAL);

        bool bSuppressSwitchCamera = false;
#if defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS)
        if (CPCModFeatures::IsDebugToolsEnabled() &&
            CGameStageDebug::CAMERA_SUPPRESS_SWITCH_TRIGGER)
        {
            CGameStageDebug::CAMERA_SUPPRESS_SWITCH_TRIGGER = false;
            bSuppressSwitchCamera = true;
        };

        // Camera locked (the default): R3 is the lock-on button and never
        // switches the retail camera mode.
        if (CPCModFeatures::IsDebugToolsEnabled() &&
            !CGameStageDebug::CAMERA_MANUAL_ALLOWED)
        {
            bSuppressSwitchCamera = true;
        };
#endif /* defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS) */

        if (!bSuppressSwitchCamera &&
            IGamepad::CheckFunction(digitalTrigger, IGamepad::FUNCTION_SWITCH_CAM))
            m_pathmode = static_cast<PATHMODE>((m_pathmode + 1) % PATHMODEMAX);
    };

    m_fRequestZoom = fZoom;
    m_fFrameReach = 0.0f;
    m_frameDiag.m_bFraming = false;

    switch (m_mode)
    {
    case MODE_MANUAL:
        UpdateManualCamera(pvAt);
        break;

    case MODE_AUTOCHANGE:
        UpdateAutoChangeCamera(pvAt);
        break;
        
    case MODE_INTRODUCTION:
        IntroductionUpdate();
        break;

    default:
        ASSERT(false);
        break;
    };

    PostUpdate();

    const CWorldMap::MAPINFO* pMapInfo = CWorldMap::GetMapInfo();
    if (pMapInfo)
    {
        switch (pMapInfo->m_weather)
        {
        case CWorldMap::WEATHER_RAIN:
            CWaterSplashManager::SetBasisPosition(&m_vAt);
            break;
            
        case CWorldMap::WEATHER_SNOW:
            CSnowManager::SetCameraPosition(this);        
            break;

        default:
            break;
        };
    };
};


void CMapCamera::PostUpdate(void)
{
    RwV3d vecDist = Math::VECTOR3_ZERO;
    Math::Vec3_Sub(&vecDist, &m_vAt, &m_vEye);

    float fDist = Math::Vec3_Length(&vecDist);
    fDist = std::fabs(fDist);

    m_fLookatViewAreaRadius = fDist * m_fViewSize;

    /*
     *  Original move boundary grows together with zoom, so zoom and boundary chase each other.
     *  With framing active boundary is the area that camera is able to frame at max zoom instead.
     */
    if (m_frameDiag.m_bFraming && (m_fFrameReach > 0.0f))
        m_fLookatViewAreaRadius = m_fFrameReach;

    m_frameDiag.m_fMoveRadius = m_fLookatViewAreaRadius;

    if (!std::strncmp(m_szCameraAreaName, "f", 1) ||
        !std::strncmp(m_szCameraAreaName, "f", 1))
        m_fLookatViewAreaRadius = 1000.0f;

    m_bChangeMoment = false;
};


void CMapCamera::UpdateFraming(const RwV3d* pvAt, const FRAMETARGET* aTarget, int32 nNumTarget, float fZoomScale)
{
    m_nNumFrameTarget = 0;

    if (aTarget)
    {
        for (int32 i = 0; (i < nNumTarget) && (m_nNumFrameTarget < FRAMETARGET_MAX); ++i)
            m_aFrameTarget[m_nNumFrameTarget++] = aTarget[i];
    };

    m_bFramingRequest = (IsEnhanced() && (m_nNumFrameTarget > 0));

    Update(pvAt, fZoomScale);

    m_bFramingRequest = false;
};


void CMapCamera::UpdateMoment(const RwV3d* pvAt)
{
    m_bChangeMoment = true;
    Update(pvAt, 1.0f);
};


void CMapCamera::UpdateAutoChangeCamera(const RwV3d* pvAt)
{
    RwV3d vCheckPoint = *pvAt;

    char szName[32];
    szName[0] = '\0';

    if (CWorldMap::GetCameraAreaName(&vCheckPoint, szName))
    {
        if (m_bChangeMoment || (m_fChangeTimer <= 0.0f && std::strcmp(m_szCameraAreaName, szName)))
        {
			std::strcpy(m_szCameraAreaName, szName);
			ResetCameraForCameraAreaChange(pvAt);
        };

        if (!std::strncmp(m_szCameraAreaName, "p", 1) ||
            !std::strncmp(m_szCameraAreaName, "P", 1))
        {
            UpdatePathCamera(pvAt, false);
        }
        else if (!std::strncmp(m_szCameraAreaName, "e", 1) ||
                 !std::strncmp(m_szCameraAreaName, "E", 1))
        {
            UpdatePathCamera(pvAt, true);
        }
        else if (!std::strncmp(m_szCameraAreaName, "t", 1) ||
                 !std::strncmp(m_szCameraAreaName, "T", 1))
        {
            UpdateSetCamera(pvAt);
        }
        else if (!std::strncmp(m_szCameraAreaName, "f", 1) ||
                 !std::strncmp(m_szCameraAreaName, "F", 1))
        {
            UpdateFixedCamera(pvAt);
        };
    };
};


void CMapCamera::ResetCameraForCameraAreaChange(const RwV3d* pvAt)
{
    if (!std::strncmp(m_szCameraAreaName, "p", 1) ||
        !std::strncmp(m_szCameraAreaName, "P", 1) ||
        !std::strncmp(m_szCameraAreaName, "e", 1) ||
        !std::strncmp(m_szCameraAreaName, "E", 1))
    {
        char szAtCameraName[32];
        szAtCameraName[0] = '\0';

        MakePathAtCameraName(szAtCameraName, m_szCameraAreaName);
        
        int32 nPathID = CCameraDataManager::GetPathIDFromName(szAtCameraName);
        if (nPathID >= 0)
        {
            RwV3d vec = *pvAt;
            float fValue = CCameraDataManager::FindNearestPosValue(&vec, nPathID, 100);
            
            m_fPathTime = fValue;
            m_fPrePathTime = fValue;
            m_fChangeTimer = 1.5f;
        };
    };
};


void CMapCamera::MakePathAtCameraName(char* pszPathAtName, const char* pszCameraAreaName)
{
    std::strcpy(pszPathAtName, pszCameraAreaName);
    std::strcat(pszPathAtName, "_t");
};


void CMapCamera::MakePathEyeCameraName(char* pszPathEyeName, const char* pszCameraAreaName)
{
    std::strcpy(pszPathEyeName, pszCameraAreaName);

    switch (m_pathmode)
    {
    case PATHMODE_SINGLEPLAYER:
        {
            std::strcat(pszPathEyeName, "_s");
            
            int32 nPathID = CCameraDataManager::GetPathIDFromName(pszPathEyeName);
            if (nPathID < 0)
            {
                std::strcpy(pszPathEyeName, pszCameraAreaName);
                std::strcat(pszPathEyeName, "_m");
            };
        }
        break;

    case PATHMODE_MULTIPLAYER:
        {
            std::strcat(pszPathEyeName, "_m");
        }
        break;

    default:
        ASSERT(false);
        break;
    };
};


void CMapCamera::MakeSetCameraName(char* pszSetCameraName, const char* pszCameraAreaName)
{
    bool bFound = true;

    std::strcpy(pszSetCameraName, pszCameraAreaName);

    switch (m_pathmode)
    {
    case PATHMODE_SINGLEPLAYER:
        {
            std::strcat(pszSetCameraName, "_s");

            int32 nPathID = CCameraDataManager::GetSetCamIDFromName(pszSetCameraName);
            if (nPathID < 0)
                bFound = false;
        }
        break;

    case PATHMODE_MULTIPLAYER:
        {
            std::strcat(pszSetCameraName, "_m");
            
            int32 nPathID = CCameraDataManager::GetSetCamIDFromName(pszSetCameraName);
            if (nPathID < 0)
                bFound = false;
        }
        break;

    default:
        ASSERT(false);
        break;
    };

    if (!bFound)
    {
        std::strcpy(pszSetCameraName, pszCameraAreaName);
    };
};


void CMapCamera::UpdateManualCamera(const RwV3d* pvAt)
{
    const float DEADZONE = 0.3f;
    const int32 controller = IGamepad::CONTROLLER_UNLOCKED_ON_VIRTUAL;

    float rx = static_cast<float>(IGamepad::GetAnalog(controller, IGamepad::ANALOG_RSTICK_X));
    float ry = static_cast<float>(IGamepad::GetAnalog(controller, IGamepad::ANALOG_RSTICK_Y));

#if defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS)
    if (CPCModFeatures::IsDebugToolsEnabled())
    {
        // Verified physical mapping for the user's PC controller: the retail
        // right-stick X/Y identities are crossed. Keep the correction local to
        // Debug Camera so normal gameplay controls remain unchanged.
        rx =  static_cast<float>(CPCPhysicalController::GetDebugRightStickHorizontal());
        ry = -static_cast<float>(GetDebugControllerAnalog(CController::ANALOG_RSTICK_X));
    };
#endif /* defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS) */

    rx = (rx >= 0.0f ? (rx / float(TYPEDEF::SINT16_MAX)) : -(rx / float(TYPEDEF::SINT16_MIN)));
    ry = (ry >= 0.0f ? (ry / float(TYPEDEF::SINT16_MAX)) : -(ry / float(TYPEDEF::SINT16_MIN)));

    if ((rx > DEADZONE) || (rx < -DEADZONE))
    {
#if defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS)
        if (CPCModFeatures::IsDebugToolsEnabled())
        {
            // Positive physical stick X means orbit the eye to screen-right.
            m_fRotY -= rx * (CGameProperty::GetElapsedTime() * 6.0f);
        }
        else
#endif /* defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS) */
        {
            m_fRotY += rx * (CGameProperty::GetElapsedTime() * 6.0f);
        };
    };

    if ((ry > DEADZONE) || (ry < -DEADZONE))
        m_fHeight += ry * (CGameProperty::GetElapsedTime() * 10.0f);

#if defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS)
    if (CPCModFeatures::IsDebugToolsEnabled())
        m_fHeight = Clamp(m_fHeight, 0.5f, 50.0f);
#endif /* defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS) */

#if defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS)
    if (CPCModFeatures::IsDebugToolsEnabled())
    {
        // Since Debug Tools v1.2.17 the DirectInput table maps buttons by
        // physical position, so the triggers are L2 (LT) and R2 (RT).
        float zoomOut = NormalizeDebugControllerAnalog(
            GetDebugControllerAnalog(CController::ANALOG_L2)
        );
        float zoomIn = NormalizeDebugControllerAnalog(
            GetDebugControllerAnalog(CController::ANALOG_R2)
        );
        float radiusDelta = (zoomOut - zoomIn) * (CGameProperty::GetElapsedTime() * 20.0f);
        if (radiusDelta != 0.0f)
        {
            float radius = Math::Sqrt((m_fDist * m_fDist) + (m_fHeight * m_fHeight));
            if (radius > 0.001f)
            {
                // Scale both camera components by the same factor. This changes
                // only eye-to-target radius and preserves the current yaw/pitch.
                float radiusMin = 1.5f;
                float radiusMax = 80.0f;
                if (m_fHeight > 0.001f)
                {
                    float heightSafeMin = radius * (0.5f / m_fHeight);
                    float heightSafeMax = radius * (50.0f / m_fHeight);
                    if (heightSafeMin > radiusMin)
                        radiusMin = heightSafeMin;
                    if (heightSafeMax < radiusMax)
                        radiusMax = heightSafeMax;
                };

                float nextRadius = Clamp(radius + radiusDelta, radiusMin, radiusMax);
                float radiusScale = nextRadius / radius;
                m_fDist *= radiusScale;
                m_fHeight *= radiusScale;
            };
        };
    }
    else
#endif /* defined(TARGET_PC) && defined(TMNT2_DEBUG_TOOLS) */
    if (IGamepad::GetDigital(controller, IGamepad::DIGITAL_L1))
        m_fDist += (CGameProperty::GetElapsedTime() * 20.0f);
    else if (IGamepad::GetDigital(controller, IGamepad::DIGITAL_R1))
        m_fDist -= (CGameProperty::GetElapsedTime() * 20.0f);
    
    if (IGamepad::GetDigital(controller, IGamepad::DIGITAL_LUP))
        m_fLookatOffsetY += (CGameProperty::GetElapsedTime() * 20.0f);
    else if (IGamepad::GetDigital(controller, IGamepad::DIGITAL_LDOWN))
        m_fLookatOffsetY -= (CGameProperty::GetElapsedTime() * 20.0f);

    m_vAt = *pvAt;
    m_vAt.y += m_fLookatOffsetY;

    RwMatrix matRotY;
    RwMatrixSetIdentity(&matRotY);
    Math::Matrix_RotateY(&matRotY, m_fRotY);

    RwV3d vecFront = Math::VECTOR3_ZERO;
    RwV3dTransformVector(&vecFront, &Math::VECTOR3_AXIS_Z, &matRotY);
    Math::Vec3_Normalize(&vecFront, &vecFront);
    Math::Vec3_Scale(&vecFront, &vecFront, m_fDist);

    Math::Vec3_Sub(&m_vEye, &m_vAt, &vecFront);
    m_vEye.y += m_fHeight;

    UpdateLookat();
};


void CMapCamera::UpdatePathCamera(const RwV3d* pvAt, bool bExPath)
{
    RwV3d vAt = Math::VECTOR3_ZERO;
    vAt = *pvAt;
    vAt.y += m_fLookatOffsetY;

    if (m_bChangeMoment)
        m_vAt = vAt;
    else
        Math::Vec3_Lerp(&m_vAt, &m_vAt, &vAt, LerpFactor(0.2f));

    int32 nAtPathID = -1;
    int32 nEyePathID = -1;

    if (std::strcmp(m_szCameraAreaName, ""))
    {
        char szPathName[32];
        szPathName[0] = '\0';

        MakePathEyeCameraName(szPathName, m_szCameraAreaName);
        nEyePathID = CCameraDataManager::GetPathIDFromName(szPathName);

        MakePathAtCameraName(szPathName, m_szCameraAreaName);
        nAtPathID = CCameraDataManager::GetPathIDFromName(szPathName);
    };

    if ((nEyePathID >= 0) &&
        (nAtPathID  >= 0))
    {
        m_fPathTime = CCameraDataManager::FindNearestPosValueLight(&m_vAt, nAtPathID, m_fPathTime);

        if (CCameraDataManager::GetPathType(nAtPathID) == 1)
        {
            const float fMinPathT = 0.0001f;

            if (m_fPathTime > m_fPrePathTime)
            {
                if (m_fPathTime >= (1.0f - fMinPathT))
                    m_fPathTime = (1.0f - fMinPathT);
            }
            else if (m_fPathTime <= fMinPathT)
            {
                m_fPathTime = fMinPathT;
            };
        };

        RwV3d vEye = Math::VECTOR3_ZERO;
        CCameraDataManager::GetSplinePos(&vEye, nEyePathID, m_fPathTime);

        if (bExPath)
        {
            float fFrontPathT = m_fPathTime + 0.01f;
            if (fFrontPathT > 1.0f)
                fFrontPathT = m_fPathTime - 0.01f;

            RwV3d vFrontEyePos = Math::VECTOR3_ZERO;
            CCameraDataManager::GetSplinePos(&vFrontEyePos, nEyePathID, fFrontPathT);

            RwV3d vLineV = Math::VECTOR3_ZERO;
            Math::Vec3_Sub(&vLineV, &vFrontEyePos, &vEye);
            vLineV.y = 0.0f;

            Math::Vec3_Normalize(&vLineV, &vLineV);

            float fDist = (((pvAt->x - vEye.x) * -vLineV.z) + ((pvAt->z - vEye.z) * vLineV.x)) /
                           (-vLineV.z * -vLineV.z + vLineV.y * vLineV.y + vLineV.x * vLineV.x);

            vEye.x += (-vLineV.z * fDist);
            vEye.z += (vLineV.x * fDist);
        };

        RwV3d vPathLookatPos = Math::VECTOR3_ZERO;
        CCameraDataManager::GetSplinePos(&vPathLookatPos, nAtPathID, m_fPathTime);

        float fTmp = vPathLookatPos.y - 5.0f;
        if (fTmp > m_vAt.y)
            m_vAt.y = fTmp;

        m_fPrePathTime = m_fPathTime;

        /*
         *  Framing: required zoom is solved against authored eye position, so result
         *  is absolute and does not depend on how far camera is zoomed at the moment.
         */
        float fZoom = m_fRequestZoom;
        float fFovZoom = 1.0f;

        FRAMEBASIS basis;
        bool bFraming = (IsFramingActive() && MakeFrameBasis(&basis, &vEye, &m_vAt, TYPEDEF::DEFAULT_VIEWWINDOW));
        if (bFraming)
        {
            float fRequestOut = CalcFrameDollyZoom(basis, 1.0f);
            float fRequestIn  = CalcFrameDollyZoom(basis, (m_tuning.m_fReturnMargin / m_tuning.m_fSafeMargin));

            StepFrameZoom(&m_fFrameZoom, fRequestOut, fRequestIn, m_tuning.m_fZoomMax);

            fZoom = (m_fFrameZoom * m_fRequestZoom);

            m_fFrameReach = CalcFrameReach(basis, m_tuning.m_fZoomMax, 1.0f);
            m_frameDiag.m_fZoomRequest = fRequestOut;
        }
        else
        {
            m_fFrameZoom = 1.0f;
        };

        RwV3d vNewEye = Math::VECTOR3_ZERO;
        vNewEye.x = m_vAt.x + ((vEye.x - m_vAt.x) * fZoom);
        vNewEye.y = m_vAt.y + ((vEye.y - m_vAt.y) * fZoom);
        vNewEye.z = m_vAt.z + ((vEye.z - m_vAt.z) * fZoom);

        if ((fZoom > 1.0f) && CWorldMap::CheckCollisionLine(&vEye, &vNewEye))
        {
            vNewEye = *CWorldMap::GetCollisionResultClosestPoint();

            if (bFraming)
            {
                /* keep eye slightly off the wall instead of exactly on its surface */
                const float fWallOffset = 0.3f;

                RwV3d vBack = Math::VECTOR3_ZERO;
                Math::Vec3_Sub(&vBack, &vEye, &vNewEye);

                float fBackLen = Math::Vec3_Length(&vBack);
                if (fBackLen > fWallOffset)
                {
                    Math::Vec3_Scale(&vBack, &vBack, (fWallOffset / fBackLen));
                    Math::Vec3_Add(&vNewEye, &vNewEye, &vBack);
                }
                else
                {
                    vNewEye = vEye;
                };

                /* dolly is blocked by map - widen the view for what is left (limited) */
                RwV3d vReached = Math::VECTOR3_ZERO;
                Math::Vec3_Sub(&vReached, &vNewEye, &m_vAt);

                fZoom = (Math::Vec3_Length(&vReached) / basis.m_fDist);
                fFovZoom = CalcFrameExtent(basis, fZoom, 1.0f, false);
                fFovZoom = Clamp(fFovZoom, 1.0f, m_tuning.m_fFovZoomMax);
            };
        };

        if (bFraming)
        {
            CalcFrameExtent(basis, fZoom, 1.0f, true);

            m_fFrameFovZoom = fFovZoom;
            m_frameDiag.m_bFraming = true;
            m_frameDiag.m_fZoom = fZoom;
            m_frameDiag.m_fFovZoom = fFovZoom;
        };

        vEye = vNewEye;

        m_fChangeTimer -= CGameProperty::GetElapsedTime();
        if (m_fChangeTimer <= 0.0f)
            Math::Vec3_Lerp(&m_vEye, &m_vEye, &vEye, LerpFactor(0.2f));
        else
            Math::Vec3_Lerp(&m_vEye, &m_vEye, &vEye, LerpFactor(0.1f));

        if (m_bChangeMoment)
            m_vEye = vEye;

        UpdateLookat();

        m_fViewSize += ((TYPEDEF::DEFAULT_VIEWWINDOW * fFovZoom) - m_fViewSize) * LerpFactor(0.1f);
        SetViewWindow(m_fViewSize);
    };
};


void CMapCamera::UpdateSetCamera(const RwV3d* pvAt)
{
	RwV3d vAt = *pvAt;
    vAt.y += m_fLookatOffsetY;

    if (m_bChangeMoment)
        m_vAt = vAt;
    else
        Math::Vec3_Lerp(&m_vAt, &m_vAt, &vAt, LerpFactor(0.4f));

    int32 cameraId = -1;

    if (!std::strcmp(m_szCameraAreaName, ""))
    {
        cameraId = CCameraDataManager::GetSetCamIDNearestPos(&m_vAt);
    }
    else
    {
		char szSetCamera[32];
		szSetCamera[0] = '\0';

        MakeSetCameraName(szSetCamera, m_szCameraAreaName);
        cameraId = CCameraDataManager::GetSetCamIDNearestPosFromName(&m_vAt, szSetCamera);
    };

    if (cameraId >= 0)
    {
		RwV3d vEye = *CCameraDataManager::GetSetCamPosEye(cameraId);

        if (m_bChangeMoment)
            m_vEye = vEye;
        else
            Math::Vec3_Lerp(&m_vEye, &m_vEye, &vEye, LerpFactor(0.04f));

		UpdateLookat();

        float fFov = CCameraDataManager::GetSetCamFov(cameraId);
        float fViewSize = Math::Tan(fFov * TYPEDEF::DEFAULT_VIEWWINDOW);

        /*
         *  Eye of set camera is authored and stays where it is,
         *  framing is corrected by widening the view only (limited).
         */
        FRAMEBASIS basis;
        if (IsFramingActive() && MakeFrameBasis(&basis, &vEye, &m_vAt, fViewSize))
        {
            float fRequestOut = CalcFrameExtent(basis, 1.0f, 1.0f, true);
            float fRequestIn  = CalcFrameExtent(basis, 1.0f, (m_tuning.m_fReturnMargin / m_tuning.m_fSafeMargin), false);

            StepFrameZoom(&m_fFrameFovZoom, fRequestOut, fRequestIn, m_tuning.m_fFovZoomMax);

            fViewSize *= m_fFrameFovZoom;

            m_fFrameZoom = 1.0f;
            m_fFrameReach = CalcFrameReach(basis, 1.0f, m_tuning.m_fFovZoomMax);
            m_frameDiag.m_bFraming = true;
            m_frameDiag.m_fZoom = 1.0f;
            m_frameDiag.m_fZoomRequest = fRequestOut;
            m_frameDiag.m_fFovZoom = m_fFrameFovZoom;
        }
        else
        {
            m_fFrameFovZoom = 1.0f;
        };

        m_fViewSize += LerpFactor(0.1f) * (fViewSize - m_fViewSize);
        
        SetViewWindow(m_fViewSize);
    };
};


void CMapCamera::UpdateFixedCamera(const RwV3d* pvAt)
{
    int32 cameraId = -1;

    if (std::strcmp(m_szCameraAreaName, ""))
        cameraId = CCameraDataManager::GetSetCamIDNearestPosFromName(&m_vAt, m_szCameraAreaName);

    if (cameraId >= 0)
    {
        /* update eye */
        RwV3d vEye = *CCameraDataManager::GetSetCamPosEye(cameraId);

        if (m_bChangeMoment)
            m_vEye = vEye;
        else
            Math::Vec3_Lerp(&m_vEye, &m_vEye, &vEye, LerpFactor(0.025f));

        /* update at */
        RwV3d vAt = *CCameraDataManager::GetSetCamPosLookat(cameraId);

        if (m_bChangeMoment)
            m_vAt = vAt;
        else
            Math::Vec3_Lerp(&m_vAt, &m_vAt, &vAt, LerpFactor(0.04f));

        UpdateLookat();

        /* update fov */
        float fFov = CCameraDataManager::GetSetCamFov(cameraId);
        float fViewSize = Math::Tan(fFov * TYPEDEF::DEFAULT_VIEWWINDOW);

        /*
         *  Fixed camera is fully authored (eye and lookat), move boundary is
         *  left as is and only view is widened when targets leave safe area.
         */
        FRAMEBASIS basis;
        if (IsFramingActive() && MakeFrameBasis(&basis, &vEye, &vAt, fViewSize))
        {
            float fRequestOut = CalcFrameExtent(basis, 1.0f, 1.0f, true);
            float fRequestIn  = CalcFrameExtent(basis, 1.0f, (m_tuning.m_fReturnMargin / m_tuning.m_fSafeMargin), false);

            StepFrameZoom(&m_fFrameFovZoom, fRequestOut, fRequestIn, m_tuning.m_fFovZoomMax);

            fViewSize *= m_fFrameFovZoom;

            m_fFrameZoom = 1.0f;
            m_frameDiag.m_bFraming = true;
            m_frameDiag.m_fZoom = 1.0f;
            m_frameDiag.m_fZoomRequest = fRequestOut;
            m_frameDiag.m_fFovZoom = m_fFrameFovZoom;
        }
        else
        {
            m_fFrameFovZoom = 1.0f;
        };

        /* original snaps view size here while eye is still gliding to its place */
        if (IsEnhanced() && !m_bChangeMoment)
            m_fViewSize += LerpFactor(0.1f) * (fViewSize - m_fViewSize);
        else
            m_fViewSize = fViewSize;

        m_fChangeTimer -= CGameProperty::GetElapsedTime();

        SetViewWindow(m_fViewSize);
    };
};


void CMapCamera::UpdateLookat(void)
{
    if (m_pVibration)
        m_pVibration->Update(&m_vEye, &m_vAt);
    
    LookAt(&m_vEye, &m_vAt, &Math::VECTOR3_AXIS_Y);
};


void CMapCamera::LookAt(const RwV3d* pvEye, const RwV3d* pvAt, const RwV3d* pvUp)
{
    RwMatrix matrix;
    Math::Matrix_LookAt(&matrix, pvEye, pvAt, pvUp);
	
    rwMatrixSetFlags(&matrix, rwMATRIXTYPEORTHONORMAL);

    RwCamera* pCamera = m_pCamera->GetRwCamera();
    RwFrame* pFrame = RwCameraGetFrame(pCamera);
    
    if (pFrame)
        RwFrameTransform(pFrame, &matrix, rwCOMBINEREPLACE);
};


void CMapCamera::Skip(void)
{
    if (m_pIntroduction)
        m_pIntroduction->Skip();
};


void CMapCamera::SetLookat(const RwV3d* pvEye, const RwV3d* pvAt)
{
    m_vEye = *pvEye;
    m_vAt = *pvAt;

    LookAt(&m_vEye, &m_vAt, &Math::VECTOR3_AXIS_Y);

    RwV3d vDir = Math::VECTOR3_ZERO;
    Math::Vec3_Sub(&vDir, &m_vAt, &m_vEye);

    m_fHeight = std::fabs(vDir.y);
    m_fRotY = Math::ATan2(vDir.x, vDir.z);
    
    vDir.y = 0.0f;
    m_fDist = Math::Vec3_Length(&vDir);
};


void CMapCamera::SetCameraVibration(float fPower, float fTime, int32 nFreq)
{
    VibrationCreate(fPower, fTime, nFreq);
};


void CMapCamera::StopCameraVibration(void)
{
    VibrationDestroy();
};


void CMapCamera::SetFarClipPlane(float fFarClipPlane)
{
    ASSERT(m_pCamera);
    ASSERT(m_pCamera->GetRwCamera());
    
    RwCameraSetFarClipPlane(m_pCamera->GetRwCamera(), fFarClipPlane);
};


void CMapCamera::SetNearClipPlane(float fNearClipPlane)
{
    ASSERT(m_pCamera);
    ASSERT(m_pCamera->GetRwCamera());

    RwCameraSetNearClipPlane(m_pCamera->GetRwCamera(), fNearClipPlane);
};


void CMapCamera::SetFogDistance(float fFogDist)
{
    ASSERT(m_pCamera);
    ASSERT(m_pCamera->GetRwCamera());

    RwCameraSetFogDistance(m_pCamera->GetRwCamera(), fFogDist);
};


void CMapCamera::SetViewWindow(float fViewSize)
{
    ASSERT(m_pCamera);
    ASSERT(m_pCamera->GetRwCamera());

    CCamera::SetAspectCorrectViewWindow(m_pCamera->GetRwCamera(), fViewSize);
};


void CMapCamera::SetViewWindow(float fViewSize, float fAspect)
{
    ASSERT(m_pCamera);
    ASSERT(m_pCamera->GetRwCamera());

    RwV2d ViewWindow;
#if defined(TARGET_PC)
    if (CPCModFeatures::IsWidescreenEnabled())
    {
        ViewWindow.y = fViewSize / TYPEDEF::DEFAULT_ASPECTRATIO;
        ViewWindow.x = ViewWindow.y * fAspect;
    }
    else
#endif /* defined(TARGET_PC) */
    {
        ViewWindow.x = fViewSize;
        ViewWindow.y = fViewSize / fAspect;
    };

    RwCameraSetViewWindow(m_pCamera->GetRwCamera(), &ViewWindow);
};


void CMapCamera::SetCameraMode(MODE mode)
{
    m_mode = mode;
	m_bChangeMoment = true;
    
    switch (m_mode)
    {
    case MODE_INTRODUCTION:
        IntroductionCreate();
        break;

    default:
        break;
    };
};


CMapCamera::MODE CMapCamera::GetCameraMode(void) const
{
    return m_mode;
};


void CMapCamera::SetPathMode(PATHMODE pathmode)
{
    m_pathmode = pathmode;
};


#if defined(TMNT2_DEBUG_TOOLS)
void CMapCamera::DebugBeginShowcase(void)
{
    RwV3d eye = m_vEye;
    RwV3d lookat = m_vAt;
    m_fLookatOffsetY = 0.0f;
    SetLookat(&eye, &lookat);
    m_fHeight = Clamp(m_fHeight, 0.5f, 50.0f);
    m_fDist = Clamp(m_fDist, 1.5f, 80.0f);
    SetCameraMode(MODE_MANUAL);
};


void CMapCamera::DebugResetShowcase(PATHMODE pathmode)
{
    m_fLookatOffsetY = 0.0f;
    SetPathMode(pathmode);
    SetCameraMode(MODE_AUTOCHANGE);
};
#endif /* defined(TMNT2_DEBUG_TOOLS) */


bool CMapCamera::IsPosVisible(const RwV3d* pvPos)
{
    ASSERT(m_pCamera);
    
    RwCamera* pCamera = m_pCamera->GetRwCamera();
    RwMatrix* pViewMatrix = RwCameraGetViewMatrix(pCamera);
    
    RwV3d vScreenPos = Math::VECTOR3_ZERO;
    RwV3dTransformPoints(&vScreenPos, pvPos, 1, pViewMatrix);

    if (vScreenPos.z > 0.0f)
    {
        vScreenPos.x *= (1.0f / vScreenPos.z);
        vScreenPos.y *= (1.0f / vScreenPos.z);

        float fClipNear = RwCameraGetNearClipPlane(pCamera);
        float fClipFar  = RwCameraGetFarClipPlane(pCamera);

        if ((vScreenPos.x >= 0.0f) &&
            (vScreenPos.x <= 1.0f) &&
            (vScreenPos.y >= 0.0f) &&
            (vScreenPos.y <= 1.0f) &&
            (vScreenPos.z >= fClipNear) &&
            (vScreenPos.z <= fClipFar))
        {
            return true;
        };
    };

    return false;
};


float CMapCamera::CalcNiceZoom(RwV3d* avPos, int32 nNumPos)
{
    if (nNumPos <= 0)
        return 1.0f;

    RwMatrix* pViewMat = RwCameraGetViewMatrix(m_pCamera->GetRwCamera());

    float fMaxScrnX = 0.0f;
    float fMaxScrnY = 0.0f;

    for (int32 i = 0; i < nNumPos; ++i)
    {
        RwV3d vScrnPos = Math::VECTOR3_ZERO;
        RwV3dTransformPoint(&vScrnPos, &avPos[i], pViewMat);

        if (vScrnPos.z > 0.0f)
        {
            float fTmp = (1.0f / vScrnPos.z);

            vScrnPos.x = (vScrnPos.x * fTmp) + (vScrnPos.x * fTmp) - 1.0f;
            vScrnPos.y = (vScrnPos.y * fTmp) + (vScrnPos.y * fTmp) - 1.0f;

            vScrnPos.x = std::fabs(vScrnPos.x);
            vScrnPos.y = std::fabs(vScrnPos.y);

            if (fMaxScrnX < vScrnPos.x)
                fMaxScrnX = vScrnPos.x;

            if (fMaxScrnY < vScrnPos.y)
                fMaxScrnY = vScrnPos.y;
        };
    };

    if ((fMaxScrnX <= 1.0f) &&
        (fMaxScrnY <= 1.0f))
        return 1.0f;    

    float fZoom = ((fMaxScrnX >= fMaxScrnY) ? fMaxScrnX : fMaxScrnY);
    if (fZoom > 3.0f)
        fZoom = 3.0f;

    return fZoom;
};


bool CMapCamera::IsFramingActive(void) const
{
    return (m_bFramingRequest && IsEnhanced() && (m_nNumFrameTarget > 0));
};


bool CMapCamera::MakeFrameBasis(FRAMEBASIS* pBasis, const RwV3d* pvEye, const RwV3d* pvAt, float fViewSize) const
{
    ASSERT(pBasis);

    Math::Vec3_Sub(&pBasis->m_vFront, pvAt, pvEye);

    float fDist = Math::Vec3_Length(&pBasis->m_vFront);
    if ((fDist < 0.01f) || (fViewSize <= 0.0f))
        return false;

    Math::Vec3_Scale(&pBasis->m_vFront, &pBasis->m_vFront, (1.0f / fDist));

    Math::Vec3_Cross(&pBasis->m_vRight, &Math::VECTOR3_AXIS_Y, &pBasis->m_vFront);

    float fRightLen = Math::Vec3_Length(&pBasis->m_vRight);
    if (fRightLen < 0.001f)
        pBasis->m_vRight = Math::VECTOR3_AXIS_X; /* looking straight up or down */
    else
        Math::Vec3_Scale(&pBasis->m_vRight, &pBasis->m_vRight, (1.0f / fRightLen));

    Math::Vec3_Cross(&pBasis->m_vUp, &pBasis->m_vFront, &pBasis->m_vRight);

    /*
     *  fViewSize is the 4:3 horizontal view size. Vertical view never depends on the
     *  display aspect and horizontal one follows it (widescreen), so take the real
     *  aspect from what camera is using.
     */
    float fAspect = TYPEDEF::DEFAULT_ASPECTRATIO;

    const RwV2d* pViewWindow = RwCameraGetViewWindow(m_pCamera->GetRwCamera());
    if (pViewWindow && (pViewWindow->x > 0.0f) && (pViewWindow->y > 0.0f))
        fAspect = (pViewWindow->x / pViewWindow->y);

    pBasis->m_vAt    = *pvAt;
    pBasis->m_fDist  = fDist;
    pBasis->m_fViewY = (fViewSize / TYPEDEF::DEFAULT_ASPECTRATIO);
    pBasis->m_fViewX = (pBasis->m_fViewY * fAspect);

    return true;
};


/*
 *  Returns multiplier of authored eye distance that is required to have all targets inside
 *  of the safe area. Eye moves along view axis only, so lateral offsets of targets in camera
 *  space do not depend on zoom and inequality "|x| <= margin * view * depth(zoom)" is solved
 *  for zoom directly.
 */
float CMapCamera::CalcFrameDollyZoom(const FRAMEBASIS& rBasis, float fMarginScale) const
{
    const float fMinDepth = 1.0f;

    float fZoom = 0.0f;

    for (int32 i = 0; i < m_nNumFrameTarget; ++i)
    {
        const FRAMETARGET& rTarget = m_aFrameTarget[i];

        float fMargin = (rTarget.m_bPrimary ? m_tuning.m_fSafeMargin : m_tuning.m_fSecondaryMargin);
        fMargin = Clamp((fMargin * fMarginScale), 0.1f, 1.0f);

        for (int32 j = 0; j < 2; ++j)
        {
            RwV3d vPt = rTarget.m_vPos;
            vPt.y += (j ? rTarget.m_fUp : -rTarget.m_fDown);

            RwV3d vOfs = Math::VECTOR3_ZERO;
            Math::Vec3_Sub(&vOfs, &vPt, &rBasis.m_vAt);

            float x = std::fabs(Math::Vec3_Dot(&vOfs, &rBasis.m_vRight));
            float y = std::fabs(Math::Vec3_Dot(&vOfs, &rBasis.m_vUp));
            float z = Math::Vec3_Dot(&vOfs, &rBasis.m_vFront);

            float fZoomX = (((x / (fMargin * rBasis.m_fViewX)) - z) / rBasis.m_fDist);
            float fZoomY = (((y / (fMargin * rBasis.m_fViewY)) - z) / rBasis.m_fDist);
            float fZoomZ = ((fMinDepth - z) / rBasis.m_fDist);

            fZoom = Max(fZoom, Max(fZoomX, Max(fZoomY, fZoomZ)));
        };
    };

    return fZoom;
};


/*
 *  Returns how far the farthest target is from the screen center in units of its margin
 *  for eye at specified zoom: 1.0 is exactly on the safe area border. This is also the
 *  scale of the view window that is required to fit all targets without moving the eye.
 */
float CMapCamera::CalcFrameExtent(const FRAMEBASIS& rBasis, float fDollyZoom, float fMarginScale, bool bDiag)
{
    const float fMinDepth = 0.1f;
    const float fExtentMax = 100.0f;

    float fExtent = 0.0f;

    if (bDiag)
    {
        m_frameDiag.m_nNumTarget = m_nNumFrameTarget;
        m_frameDiag.m_nNumOutside = 0;
    };

    for (int32 i = 0; i < m_nNumFrameTarget; ++i)
    {
        const FRAMETARGET& rTarget = m_aFrameTarget[i];

        float fMargin = (rTarget.m_bPrimary ? m_tuning.m_fSafeMargin : m_tuning.m_fSecondaryMargin);
        fMargin = Clamp((fMargin * fMarginScale), 0.1f, 1.0f);

        float fExtentTarget = 0.0f;
        bool bBehind = false;

        for (int32 j = 0; j < 2; ++j)
        {
            RwV3d vPt = rTarget.m_vPos;
            vPt.y += (j ? rTarget.m_fUp : -rTarget.m_fDown);

            RwV3d vOfs = Math::VECTOR3_ZERO;
            Math::Vec3_Sub(&vOfs, &vPt, &rBasis.m_vAt);

            float x = std::fabs(Math::Vec3_Dot(&vOfs, &rBasis.m_vRight));
            float y = std::fabs(Math::Vec3_Dot(&vOfs, &rBasis.m_vUp));
            float z = Math::Vec3_Dot(&vOfs, &rBasis.m_vFront) + (rBasis.m_fDist * fDollyZoom);

            if (z < fMinDepth)
            {
                /* behind the eye - no view window is able to fit it, so do not try */
                bBehind = true;
                continue;
            };

            float fScrX = (x / (z * rBasis.m_fViewX));
            float fScrY = (y / (z * rBasis.m_fViewY));

            fExtentTarget = Max(fExtentTarget, (Max(fScrX, fScrY) / fMargin));
        };

        if (bDiag)
        {
            RwV3d vOfs = Math::VECTOR3_ZERO;
            Math::Vec3_Sub(&vOfs, &rTarget.m_vPos, &rBasis.m_vAt);

            float z = Math::Vec3_Dot(&vOfs, &rBasis.m_vFront) + (rBasis.m_fDist * fDollyZoom);
            if (z < fMinDepth)
                z = fMinDepth;

            m_frameDiag.m_afScreenX[i] = (Math::Vec3_Dot(&vOfs, &rBasis.m_vRight) / (z * rBasis.m_fViewX));
            m_frameDiag.m_afScreenY[i] = (Math::Vec3_Dot(&vOfs, &rBasis.m_vUp)    / (z * rBasis.m_fViewY));

            if (bBehind || (fExtentTarget > 1.001f))
                ++m_frameDiag.m_nNumOutside;
        };

        fExtent = Max(fExtent, fExtentTarget);
    };

    return Min(fExtent, fExtentMax);
};


/*
 *  Returns radius of the circle around lookat that stays on screen for specified zoom:
 *  limited by screen sides and, for the one who is moving towards the camera, by bottom
 *  of the screen (feet are m_fTargetHalfHeight below the lookat level).
 */
float CMapCamera::CalcFrameReach(const FRAMEBASIS& rBasis, float fDollyZoom, float fFovZoom) const
{
    const float fEdge = 0.95f;

    float fDist  = (rBasis.m_fDist * fDollyZoom);
    float fViewX = (rBasis.m_fViewX * fFovZoom * fEdge);
    float fViewY = (rBasis.m_fViewY * fFovZoom * fEdge);
    float fFeet  = m_tuning.m_fTargetHalfHeight;

    float fSin = std::fabs(rBasis.m_vFront.y);
    float fCos = Math::Sqrt(Max(0.0f, (1.0f - (fSin * fSin))));

    float fReachSide = (fViewX * fDist);
    float fReachNear = (((fViewY * (fDist + (fFeet * fSin))) - (fFeet * fCos)) / Max(0.001f, (fSin + (fViewY * fCos))));

    /* never up to the eye itself */
    fReachNear = Min(fReachNear, (fDist * 0.8f));

    return Max(0.0f, Min(fReachSide, fReachNear));
};


/*
 *  Zooms out quickly up to fRequestOut, returns slowly and after delay down to fRequestIn.
 *  Requests are solved for two different margins (fRequestIn >= fRequestOut), everything
 *  in between is a dead band where zoom is not changing at all - that prevents zoom from
 *  oscillating when targets are moving near the safe area border.
 */
void CMapCamera::StepFrameZoom(float* pfZoom, float fRequestOut, float fRequestIn, float fZoomMax)
{
    ASSERT(pfZoom);

    if (fZoomMax < 1.0f)
        fZoomMax = 1.0f;

    /* request may be garbage if target positions are garbage */
    if (!(fRequestOut == fRequestOut) || !(fRequestIn == fRequestIn))
        return;

    fRequestOut = Clamp(fRequestOut, 1.0f, fZoomMax);
    fRequestIn  = Clamp(fRequestIn,  1.0f, fZoomMax);

    if (fRequestIn < fRequestOut)
        fRequestIn = fRequestOut;

    if (m_bChangeMoment)
    {
        *pfZoom = fRequestOut;
        m_fFrameZoomHold = m_tuning.m_fZoomInDelay;
        return;
    };

    float fZoom = Clamp(*pfZoom, 1.0f, fZoomMax);

    if (fZoom < fRequestOut)
    {
        /* far behind the request means that someone is leaving the screen right now */
        float fSpeed = m_tuning.m_fZoomOutSpeed;
        if (fRequestOut > (fZoom * 1.25f))
            fSpeed *= 2.0f;

        fZoom += ((fRequestOut - fZoom) * ExpFactor(fSpeed));
        m_fFrameZoomHold = m_tuning.m_fZoomInDelay;
    }
    else if (fZoom > fRequestIn)
    {
        if (m_fFrameZoomHold > 0.0f)
            m_fFrameZoomHold -= CGameProperty::GetElapsedTime();
        else
            fZoom += ((fRequestIn - fZoom) * ExpFactor(m_tuning.m_fZoomInSpeed));
    }
    else
    {
        m_fFrameZoomHold = m_tuning.m_fZoomInDelay;
    };

    *pfZoom = fZoom;
};


const CMapCamera::FRAMEDIAG& CMapCamera::GetFrameDiag(void) const
{
    return m_frameDiag;
};


const char* CMapCamera::GetCameraAreaName(void) const
{
    return m_szCameraAreaName;
};


RwCamera* CMapCamera::GetRwCamera(void) const
{
    return m_pCamera->GetRwCamera();
};


bool CMapCamera::IsIntroductionEnd(void) const
{
    return (m_pIntroduction == nullptr);
};


float CMapCamera::GetViewRatio(void) const
{
	return (m_fDefaultViewSize / m_fViewSize);
};


void CMapCamera::GetEye(RwV3d* pvEye) const
{
    ASSERT(pvEye);

    *pvEye = m_vEye;
};


void CMapCamera::GetLookat(RwV3d* pvLookat) const
{
    ASSERT(pvLookat);

    *pvLookat = m_vAt;
};


float CMapCamera::GetViewAreaRadius(void) const
{
    return m_fLookatViewAreaRadius;
};


bool CMapCamera::FrustumSphereTest(const RwV3d* pos, float fRadius) const
{
    RwSphere sphere;
    sphere.center = *pos;
    sphere.radius = fRadius;

    return FrustumSphereTest(&sphere);
};


bool CMapCamera::FrustumSphereTest(const RwSphere* sphere) const
{
    return (RwCameraFrustumTestSphere(m_pCamera->GetRwCamera(), sphere) != rwSPHEREOUTSIDE);
};


void CMapCamera::IntroductionCreate(void)
{
    if (!m_pIntroduction)
        m_pIntroduction = new CIntroduction;
};


void CMapCamera::IntroductionDestroy(void)
{
    if (m_pIntroduction)
    {
        delete m_pIntroduction;
        m_pIntroduction = nullptr;
    };
};


void CMapCamera::IntroductionUpdate(void)
{
    if (m_pIntroduction)
    {
        RwV3d vEye = Math::VECTOR3_ZERO;
        RwV3d vAt = Math::VECTOR3_ZERO;
        
        if (m_pIntroduction->Update(&vEye, &vAt))
        {
            m_vEye = vEye;
            m_vAt = vAt;

            LookAt(&m_vEye, &m_vAt, &Math::VECTOR3_AXIS_Y);
            SetViewWindow(m_fViewSize);
        }
        else
        {
            IntroductionDestroy();
        };
    };
};


void CMapCamera::VibrationCreate(float fPower, float fTime, int32 nFreq)
{
    if (!m_pVibration)
        m_pVibration = new CVibration(fPower, fTime, nFreq);
};


void CMapCamera::VibrationDestroy(void)
{
    if (m_pVibration)
    {
        delete m_pVibration;
        m_pVibration = nullptr;
    };
};


void CMapCamera::VibrationUpdate(void)
{
    if (m_pVibration)
    {
        m_pVibration->Period();        
        if (!m_pVibration->IsVibrating())
            VibrationDestroy();
    };
};
