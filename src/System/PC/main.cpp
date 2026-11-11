#include "PCTypedefs.hpp"
#include "PCDebug.hpp"
#include "PCSpecific.hpp"
#include "PCSetting.hpp"
#include "PCFramework.hpp"
#include "PCCrashReporter.hpp"
#include "PCModFeatures.hpp"

#include "System/Common/Configure.hpp"


// Hybrid-graphics laptops (NVIDIA Optimus, AMD PowerXpress / Switchable Graphics)
// run unlisted 32-bit D3D9 games on the integrated GPU. These exported symbols
// ask both drivers to use the dedicated GPU for this process.
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
};


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
    CPCCrashReporter::Breadcrumb("BUILD TMNT2_Core_Karai_Boss_Variant_v1_4_1_US");
    CPCModFeatures::Initialize();

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
