#include "PCDebugImGui.hpp"

#if defined(TMNT2_IMGUI)

#include <Windows.h>
#include <d3d9.h>

#include "imgui.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"

#include "PCSpecific.hpp"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


namespace
{
    enum STATE
    {
        STATE_NONE = 0,     // not tried yet
        STATE_READY,
        STATE_FAILED,       // never retried
    };

    STATE s_state = STATE_NONE;
    bool s_bInFrame = false;
    bool s_bShownLastFrame = false;
    bool s_bDeviceLost = false;
    float s_fFontScale = 1.0f;
    IDirect3DDevice9* s_pDevice = nullptr;
    rwD3D9DeviceReleaseCallBack s_pfnPrevRelease = nullptr;
    rwD3D9DeviceRestoreCallBack s_pfnPrevRestore = nullptr;


    void OnDeviceRelease(void)
    {
        if (s_pfnPrevRelease)
            s_pfnPrevRelease();

        if (s_state == STATE_READY)
            ImGui_ImplDX9_InvalidateDeviceObjects();
        s_bDeviceLost = true;
    };


    void OnDeviceRestore(void)
    {
        if (s_pfnPrevRestore)
            s_pfnPrevRestore();

        // Device objects are recreated lazily by ImGui_ImplDX9_NewFrame.
        s_bDeviceLost = false;
    };


    float FontPixelSize(void)
    {
        RECT rect = {};
        if (CPCSpecific::m_hWnd && GetClientRect(CPCSpecific::m_hWnd, &rect))
        {
            float height = static_cast<float>(rect.bottom - rect.top);
            float size = height / 46.0f;
            if (size < 15.0f) size = 15.0f;
            if (size > 30.0f) size = 30.0f;
            return size;
        };
        return 18.0f;
    };


    void ApplyStyle(void)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark(&style);

        style.WindowRounding = 6.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 4.0f;
        style.ScrollbarRounding = 6.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.WindowPadding = ImVec2(12.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 5.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.SeparatorTextBorderSize = 2.0f;
        style.SeparatorTextPadding = ImVec2(10.0f, 4.0f);

        ImVec4* c = style.Colors;
        const ImVec4 accent      = ImVec4(0.23f, 0.61f, 0.85f, 1.00f);
        const ImVec4 accentHover = ImVec4(0.32f, 0.70f, 0.93f, 1.00f);
        const ImVec4 orange      = ImVec4(1.00f, 0.66f, 0.16f, 1.00f);

        c[ImGuiCol_WindowBg]            = ImVec4(0.04f, 0.055f, 0.08f, 0.94f);
        c[ImGuiCol_ChildBg]             = ImVec4(0.06f, 0.08f, 0.11f, 0.80f);
        c[ImGuiCol_PopupBg]             = ImVec4(0.05f, 0.07f, 0.10f, 0.97f);
        c[ImGuiCol_Border]              = ImVec4(0.23f, 0.61f, 0.85f, 0.55f);
        c[ImGuiCol_TitleBg]             = ImVec4(0.08f, 0.23f, 0.36f, 1.00f);
        c[ImGuiCol_TitleBgActive]       = ImVec4(0.08f, 0.23f, 0.36f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]    = ImVec4(0.06f, 0.15f, 0.24f, 0.90f);
        c[ImGuiCol_FrameBg]             = ImVec4(0.11f, 0.15f, 0.20f, 1.00f);
        c[ImGuiCol_FrameBgHovered]      = ImVec4(0.15f, 0.22f, 0.30f, 1.00f);
        c[ImGuiCol_FrameBgActive]       = ImVec4(0.18f, 0.29f, 0.41f, 1.00f);
        c[ImGuiCol_Tab]                 = ImVec4(0.11f, 0.15f, 0.20f, 1.00f);
        c[ImGuiCol_TabHovered]          = accentHover;
        c[ImGuiCol_TabSelected]         = ImVec4(0.12f, 0.29f, 0.45f, 1.00f);
        c[ImGuiCol_TabSelectedOverline] = orange;
        c[ImGuiCol_Header]              = ImVec4(0.12f, 0.29f, 0.45f, 0.85f);
        c[ImGuiCol_HeaderHovered]       = ImVec4(0.16f, 0.36f, 0.55f, 0.90f);
        c[ImGuiCol_HeaderActive]        = accent;
        c[ImGuiCol_Button]              = ImVec4(0.14f, 0.24f, 0.34f, 1.00f);
        c[ImGuiCol_ButtonHovered]       = ImVec4(0.20f, 0.36f, 0.52f, 1.00f);
        c[ImGuiCol_ButtonActive]        = accent;
        c[ImGuiCol_SliderGrab]          = accent;
        c[ImGuiCol_SliderGrabActive]    = orange;
        c[ImGuiCol_CheckMark]           = orange;
        c[ImGuiCol_Separator]           = ImVec4(0.23f, 0.61f, 0.85f, 0.40f);
        c[ImGuiCol_Text]                = ImVec4(0.91f, 0.93f, 0.97f, 1.00f);
        c[ImGuiCol_TextDisabled]        = ImVec4(0.55f, 0.60f, 0.68f, 1.00f);
        c[ImGuiCol_PlotHistogram]       = accent;
    };


    bool Initialize(void)
    {
        if (!CPCSpecific::m_hWnd)
            return false;

        IDirect3DDevice9* pDevice = static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
        if (!pDevice)
            return false;

        // ImGui's DX9 backend saves and restores device state with a state
        // block; a pure device cannot capture one, so use the game-font menu.
        D3DDEVICE_CREATION_PARAMETERS params = {};
        if (SUCCEEDED(pDevice->GetCreationParameters(&params)) &&
            (params.BehaviorFlags & D3DCREATE_PUREDEVICE))
            return false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = "TMNT2_debugtools_imgui.ini";
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        ApplyStyle();

        // Segoe UI when present, ImGui's built-in font otherwise.
        float size = FontPixelSize();
        char fontPath[MAX_PATH] = {};
        UINT len = GetWindowsDirectoryA(fontPath, MAX_PATH);
        bool bLoaded = false;
        if ((len > 0) && (len < (MAX_PATH - 32)))
        {
            lstrcatA(fontPath, "\\Fonts\\segoeui.ttf");
            if (GetFileAttributesA(fontPath) != INVALID_FILE_ATTRIBUTES)
                bLoaded = (io.Fonts->AddFontFromFileTTF(fontPath, size) != nullptr);
        };

        if (!bLoaded)
        {
            ImFontConfig config;
            config.SizePixels = size;
            io.Fonts->AddFontDefault(&config);
        };

        if (!ImGui_ImplWin32_Init(CPCSpecific::m_hWnd))
        {
            ImGui::DestroyContext();
            return false;
        };

        if (!ImGui_ImplDX9_Init(pDevice))
        {
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            return false;
        };

        s_pDevice = pDevice;
        s_pfnPrevRelease = _rwD3D9DeviceGetReleaseCallback();
        s_pfnPrevRestore = _rwD3D9DeviceGetRestoreCallback();
        _rwD3D9DeviceSetReleaseCallback(OnDeviceRelease);
        _rwD3D9DeviceSetRestoreCallback(OnDeviceRestore);
        return true;
    };
};


/*static*/ bool CPCDebugImGui::WndProc(void* pWnd, unsigned int uMsg, std::uintptr_t wParamRaw, std::intptr_t lParamRaw)
{
    if (s_state != STATE_READY)
        return false;

    HWND hWnd = static_cast<HWND>(pWnd);
    WPARAM wParam = static_cast<WPARAM>(wParamRaw);
    LPARAM lParam = static_cast<LPARAM>(lParamRaw);

    // Always keep ImGui's key/mouse state current, but only swallow input
    // while a menu frame is shown and the pointer is over it.
    LRESULT result = ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
    if (result)
        return true;

    if (!s_bShownLastFrame)
        return false;

    switch (uMsg)
    {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
        return ImGui::GetIO().WantCaptureMouse;

    default:
        return false;
    };
};


/*static*/ bool CPCDebugImGui::BeginFrame(float fontScale)
{
    if (s_bInFrame)
        return true;

    if (s_state == STATE_NONE)
        s_state = (Initialize() ? STATE_READY : STATE_FAILED);

    if ((s_state != STATE_READY) || s_bDeviceLost)
        return false;

    // The device can be recreated (resolution change): rebind if it changed.
    IDirect3DDevice9* pDevice = static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
    if (!pDevice)
        return false;

    if (pDevice != s_pDevice)
    {
        ImGui_ImplDX9_Shutdown();
        if (!ImGui_ImplDX9_Init(pDevice))
        {
            s_state = STATE_FAILED;
            return false;
        };
        s_pDevice = pDevice;
    };

    s_fFontScale = fontScale;
    ImGui::GetIO().FontGlobalScale = fontScale;
    ImGui::GetIO().MouseDrawCursor = true;   // the game hides the OS cursor

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    s_bInFrame = true;
    return true;
};


/*static*/ void CPCDebugImGui::EndFrame(void)
{
    if (!s_bInFrame)
        return;

    s_bInFrame = false;
    ImGui::Render();
    if (!s_bDeviceLost)
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    s_bShownLastFrame = true;
};


/*static*/ void CPCDebugImGui::MarkHidden(void)
{
    s_bShownLastFrame = false;
};


/*static*/ bool CPCDebugImGui::IsAvailable(void)
{
    return (s_state != STATE_FAILED);
};


/*static*/ bool CPCDebugImGui::IsActive(void)
{
    return s_bShownLastFrame;
};


/*static*/ bool CPCDebugImGui::WantsMouse(void)
{
    return ((s_state == STATE_READY) && s_bShownLastFrame && ImGui::GetIO().WantCaptureMouse);
};


/*static*/ void CPCDebugImGui::Shutdown(void)
{
    if (s_state != STATE_READY)
        return;

    _rwD3D9DeviceSetReleaseCallback(s_pfnPrevRelease);
    _rwD3D9DeviceSetRestoreCallback(s_pfnPrevRestore);
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_state = STATE_FAILED;
    s_pDevice = nullptr;
};

#endif /* defined(TMNT2_IMGUI) */
