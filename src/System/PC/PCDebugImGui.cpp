#include "PCDebugImGui.hpp"

#if defined(TMNT2_IMGUI)

#include <Windows.h>
#include <d3d9.h>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"

#include "PCSpecific.hpp"
#include "PCCrashReporter.hpp"

#include "System/Common/Camera.hpp"
#include "System/Common/RenderState.hpp"
#include "System/Common/System2D.hpp"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


// Embedded UI font: Inter Medium (SIL OFL 1.1), Latin subset - vendor/fonts/inter.
#include "inter/InterMedium_compressed.inl"


//
//  Rendering goes through RenderWare's Im2D instead of a raw Direct3D 9
//  backend: RW owns the device (it may be a pure device, which cannot save
//  and restore state blocks) and keeps a render-state cache, so ImGui draws
//  the same way the game's own 2D does. Only the scissor rectangle is set on
//  the device directly (RwD3D9SetRenderState keeps RW's cache in step).
//


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
    const char* s_pszFailReason = "";
    RwRaster* s_pFontRaster = nullptr;

    enum { VERTEX_MAX = 65535 };
    RwIm2DVertex* s_pVertex = nullptr;


    void Fail(const char* pszReason)
    {
        s_pszFailReason = pszReason;
        CPCCrashReporter::Breadcrumb(pszReason);
    };


    float FontPixelSize(void)
    {
        RECT rect = {};
        if (CPCSpecific::m_hWnd && GetClientRect(CPCSpecific::m_hWnd, &rect))
        {
            float height = static_cast<float>(rect.bottom - rect.top);
            float size = height / 44.0f;
            if (size < 16.0f) size = 16.0f;
            if (size > 32.0f) size = 32.0f;
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
        style.AntiAliasedFill = true;
        style.AntiAliasedLines = true;

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
        c[ImGuiCol_Text]                = ImVec4(0.92f, 0.94f, 0.97f, 1.00f);
        c[ImGuiCol_TextDisabled]        = ImVec4(0.58f, 0.63f, 0.70f, 1.00f);
        c[ImGuiCol_PlotHistogram]       = accent;
    };


    // Font atlas -> RW texture raster (A8R8G8B8, stored B,G,R,A in memory).
    bool CreateFontRaster(void)
    {
        ImGuiIO& io = ImGui::GetIO();
        unsigned char* pPixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pPixels, &width, &height);
        if (!pPixels || (width <= 0) || (height <= 0))
            return false;

        RwRaster* pRaster = RwRasterCreate(width, height, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        if (!pRaster)
            return false;

        RwUInt8* pDst = RwRasterLock(pRaster, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
        if (!pDst)
        {
            RwRasterDestroy(pRaster);
            return false;
        };

        int32 stride = RwRasterGetStride(pRaster);
        for (int y = 0; y < height; ++y)
        {
            const unsigned char* pSrcRow = &pPixels[y * width * 4];
            RwUInt8* pDstRow = &pDst[y * stride];
            for (int x = 0; x < width; ++x)
            {
                pDstRow[(x * 4) + 0] = pSrcRow[(x * 4) + 2];
                pDstRow[(x * 4) + 1] = pSrcRow[(x * 4) + 1];
                pDstRow[(x * 4) + 2] = pSrcRow[(x * 4) + 0];
                pDstRow[(x * 4) + 3] = pSrcRow[(x * 4) + 3];
            };
        };
        RwRasterUnlock(pRaster);

        s_pFontRaster = pRaster;
        io.Fonts->SetTexID(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(pRaster)));
        io.Fonts->ClearTexData();
        return true;
    };


    void BuildFonts(float size)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 1;
        config.PixelSnapH = false;
        io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, size, &config);
    };


    bool Initialize(void)
    {
        if (!CPCSpecific::m_hWnd)
        {
            Fail("IMGUI init failed: no game window");
            return false;
        };

        s_pVertex = new RwIm2DVertex[VERTEX_MAX];

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = "TMNT2_debugtools_imgui.ini";
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.BackendRendererName = "tmnt2_rw_im2d";

        ApplyStyle();
        BuildFonts(FontPixelSize());

        if (!ImGui_ImplWin32_Init(CPCSpecific::m_hWnd))
        {
            ImGui::DestroyContext();
            Fail("IMGUI init failed: Win32 input backend");
            return false;
        };

        if (!CreateFontRaster())
        {
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            Fail("IMGUI init failed: font texture");
            return false;
        };

        CPCCrashReporter::Breadcrumb("IMGUI ready (RW Im2D renderer, Inter font)");
        return true;
    };


    void SetScissor(IDirect3DDevice9* pDevice, float x0, float y0, float x1, float y1)
    {
        if (!pDevice)
            return;

        RECT rect;
        rect.left = static_cast<LONG>(x0);
        rect.top = static_cast<LONG>(y0);
        rect.right = static_cast<LONG>(x1);
        rect.bottom = static_cast<LONG>(y1);
        pDevice->SetScissorRect(&rect);
    };


    void RenderDrawData(ImDrawData* pDrawData)
    {
        RwCamera* pCamera = CCamera::CameraCurrent();
        if (!pCamera || !pDrawData || (pDrawData->DisplaySize.x <= 0.0f) || (pDrawData->DisplaySize.y <= 0.0f))
            return;

        // ImGui works in window client pixels; RW draws in raster pixels.
        RwRaster* pTarget = RwCameraGetRaster(pCamera);
        float targetW = (pTarget ? static_cast<float>(RwRasterGetWidth(pTarget)) : pDrawData->DisplaySize.x);
        float targetH = (pTarget ? static_cast<float>(RwRasterGetHeight(pTarget)) : pDrawData->DisplaySize.y);
        float sx = targetW / pDrawData->DisplaySize.x;
        float sy = targetH / pDrawData->DisplaySize.y;
        float z = RwIm2DGetNearScreenZ();
        float rhw = 1.0f / RwCameraGetNearClipPlane(pCamera);

        IDirect3DDevice9* pDevice = static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());

        CSystem2D::PushRenderState();
        RENDERSTATE_PUSH(rwRENDERSTATETEXTUREFILTER, rwFILTERLINEAR);
        RENDERSTATE_PUSH(rwRENDERSTATETEXTUREADDRESS, rwTEXTUREADDRESSCLAMP);
        RENDERSTATE_PUSH(rwRENDERSTATETEXTURERASTER, 0);
        if (pDevice)
            RwD3D9SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

        for (int n = 0; n < pDrawData->CmdListsCount; ++n)
        {
            const ImDrawList* pList = pDrawData->CmdLists[n];
            int vertexCount = pList->VtxBuffer.Size;
            if ((vertexCount <= 0) || (vertexCount > VERTEX_MAX))
                continue;

            for (int v = 0; v < vertexCount; ++v)
            {
                const ImDrawVert& rSrc = pList->VtxBuffer[v];
                RwIm2DVertex* pDst = &s_pVertex[v];
                RwIm2DVertexSetScreenX(pDst, (rSrc.pos.x - pDrawData->DisplayPos.x) * sx);
                RwIm2DVertexSetScreenY(pDst, (rSrc.pos.y - pDrawData->DisplayPos.y) * sy);
                RwIm2DVertexSetScreenZ(pDst, z);
                RwIm2DVertexSetRecipCameraZ(pDst, rhw);
                RwIm2DVertexSetU(pDst, rSrc.uv.x, rhw);
                RwIm2DVertexSetV(pDst, rSrc.uv.y, rhw);
                RwIm2DVertexSetIntRGBA(pDst,
                                       (rSrc.col >> IM_COL32_R_SHIFT) & 0xFF,
                                       (rSrc.col >> IM_COL32_G_SHIFT) & 0xFF,
                                       (rSrc.col >> IM_COL32_B_SHIFT) & 0xFF,
                                       (rSrc.col >> IM_COL32_A_SHIFT) & 0xFF);
            };

            for (int c = 0; c < pList->CmdBuffer.Size; ++c)
            {
                const ImDrawCmd& rCmd = pList->CmdBuffer[c];
                if (rCmd.UserCallback || (rCmd.ElemCount == 0))
                    continue;

                float x0 = (rCmd.ClipRect.x - pDrawData->DisplayPos.x) * sx;
                float y0 = (rCmd.ClipRect.y - pDrawData->DisplayPos.y) * sy;
                float x1 = (rCmd.ClipRect.z - pDrawData->DisplayPos.x) * sx;
                float y1 = (rCmd.ClipRect.w - pDrawData->DisplayPos.y) * sy;
                if ((x1 <= x0) || (y1 <= y0))
                    continue;
                SetScissor(pDevice, x0, y0, x1, y1);

                RwRaster* pRaster = reinterpret_cast<RwRaster*>(static_cast<intptr_t>(rCmd.GetTexID()));
                RwRenderStateSet(rwRENDERSTATETEXTURERASTER, pRaster);

                // ImDrawIdx and RwImVertexIndex are both 16-bit
                RwIm2DRenderIndexedPrimitive(
                    rwPRIMTYPETRILIST,
                    s_pVertex,
                    vertexCount,
                    reinterpret_cast<RwImVertexIndex*>(const_cast<ImDrawIdx*>(&pList->IdxBuffer[rCmd.IdxOffset])),
                    static_cast<RwInt32>(rCmd.ElemCount)
                );
            };
        };

        if (pDevice)
            RwD3D9SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        RENDERSTATE_POP(rwRENDERSTATETEXTURERASTER);
        RENDERSTATE_POP(rwRENDERSTATETEXTUREADDRESS);
        RENDERSTATE_POP(rwRENDERSTATETEXTUREFILTER);
        CSystem2D::PopRenderState();
    };
};


static_assert(sizeof(ImDrawIdx) == sizeof(RwImVertexIndex), "ImGui indices must match RW Im2D indices");


/*static*/ bool CPCDebugImGui::WndProc(void* pWnd, unsigned int uMsg, std::uintptr_t wParamRaw, std::intptr_t lParamRaw)
{
    if (s_state != STATE_READY)
        return false;

    HWND hWnd = static_cast<HWND>(pWnd);
    WPARAM wParam = static_cast<WPARAM>(wParamRaw);
    LPARAM lParam = static_cast<LPARAM>(lParamRaw);

    // Keep ImGui's key/mouse state current; only swallow mouse buttons while
    // a menu frame is shown and the pointer is over an ImGui window.
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
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

    if (s_state != STATE_READY)
        return false;

    if (!CCamera::CameraCurrent())
        return false;

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = fontScale;
    io.MouseDrawCursor = true;   // the game hides the OS cursor

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
    RenderDrawData(ImGui::GetDrawData());
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


/*static*/ const char* CPCDebugImGui::GetFailReason(void)
{
    return s_pszFailReason;
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

    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (s_pFontRaster)
    {
        RwRasterDestroy(s_pFontRaster);
        s_pFontRaster = nullptr;
    };
    delete[] s_pVertex;
    s_pVertex = nullptr;
    s_state = STATE_FAILED;
};

#endif /* defined(TMNT2_IMGUI) */
