#pragma once

// Dear ImGui bridge for the Debug Tools (Win32 + Direct3D 9 through RenderWare).
// Only compiled into Debug Tools builds (TMNT2_IMGUI). The game itself never
// calls ImGui; the Debug Tools process opens one frame per draw while its menu
// is shown in ImGui style.

#if defined(TMNT2_IMGUI)

#include <cstdint>


class CPCDebugImGui
{
public:
    // Called first in the game's window procedure. Returns true when ImGui
    // consumed the message (only while a frame is being shown).
    // (Windows types are passed untyped so game code can include this header
    // without <Windows.h>.)
    static bool WndProc(void* hWnd, unsigned int uMsg, std::uintptr_t wParam, std::intptr_t lParam);

    // Lazily creates the context and backends on the current D3D9 device.
    // Returns false when ImGui cannot be used (no device, pure device, init
    // failure); callers fall back to the game-font menu.
    static bool BeginFrame(float fontScale);

    // Renders the frame built since BeginFrame. Must run inside the RW camera
    // update (inside the D3D9 BeginScene/EndScene pair).
    static void EndFrame(void);

    // Called on frames where no ImGui frame is shown (menu closed or another style).
    static void MarkHidden(void);

    static bool IsAvailable(void);
    static bool IsActive(void);     // a frame was shown during the last game frame
    static bool WantsMouse(void);   // pointer is over an ImGui window
    static void Shutdown(void);
};

#endif /* defined(TMNT2_IMGUI) */
