#include "PCTypedefs.hpp"
#include "PCDebug.hpp"
#include "PCSpecific.hpp"
#include "PCSetting.hpp"
#include "PCFramework.hpp"
#include "PCCrashReporter.hpp"

#include "System/Common/Configure.hpp"


//#define VLDCHECK

#ifdef VLDCHECK
#include "vld.h"
#endif /* VLDCHECK */


int32 APIENTRY
_tWinMain(
    _In_     HINSTANCE	hInstance,
    _In_opt_ HINSTANCE	hPrevInstance,
    _In_     LPTSTR		lpCmdLine,
    _In_     int32		iCmdShow
)
{
    CPCCrashReporter::Install();
    CPCCrashReporter::Breadcrumb("BUILD TMNT2_Slashuur_Adaptive_Teleport_v2_Radius25_NoFlying_US");

#ifdef VLDCHECK    
    VLDEnable();
#endif /* VLDCHECK */

#if defined(NDEBUG)
    CDebug::Fatal = CPCDebug::Fatal;
#elif defined(_DEBUG)
    CPCDebug::Initialize();
#endif    

    CConfigure::SetLaunchMode(TYPEDEF::CONFIG_LAUNCH_NORMAL);
    CConfigure::InitArgs(__argc, __argv);

    CPCSetting::Initialize();
    CPCSpecific::m_hInstance = hInstance;
    
    bool bResult = CPCFramework::StartAndRun();

    CPCSetting::Terminate();

#if defined(NDEBUG)
    CDebug::Fatal = nullptr;
#elif defined(_DEBUG)
    CPCDebug::Terminate();
#endif

    CPCCrashReporter::Uninstall();
    return (bResult ? EXIT_SUCCESS : EXIT_FAILURE);
};
