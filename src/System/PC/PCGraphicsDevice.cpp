#include "PCGraphicsDevice.hpp"
#include "PCFrameTimer.hpp"
#include "PCSpecific.hpp"
#include "PCSetting.hpp"
#include "PCError.hpp"
#include "PCCrashReporter.hpp"

#include "System/Common/Configure.hpp"
#include "System/Common/Screen.hpp"

#if defined(TMNT2_RWDRV_D3D9)
#include <d3d9.h>
#endif /* defined(TMNT2_RWDRV_D3D9) */


#if defined(TMNT2_RWDRV_OPENGL)

#define RwDrvChangeVideoMode \
    RwOpenGLChangeVideoMode

#define RwDrvSetRefreshRate \
    // no op

#define RwDrvSetMultiSamplingLevels(_levels) \
    ((void)(_levels))

#define RwDrvChangeMultiSamplingLevels(_levels) \
    (true)

#define RwDrvGetMaxMultiSamplingLevels() \
    (0)

#elif defined(TMNT2_RWDRV_D3D9)

#define RwDrvChangeVideoMode \
    RwD3D9ChangeVideoMode

#define RwDrvSetRefreshRate \
    RwD3D9EngineSetRefreshRate

#define RwDrvSetMultiSamplingLevels \
    RwD3D9EngineSetMultiSamplingLevels

#define RwDrvChangeMultiSamplingLevels \
    RwD3D9ChangeMultiSamplingLevels

#define RwDrvGetMaxMultiSamplingLevels \
    RwD3D9EngineGetMaxMultiSamplingLevels

#endif


namespace
{
    static int32 SelectSupportedMSAASamples(int32 nRequestedSamples, int32 nMaxSamples)
    {
        const int32 nLimit = std::min(nRequestedSamples, nMaxSamples);

        if (nLimit >= 8)
            return 8;

        if (nLimit >= 4)
            return 4;

        if (nLimit >= 2)
            return 2;

        return 0;
    };


    static uint32 GetRenderWareMultiSamplingLevels(int32 nSamples)
    {
        return (nSamples >= 2 ? static_cast<uint32>(nSamples) : 1u);
    };
}; /* anonymous namespace */

#define NOASM
#if defined(TMNT2_RWDRV_OPENGL)

//
//  There is a problem to run prebuild RWSDK37 with opengl driver
//  all code from baprocfp.obj for some reason is placed in data segment 
//  instead code segment so change protection to EXEC for this
// 


extern "C" void _rwProcessorInitialize(void);
extern "C" void _rwProcessorRelease(void);
extern "C" void _rwProcessorForceSinglePrecision(void);


static bool 
FixPrebuiltOpenglRwSDK37(void)
{
#if !defined(NOASM)
    SYSTEM_INFO sysInfo = {};
    GetSystemInfo(&sysInfo);

    DWORD dwPageSize = sysInfo.dwPageSize;
    DWORD dwNewProt = PAGE_EXECUTE_WRITECOPY;
    DWORD dwOldProt[3] = {};
    BOOL bResult[3] = {};

    bResult[0] = VirtualProtect(&_rwProcessorInitialize, dwPageSize, dwNewProt, &dwOldProt[0]);
    bResult[1] = VirtualProtect(&_rwProcessorRelease, dwPageSize, dwNewProt, &dwOldProt[1]);
    bResult[2] = VirtualProtect(&_rwProcessorForceSinglePrecision, dwPageSize, dwNewProt, &dwOldProt[2]);

    return (bResult[0] &&
            bResult[1] &&
            bResult[2]);
#else /* !defined(NOASM) */
    return true;
#endif /* !defined(NOASM) */
};

#endif /* defined(TMNT2_RWDRV_OPENGL) */


struct CPCGraphicsDevice::VIDEOMODE : public RwVideoMode
{
    int32   m_index;
    int32   m_maxMultiSamplingLevels;
    char    m_szName[64];

    static int32 SortCallback(const void* a, const void* b);
    static bool Eval(const VIDEOMODE* pVideomode);
};


struct CPCGraphicsDevice::DEVICEINFO : public RwSubSystemInfo
{
    VIDEOMODE*  m_pModes;
    int32       m_numModes;
    int32       m_curMode;
    int32       m_numMultisamplingLvls;
    int32       m_idxModeWnd;
    bool        m_bModeWndExist;
};


/*static*/ int32 CPCGraphicsDevice::VIDEOMODE::SortCallback(const void* a, const void* b)
{
    const VIDEOMODE* pVideomodeA = static_cast<const VIDEOMODE*>(a);
    ASSERT(pVideomodeA);

    const VIDEOMODE* pVideomodeB = static_cast<const VIDEOMODE*>(b);
    ASSERT(pVideomodeB);

    if (pVideomodeA->depth > pVideomodeB->depth)
        return 1;

    if (pVideomodeA->depth < pVideomodeB->depth)
        return -1;

    if (pVideomodeA->width > pVideomodeB->width)
        return 1;

    if (pVideomodeA->width < pVideomodeB->width)
        return -1;

    if (pVideomodeA->height > pVideomodeB->height)
        return 1;

    if (pVideomodeA->height < pVideomodeB->height)
        return -1;

    return 0;
};


/*static*/ bool CPCGraphicsDevice::VIDEOMODE::Eval(const VIDEOMODE* pVideomode)
{
    const int32 iMinWidth  = CPCSetting::VIDEOMODE_DEFAULT.w;
    const int32 iMinHeight = CPCSetting::VIDEOMODE_DEFAULT.h;
    const int32 iMinDepth = 16;
    const int32 iMaxDepth = 32;
    
    if ((pVideomode->width  < iMinWidth) ||
        (pVideomode->height < iMinHeight))
        return false;

    if ((pVideomode->depth < iMinDepth) ||
        (pVideomode->depth > iMaxDepth))
        return false;

    if (!(pVideomode->flags & rwVIDEOMODEEXCLUSIVE))
        return true;

    static const RwV2d s_aAspectRatio[] =
    {
        { 4,    3   },
        { 5,    4   },
        { 16,   9   },
    };

    for (int32 i = 0; i < COUNT_OF(s_aAspectRatio); ++i)
    {
        float fCurrentAspect = static_cast<float>(pVideomode->width) /
                               static_cast<float>(pVideomode->height);
        float fTargetAspect = s_aAspectRatio[i].x / s_aAspectRatio[i].y;

        /* Accept common near-16:9 PC modes such as 1366x768 and 1360x768. */
        if (std::fabs(fCurrentAspect - fTargetAspect) < 0.01f)
            return true;
    };

    return false;
};


CPCGraphicsDevice::CPCGraphicsDevice(void)
: m_pFrameTimer(nullptr)
, m_pDeviceInfo(nullptr)
, m_numDevices(0)
, m_curDevice(-1)
, m_multisamplingLvl(0)
, m_bFullscreen(false)
, m_bHighReso(false)
{
    ;
};


CPCGraphicsDevice::~CPCGraphicsDevice(void)
{
    ;
};


bool CPCGraphicsDevice::Initialize(void)
{
    if (!CGraphicsDevice::Initialize())
    {
        CPCError::ShowNoRet("Video Initialization Failed");
        return false;
    };

    m_pFrameTimer = new CPCFrameTimer(*this);
    if (!m_pFrameTimer)
        return false;

    SetMultiSamplingBeforeStart();

    if (m_bFullscreen)
    {
        uint32 refreshRate = 60;
#ifdef TMNT2_BUILD_EU
        if ((CConfigure::GetTVMode() == TYPEDEF::CONFIG_TV_PAL) && IsPalMode())
            refreshRate = 50;
#endif /* TMNT2_BUILD_EU */

        RwDrvSetRefreshRate(refreshRate);
    };

    SetWindowRect(m_pDeviceInfo[m_curDevice].m_pModes[m_pDeviceInfo[m_curDevice].m_curMode].width,
                  m_pDeviceInfo[m_curDevice].m_pModes[m_pDeviceInfo[m_curDevice].m_curMode].height);
    
    return true;
};


void CPCGraphicsDevice::Terminate(void)
{
    Cleanup();
    
    if (m_pFrameTimer)
    {
        delete m_pFrameTimer;
        m_pFrameTimer = nullptr;
    };

    CGraphicsDevice::Terminate();
};


bool CPCGraphicsDevice::Start(void)
{
#if defined(TMNT2_RWDRV_OPENGL)
    FixPrebuiltOpenglRwSDK37();
#endif /* defined(TMNT2_RWDRV_OPENGL) */
    
    if (!CGraphicsDevice::Start())
    {
        CPCError::ShowNoRet("Video Start Failed");
        return false;
    };

    RwImageSetGamma(1.2f);

    const int32 nExpectedSamples = m_multisamplingLvl;
    const int32 nActualSamples =
        TraceActualMultiSampling("engine_start", nExpectedSamples);

    if (nActualSamples >= 0)
    {
        m_multisamplingLvl = nActualSamples;
        m_pDeviceInfo[m_curDevice].m_numMultisamplingLvls = nActualSamples;
    };

    CPCCrashReporter::Breadcrumb(
        "MSAA engine_start expected=%d actual=%d fullscreen=%d",
        nExpectedSamples,
        nActualSamples,
        (m_bFullscreen ? 1 : 0));

    if (m_bFullscreen &&
        (nExpectedSamples >= 2) &&
        (nActualSamples >= 0) &&
        (nActualSamples != nExpectedSamples))
    {
        CPCCrashReporter::Breadcrumb(
            "MSAA engine_start repair expected=%d actual=%d action=disable_then_enable",
            nExpectedSamples,
            nActualSamples);

        RwDrvChangeMultiSamplingLevels(1u);
        ChangeMultiSamplingAfterStart("engine_start_repair");
    };

    return true;
};


void CPCGraphicsDevice::Flip(void)
{
    ASSERT(m_pFrameTimer);    

    m_pFrameTimer->Update();
    CGraphicsDevice::Flip();
    m_pFrameTimer->Sync();
};


int32 CPCGraphicsDevice::ScreenWidth(void)
{
    ASSERT(m_pDeviceInfo);
    ASSERT(m_numDevices > 0);
    ASSERT(m_curDevice >= 0);
    ASSERT(m_curDevice < m_numDevices);

    if (IsFullscreenWindow())
    {
        RECT rc;
        SetRectEmpty(&rc);
        GetClientRect(CPCSpecific::m_hWnd, &rc);

        return (rc.right - rc.left);
    };

    int32 curMode = m_pDeviceInfo[m_curDevice].m_curMode;

    ASSERT(curMode >= 0);
    ASSERT(curMode <= m_pDeviceInfo[m_curDevice].m_numModes);

    return m_pDeviceInfo[m_curDevice].m_pModes[curMode].width;
};


int32 CPCGraphicsDevice::ScreenHeight(void)
{
    ASSERT(m_pDeviceInfo);
    ASSERT(m_numDevices > 0);
    ASSERT(m_curDevice >= 0);
    ASSERT(m_curDevice < m_numDevices);

    if (IsFullscreenWindow())
    {
        RECT rc;
        SetRectEmpty(&rc);
        GetClientRect(CPCSpecific::m_hWnd, &rc);

        return (rc.bottom - rc.top);
    };

    int32 curMode = m_pDeviceInfo[m_curDevice].m_curMode;

    ASSERT(curMode >= 0);
    ASSERT(curMode <= m_pDeviceInfo[m_curDevice].m_numModes);

    return m_pDeviceInfo[m_curDevice].m_pModes[curMode].height;
};


int32 CPCGraphicsDevice::ScreenDepth(void)
{
    ASSERT(m_pDeviceInfo);
    ASSERT(m_numDevices > 0);
    ASSERT(m_curDevice >= 0);
    ASSERT(m_curDevice < m_numDevices);

    int32 curMode = m_pDeviceInfo[m_curDevice].m_curMode;

    ASSERT(curMode >= 0);
    ASSERT(curMode <= m_pDeviceInfo[m_curDevice].m_numModes);

    return m_pDeviceInfo[m_curDevice].m_pModes[curMode].depth;
};


void* CPCGraphicsDevice::Configure(void)
{
    return CPCSpecific::m_hWnd;
};


int32 CPCGraphicsDevice::Subsystem(void)
{
    bool bResult = EnumerateVideomodes();
    ASSERT(bResult);

#ifdef _DEBUG
    OutputInfo();
#endif    

    m_bFullscreen = !CPCSetting::m_bWindowMode;
    m_curDevice = SearchAndSetVideomode(CPCSetting::m_videomode, false);
    
    if (m_curDevice == -1)
    {
        OUTPUT(
            "Settings videomode \"%d x %d x %d\" not found. Set to default.\n",
            CPCSetting::m_videomode.w,
            CPCSetting::m_videomode.h,
            CPCSetting::m_videomode.d
        );
        
        m_curDevice = SearchAndSetVideomode(CPCSetting::VIDEOMODE_DEFAULT, false);
    };

    ASSERT(m_curDevice >= 0);
    ASSERT(m_curDevice < m_numDevices);

    return m_curDevice;
};


int32 CPCGraphicsDevice::Videomode(void)
{
    if (m_bFullscreen)
        return m_pDeviceInfo[m_curDevice].m_pModes[m_pDeviceInfo->m_curMode].m_index;
    
    return m_pDeviceInfo[m_curDevice].m_idxModeWnd;
};


bool CPCGraphicsDevice::EnumerateVideomodes(void)
{
    ASSERT(!m_pDeviceInfo);
    
    m_numDevices = RwEngineGetNumSubSystems();
    if (!m_numDevices)
        return false;

    m_pDeviceInfo = new DEVICEINFO[m_numDevices];
    if (!m_pDeviceInfo)
    {
        m_numDevices = 0;
        return false;
    };

    for (int32 i = 0; i < m_numDevices; ++i)
    {
        DEVICEINFO* pDeviceInfo = &m_pDeviceInfo[i];
        
        RwEngineGetSubSystemInfo(pDeviceInfo, i);
        RwEngineSetSubSystem(i);
        int32 numModes = RwEngineGetNumVideoModes();
        
        pDeviceInfo->m_numMultisamplingLvls = 0;
        pDeviceInfo->m_idxModeWnd = -1;
        pDeviceInfo->m_numModes = 0;
        pDeviceInfo->m_curMode = 0;
        pDeviceInfo->m_pModes = new VIDEOMODE[numModes];

        VIDEOMODE* pVideomode = &pDeviceInfo->m_pModes[0];
        
        for (int32 j = 0; j < numModes; ++j)
        {
            RwEngineGetVideoModeInfo(pVideomode, j);

            if (!VIDEOMODE::Eval(pVideomode))
                continue;
           
            if (pVideomode->flags & rwVIDEOMODEEXCLUSIVE)
            {
                pVideomode->m_index = j;
                std::sprintf(pVideomode->m_szName,
                             "%d x %d x %d",
                             pVideomode->width,
                             pVideomode->height,
                             pVideomode->depth);

                RwEngineSetVideoMode(j);
                pVideomode->m_maxMultiSamplingLevels = RwDrvGetMaxMultiSamplingLevels();

                ++pDeviceInfo->m_numModes;
                ++pVideomode;
            }
            else
            {
                pDeviceInfo->m_bModeWndExist = true;
                pDeviceInfo->m_idxModeWnd = j;
            };
        };

        std::qsort(pDeviceInfo->m_pModes, pDeviceInfo->m_numModes, sizeof(VIDEOMODE), VIDEOMODE::SortCallback);
    };

    if (m_numDevices > 0)
        return true;

    Cleanup();
    
    return false;
};


void CPCGraphicsDevice::Cleanup(void)
{
    for (int32 i = 0; i < m_numDevices; ++i)
    {
        if (m_pDeviceInfo[i].m_numModes)
        {
            delete[] m_pDeviceInfo[i].m_pModes;
            m_pDeviceInfo[i].m_pModes = nullptr;
            m_pDeviceInfo[i].m_numModes = 0;
        };
    };

    delete[] m_pDeviceInfo;
    m_pDeviceInfo = nullptr;
    m_numDevices = 0;
};


int32 CPCGraphicsDevice::SearchAndSetVideomode(const PC::VIDEOMODE& vm, bool bSearchInProgress)
{
    ASSERT(m_pDeviceInfo);
    ASSERT(m_numDevices > 0);
    ASSERT((!bSearchInProgress) || ((bSearchInProgress) && ((m_curDevice >= 0) && (m_curDevice < m_numDevices))));

    if (m_curDevice >= 0)
    {
        //
        //  first attempt is search videomode in current device
        //
        for (int32 i = 0; i < m_pDeviceInfo[m_curDevice].m_numModes; ++i)
        {
            VIDEOMODE* pVideomode = &m_pDeviceInfo[m_curDevice].m_pModes[i];

            if ((pVideomode->width  == vm.w)    &&
                (pVideomode->height == vm.h)    &&
                (pVideomode->depth  == vm.d))
            {
                if (!bSearchInProgress)
                    m_pDeviceInfo[m_curDevice].m_curMode = i;

                return m_curDevice;
            };
        };
    };
    
    //
    //  second attempt is search videomode across all devices
    //
    for (int32 i = 0; i < m_numDevices; ++i)
    {
        DEVICEINFO* pDeviceInfo = &m_pDeviceInfo[i];

        for (int32 j = 0; j < pDeviceInfo->m_numModes; ++j)
        {
            VIDEOMODE* pVideomode = &pDeviceInfo->m_pModes[j];

            if ((pVideomode->width  != vm.w)    ||
                (pVideomode->height != vm.h)    ||
                (pVideomode->depth  != vm.d))
                continue;
            
            if (!bSearchInProgress)
                pDeviceInfo->m_curMode = j;

            return i;
        };
    };

    //
    //  third attempt is the same as second except depth and refrate matching
    //
    for (int32 i = 0; i < m_numDevices; ++i)
    {
        DEVICEINFO* pDeviceInfo = &m_pDeviceInfo[i];

        for (int32 j = 0; j < pDeviceInfo->m_numModes; ++j)
        {
            VIDEOMODE* pVideomode = &pDeviceInfo->m_pModes[j];

            if ((pVideomode->width  != vm.w)    ||
                (pVideomode->height != vm.h))
                continue;

            if (!bSearchInProgress)
                pDeviceInfo->m_curMode = j;

            return i;
        };
    };
    
    return -1;
};


void CPCGraphicsDevice::SetWindowRect(int32 iWidth, int32 iHeight)
{
    HWND hWnd = CPCSpecific::m_hWnd;
    
    ASSERT(hWnd != NULL);

    if (m_bFullscreen)
    {
        SetWindowLongA(hWnd, GWL_STYLE, WS_POPUP);
        SetWindowLongA(hWnd, GWL_EXSTYLE, 0);
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, (SWP_NOSIZE |
                                                      SWP_NOMOVE |
                                                      SWP_NOACTIVATE));
    }
    else
    {
        const uint32 WndStyle = (WS_OVERLAPPEDWINDOW) ^ (WS_THICKFRAME | WS_MAXIMIZEBOX);
        const uint32 WndStyleEx = WS_EX_CLIENTEDGE;

        SetWindowLongA(hWnd, GWL_STYLE, WndStyle);
        SetWindowLongA(hWnd, GWL_EXSTYLE, WndStyleEx);
        SetWindowPos(hWnd, NULL, 0, 0, 0, 0, (SWP_NOACTIVATE |
                                              SWP_NOMOVE |
                                              SWP_NOSIZE |
                                              SWP_NOZORDER |
                                              SWP_FRAMECHANGED));

        int32 cxScreen = GetSystemMetrics(SM_CXSCREEN);
        int32 cyScreen = GetSystemMetrics(SM_CYSCREEN);

        BOOL bMenu = (GetMenu(hWnd) != NULL);

        RECT rc;
        SetRect(&rc, 0, 0, iWidth, iHeight);
        AdjustWindowRectEx(&rc, WndStyle, bMenu, WndStyleEx);

        int32 w = (rc.right - rc.left);
        int32 h = (rc.bottom - rc.top);

        if (IsFullscreenWindow())
        {
            w = iWidth  - ((rc.right - rc.left) - iWidth);
            h = iHeight - ((rc.bottom - rc.top) - iHeight);
        };

        int32 x = Clamp((cxScreen - w) / 2, 0, cxScreen - w);
        int32 y = Clamp((cyScreen - h) / 2, 0, cyScreen - h);

        SetWindowPos(hWnd, HWND_NOTOPMOST, x, y, w, h, (SWP_NOACTIVATE | SWP_FRAMECHANGED));
    };
};


bool CPCGraphicsDevice::SetVideomode(const PC::VIDEOMODE& vm)
{
    //
    //  search for new videomode
    //
    int32 curDevice = SearchAndSetVideomode(vm, true);
    if ((curDevice < 0) || (curDevice != m_curDevice))
        return false;

    //
    //  save current videomode for failure case
    //
    PC::VIDEOMODE vmbuff(ScreenWidth(), ScreenHeight(), ScreenDepth());

    //
    //  set new video mode
    //
    curDevice = SearchAndSetVideomode(vm, false);
    ASSERT(curDevice == m_curDevice);

    if (m_bFullscreen && (m_multisamplingLvl >= 2))
    {
        if (RwDrvChangeMultiSamplingLevels(1u))
        {
            CPCCrashReporter::Breadcrumb("MSAA mode_change prepare disabled_previous=%d",
                                         m_multisamplingLvl);
            m_multisamplingLvl = 0;
            m_pDeviceInfo[m_curDevice].m_numMultisamplingLvls = 0;
        }
        else
        {
            CPCCrashReporter::Breadcrumb("MSAA mode_change prepare_disable_failed previous=%d",
                                         m_multisamplingLvl);
        };
    };

    //
    //  adjust window size and recreate frame buffers
    //
    if (!m_bFullscreen)
    {
        DestroyFrameBuffer();
        RwDrvSetRefreshRate(0);
    };

    SetWindowRect(vm.w, vm.h);

    bool bVideomodeChangedFlag = false;

    if (RwDrvChangeVideoMode(Videomode()))
    {
        bVideomodeChangedFlag = true;
        bool bMultiSamplingReady = true;

        if (m_bFullscreen)
        {
            if (!ChangeMultiSamplingAfterStart("mode_change"))
                bMultiSamplingReady = false;

            uint32 refreshRate = 60;
#ifdef TMNT2_BUILD_EU
            if ((CConfigure::GetTVMode() == TYPEDEF::CONFIG_TV_PAL) && IsPalMode())
                refreshRate = 50;
#endif /* TMNT2_BUILD_EU */

            RwDrvSetRefreshRate(refreshRate);
        };

        if (bMultiSamplingReady && (m_bFullscreen || CreateFrameBuffer()))
        {
            CScreen::DeviceChanged();
            return true;
        };
    };

    //
    //  setting new videomode failed
    //  set saved old videomode, recreate framebuffers and adjust window rect... again
    //
    SearchAndSetVideomode(vmbuff, false);
    SetWindowRect(CPCSetting::m_videomode.w, CPCSetting::m_videomode.h);

    if (bVideomodeChangedFlag)
        RwDrvChangeVideoMode(Videomode());

    if (m_bFullscreen)
        ChangeMultiSamplingAfterStart("mode_change_rollback");

    if (!m_bFullscreen)
    {
        bool bResult = CreateFrameBuffer();
        ASSERT(bResult);
    };

    return false;
};


bool CPCGraphicsDevice::SetVideomode(int32 No)
{
    PC::VIDEOMODE vm;

    if (GetVideomode(No, vm))
        return SetVideomode(vm);

    return false;
};


bool CPCGraphicsDevice::GetVideomode(int32 No, PC::VIDEOMODE& vm) const
{
    ASSERT(m_pDeviceInfo);
    ASSERT(m_curDevice >= 0);
    ASSERT(m_curDevice < m_numDevices);
    ASSERT(No >= 0);
    ASSERT(No < m_pDeviceInfo[m_curDevice].m_numModes);

    if (No < 0 && No >= m_pDeviceInfo[m_curDevice].m_numModes)
        return false;

    vm.w    = m_pDeviceInfo[m_curDevice].m_pModes[No].width;
    vm.h    = m_pDeviceInfo[m_curDevice].m_pModes[No].height;
    vm.d    = m_pDeviceInfo[m_curDevice].m_pModes[No].depth;

    return true;
};


int32 CPCGraphicsDevice::GetVideomodeCur(void) const
{
    ASSERT(m_pDeviceInfo);
    ASSERT(m_curDevice >= 0);
    ASSERT(m_curDevice < m_numDevices);

    return m_pDeviceInfo[m_curDevice].m_curMode;
};


int32 CPCGraphicsDevice::GetVideomodeNum(void) const
{
    ASSERT(m_pDeviceInfo);
    ASSERT(m_curDevice >= 0);
    ASSERT(m_curDevice < m_numDevices);

    return m_pDeviceInfo[m_curDevice].m_numModes;
};


bool CPCGraphicsDevice::IsFullscreen(void) const
{
    return m_bFullscreen;
};


bool CPCGraphicsDevice::ApplyConfiguredMultiSampling(void)
{
    if (!m_bFullscreen)
    {
        m_multisamplingLvl = 0;
        m_pDeviceInfo[m_curDevice].m_numMultisamplingLvls = 0;
        CPCCrashReporter::Breadcrumb("MSAA display_menu requested=%d active=0 reason=windowed_mode",
                                     CPCSetting::m_nMSAASamples);
        return true;
    };

    const int32 nSelectedSamples =
        SelectSupportedMSAASamples(CPCSetting::m_nMSAASamples,
                                   GetCurrentModeMaxMultiSamplingLevels());
    if (nSelectedSamples == m_multisamplingLvl)
    {
        CPCCrashReporter::Breadcrumb("MSAA display_menu requested=%d active=%d result=unchanged",
                                     CPCSetting::m_nMSAASamples,
                                     m_multisamplingLvl);
        return true;
    };

    /*
     * A driver or wrapper can leave the real target non-multisampled while
     * RenderWare still remembers the requested level. Cycling through OFF
     * guarantees that the following call performs a device reset instead of
     * returning early because its internal selection already matches.
     */
    if (nSelectedSamples >= 2)
        RwDrvChangeMultiSamplingLevels(1u);

    if (!ChangeMultiSamplingAfterStart("display_menu"))
        return false;

    CScreen::DeviceChanged();
    return true;
};


int32 CPCGraphicsDevice::GetCurrentModeMaxMultiSamplingLevels(void) const
{
    ASSERT(m_pDeviceInfo);
    ASSERT(m_curDevice >= 0);
    ASSERT(m_curDevice < m_numDevices);

    const DEVICEINFO* pDeviceInfo = &m_pDeviceInfo[m_curDevice];
    ASSERT(pDeviceInfo->m_curMode >= 0);
    ASSERT(pDeviceInfo->m_curMode < pDeviceInfo->m_numModes);

    return pDeviceInfo->m_pModes[pDeviceInfo->m_curMode].m_maxMultiSamplingLevels;
};


void CPCGraphicsDevice::SetMultiSamplingBeforeStart(void)
{
    m_multisamplingLvl = 0;
    m_pDeviceInfo[m_curDevice].m_numMultisamplingLvls = 0;

    if (!m_bFullscreen)
    {
        CPCCrashReporter::Breadcrumb("MSAA initialize requested=%d active=0 reason=windowed_mode",
                                     CPCSetting::m_nMSAASamples);
        return;
    };

    const int32 nMaxSamples = GetCurrentModeMaxMultiSamplingLevels();
    m_multisamplingLvl = SelectSupportedMSAASamples(CPCSetting::m_nMSAASamples, nMaxSamples);
    m_pDeviceInfo[m_curDevice].m_numMultisamplingLvls = m_multisamplingLvl;

    RwDrvSetMultiSamplingLevels(GetRenderWareMultiSamplingLevels(m_multisamplingLvl));

    const VIDEOMODE* pVideomode =
        &m_pDeviceInfo[m_curDevice].m_pModes[m_pDeviceInfo[m_curDevice].m_curMode];
    CPCCrashReporter::Breadcrumb(
        "MSAA initialize requested=%d max=%d active=%d rw_levels=%u mode=%dx%dx%d",
        CPCSetting::m_nMSAASamples,
        nMaxSamples,
        m_multisamplingLvl,
        GetRenderWareMultiSamplingLevels(m_multisamplingLvl),
        pVideomode->width,
        pVideomode->height,
        pVideomode->depth);
};


bool CPCGraphicsDevice::ChangeMultiSamplingAfterStart(const char* pszPhase)
{
    ASSERT(m_bFullscreen);

    const int32 nMaxSamples = GetCurrentModeMaxMultiSamplingLevels();
    const int32 nSelectedSamples =
        SelectSupportedMSAASamples(CPCSetting::m_nMSAASamples, nMaxSamples);
    const uint32 nRenderWareLevels = GetRenderWareMultiSamplingLevels(nSelectedSamples);

    if (RwDrvChangeMultiSamplingLevels(nRenderWareLevels))
    {
        const int32 nActualSamples =
            TraceActualMultiSampling(pszPhase, nSelectedSamples);

        m_multisamplingLvl =
            (nActualSamples >= 0 ? nActualSamples : nSelectedSamples);
        m_pDeviceInfo[m_curDevice].m_numMultisamplingLvls = m_multisamplingLvl;
        CPCCrashReporter::Breadcrumb(
            "MSAA %s requested=%d max=%d active=%d rw_levels=%u result=%s",
            pszPhase,
            CPCSetting::m_nMSAASamples,
            nMaxSamples,
            m_multisamplingLvl,
            nRenderWareLevels,
            ((nActualSamples < 0) || (nActualSamples == nSelectedSamples)) ?
                "success" : "surface_mismatch");
        return true;
    };

    if ((nSelectedSamples >= 2) && RwDrvChangeMultiSamplingLevels(1u))
    {
        m_multisamplingLvl = 0;
        m_pDeviceInfo[m_curDevice].m_numMultisamplingLvls = 0;
        CPCCrashReporter::Breadcrumb(
            "MSAA %s requested=%d max=%d active=0 result=fallback_disabled",
            pszPhase,
            CPCSetting::m_nMSAASamples,
            nMaxSamples);
        return true;
    };

    m_multisamplingLvl = 0;
    m_pDeviceInfo[m_curDevice].m_numMultisamplingLvls = 0;
    CPCCrashReporter::Breadcrumb(
        "MSAA %s requested=%d max=%d active=0 result=change_failed",
        pszPhase,
        CPCSetting::m_nMSAASamples,
        nMaxSamples);
    return false;
};


int32 CPCGraphicsDevice::TraceActualMultiSampling(const char* pszPhase,
                                                  int32 nExpectedSamples)
{
#if defined(TMNT2_RWDRV_D3D9)
    IDirect3DDevice9* pDevice =
        static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
    if (!pDevice)
    {
        CPCCrashReporter::Breadcrumb(
            "MSAA surface phase=%s expected=%d result=no_d3d9_device",
            pszPhase,
            nExpectedSamples);
        return -1;
    };

    DWORD dwStateBefore = 0;
    DWORD dwStateAfter = 0;
    HRESULT hrStateBefore =
        pDevice->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS, &dwStateBefore);

    /* Use RenderWare's setter so its state cache and the D3D9 device agree. */
    RwD3D9SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,
                         (nExpectedSamples >= 2 ? TRUE : FALSE));

    HRESULT hrStateAfter =
        pDevice->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS, &dwStateAfter);

    IDirect3DSurface9* pRenderTarget = nullptr;
    IDirect3DSurface9* pBackBuffer = nullptr;
    IDirect3DSurface9* pDepthBuffer = nullptr;
    D3DSURFACE_DESC renderTargetDesc = {};
    D3DSURFACE_DESC backBufferDesc = {};
    D3DSURFACE_DESC depthBufferDesc = {};

    HRESULT hrRenderTarget = pDevice->GetRenderTarget(0, &pRenderTarget);
    if (SUCCEEDED(hrRenderTarget) && pRenderTarget)
        hrRenderTarget = pRenderTarget->GetDesc(&renderTargetDesc);

    HRESULT hrBackBuffer =
        pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    if (SUCCEEDED(hrBackBuffer) && pBackBuffer)
        hrBackBuffer = pBackBuffer->GetDesc(&backBufferDesc);

    HRESULT hrDepthBuffer = pDevice->GetDepthStencilSurface(&pDepthBuffer);
    if (SUCCEEDED(hrDepthBuffer) && pDepthBuffer)
        hrDepthBuffer = pDepthBuffer->GetDesc(&depthBufferDesc);

    const D3DMULTISAMPLE_TYPE multisampleType =
        (SUCCEEDED(hrRenderTarget) ? renderTargetDesc.MultiSampleType :
         (SUCCEEDED(hrBackBuffer) ? backBufferDesc.MultiSampleType :
          D3DMULTISAMPLE_NONE));

    int32 nActualSamples = -1;
    if (SUCCEEDED(hrRenderTarget) || SUCCEEDED(hrBackBuffer))
    {
        if (multisampleType == D3DMULTISAMPLE_NONE)
            nActualSamples = 0;
        else if (multisampleType == D3DMULTISAMPLE_NONMASKABLE)
            nActualSamples = nExpectedSamples;
        else
            nActualSamples = static_cast<int32>(multisampleType);
    };

    CPCCrashReporter::Breadcrumb(
        "MSAA surface phase=%s expected=%d actual=%d rt_hr=0x%08lX rt_type=%u rt_quality=%lu rt=%ux%u bb_hr=0x%08lX bb_type=%u bb_quality=%lu depth_hr=0x%08lX depth_type=%u depth_quality=%lu state_before_hr=0x%08lX state_before=%lu state_after_hr=0x%08lX state_after=%lu",
        pszPhase,
        nExpectedSamples,
        nActualSamples,
        static_cast<unsigned long>(hrRenderTarget),
        static_cast<unsigned int>(renderTargetDesc.MultiSampleType),
        static_cast<unsigned long>(renderTargetDesc.MultiSampleQuality),
        static_cast<unsigned int>(renderTargetDesc.Width),
        static_cast<unsigned int>(renderTargetDesc.Height),
        static_cast<unsigned long>(hrBackBuffer),
        static_cast<unsigned int>(backBufferDesc.MultiSampleType),
        static_cast<unsigned long>(backBufferDesc.MultiSampleQuality),
        static_cast<unsigned long>(hrDepthBuffer),
        static_cast<unsigned int>(depthBufferDesc.MultiSampleType),
        static_cast<unsigned long>(depthBufferDesc.MultiSampleQuality),
        static_cast<unsigned long>(hrStateBefore),
        static_cast<unsigned long>(dwStateBefore),
        static_cast<unsigned long>(hrStateAfter),
        static_cast<unsigned long>(dwStateAfter));

    if (pDepthBuffer)
        pDepthBuffer->Release();
    if (pBackBuffer)
        pBackBuffer->Release();
    if (pRenderTarget)
        pRenderTarget->Release();

    return nActualSamples;
#else /* defined(TMNT2_RWDRV_D3D9) */
    CPCCrashReporter::Breadcrumb(
        "MSAA surface phase=%s expected=%d result=non_d3d9_renderer",
        pszPhase,
        nExpectedSamples);
    return 0;
#endif /* defined(TMNT2_RWDRV_D3D9) */
};


void CPCGraphicsDevice::OutputInfo(void)
{
    for (int32 i = 0; i < m_numDevices; ++i)
    {
        DEVICEINFO* pDeviceInfo = &m_pDeviceInfo[i];
        
        OUTPUT(
            "Device: %s, number of videomodes: %d, window mode support: %s\n",
            pDeviceInfo->name,
            pDeviceInfo->m_numModes,
            pDeviceInfo->m_bModeWndExist ? "true" : "false"
        );

        for (int32 j = 0; j < pDeviceInfo->m_numModes; ++j)
        {
            VIDEOMODE* pVideomode = &pDeviceInfo->m_pModes[j];

            OUTPUT(
                "\t%d) Videomode: %4d x %4d x %4d - %d (FLAGS: 0x%x, FORMAT: 0x%x)\n",
                j + 1,
                pVideomode->width,
                pVideomode->height,
                pVideomode->depth,
                pVideomode->refRate,
                pVideomode->flags,
                pVideomode->format
            );
        };
    };    
};


bool CPCGraphicsDevice::IsFullscreenWindow(void) const
{
    if (m_bFullscreen)
        return false; // not a windowed mode

    const DEVICEINFO* pDeviceInfo = &m_pDeviceInfo[m_curDevice];

    if (!pDeviceInfo->m_bModeWndExist)
        return false; // window mode not exist

    RwVideoMode vmFullscreen;
    RwEngineGetVideoModeInfo(&vmFullscreen, pDeviceInfo->m_idxModeWnd);

    RwVideoMode vmCurrent;
    RwEngineGetVideoModeInfo(&vmCurrent, pDeviceInfo->m_pModes[pDeviceInfo->m_curMode].m_index);

    if ((vmFullscreen.width == vmCurrent.width) &&
        (vmFullscreen.height == vmCurrent.height))
        return true; // current mode is windowed and equals to fullscreen

    return false;
};
