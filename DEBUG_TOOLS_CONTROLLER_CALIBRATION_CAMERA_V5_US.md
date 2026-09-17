# TMNT2 Debug Tools Controller Calibration Camera v5 US

Target: TMNT 2: Battle Nexus, US/NA 32-bit PC.

This is an EXE-only, composable T2 Mod Manager feature. It does not contain,
rebuild, or replace `TMNT.DAT` or any LPAC entry. No additional attachment is
required. The release build keeps the retail startup flow and does not boot into
the recovered developer menu.

## Access and final controller contract

- `F4` or controller `Back/View`: open or close Debug Tools during gameplay.
- Controller `Start`: use the normal in-game pause menu, equivalent to Escape.
- D-pad or left stick: select an item and adjust its value.
- Physical `LB` / `LT`: previous or next Debug Tools page.
- Physical `RB` / `RT`: increase or decrease showcase-camera distance while
  Debug Tools is open.
- Cross / Xbox `A`: execute or confirm the selected item.
- Circle / Xbox `B`: back/cancel. During calibration it cancels the calibration
  step; otherwise it closes Debug Tools.
- Square / Xbox `X`: reset only the selected item.
- Triangle / Xbox `Y`: reset the current page.
- Right stick: orbit the showcase camera even when Debug Tools is closed.
- Right-stick press / `R3`: return to the normal automatic game camera.

Keyboard controls remain `Q` / `E` for pages, arrows for navigation and
adjustment, Page Up / Page Down for five-step adjustment, Enter for execute,
Backspace for selected-item reset, Delete for page reset, and Escape for back.

The physical shoulder translation is deliberately local to Debug Tools. The
retail PC DirectInput table reports physical LB, RB, LT, and RT through internal
L1, L2, R1, and R2 identities respectively. Normal gameplay controls are not
remapped.

## Input page and controller calibration

The Input page displays the raw DirectInput `X`, `Y`, `Z`, `Rx`, `Ry`,
`Rz`, `Slider 0`, and `Slider 1` values. This makes axis behavior observable
instead of assuming that every controller exposes its right stick identically.

Calibration procedure:

1. Select **Right-stick calibration** and press A or Enter.
2. Release both sticks and press A or Enter to capture the center.
3. Hold the right stick fully right and press A or Enter.
4. Hold the right stick fully up and press A or Enter.

Debug Tools detects the dominant horizontal and vertical axes and their signs.
The profile is keyed by the controller product GUID and saved at runtime to:

`TMNT2-DebugTools\ControllerProfiles.ini`

The folder is created beside the running game EXE. X / Backspace resets the
current controller profile, Y / Delete resets the Input page, and B / Esc
cancels an in-progress calibration. If no calibrated profile exists, the retail
right-stick mapping remains the fallback until calibration is completed.

## Spherical showcase camera

The showcase camera now tracks three independent values: yaw, pitch, and
radius. Right-stick right/left changes yaw; up/down changes pitch; RB/RT changes
radius. Pitch is clamped to avoid flipping at the poles, and camera distance is
clamped to a safe range. Right-stick input enters Manual/Showcase mode from
normal gameplay even while the menu is closed. R3 safely restores the automatic
camera, normal zoom, and the correct single-player or multiplayer path mode.

## Menu and telemetry layout

The controller legend is permanently visible while Debug Tools is open and
uses the final A/B/X/Y and LB/LT/RB/RT contract. Open-menu telemetry is reduced
to a compact two-line panel below the legend, preventing it from covering the
menu items, descriptions, or controller diagnostics. Full telemetry remains
available when the menu is closed.

## Existing pages and cumulative compatibility

Difficulty Lab, Player, Enemy/AI, Stage, Camera, Graphics, Hitboxes, and
Telemetry remain available. Runtime MSAA Off/2X/4X/8X behavior is preserved.

The build preserves the cumulative US feature set: Ultimate Slashuur and its
runtime/FPS fixes, Intro Skip v1, MSAA Frame State Lock v4, final brutal-v3
difficulties, Hor+ widescreen with intentionally stretched original UI, and all
previously accepted Debug Tools functionality. Anisotropic filtering, cold
breath, developer-menu boot, and the obsolete-disc-check fallback remain
disabled.

The T2 Mod Manager feature flag remains `debug-tools-f4`.

## Unique build identities

- Branch: `codex/debug-tools-controller-calibration-camera-v5-us`
- Workflow: `build-debug-tools-controller-calibration-camera-v5-us.yml`
- Artifact: `TMNT2-Debug-Tools-Controller-Calibration-Camera-v5-US-run-N`
- EXE: `TMNT2_Debug_Tools_Controller_Calibration_Camera_v5_US.exe`
- PDB: `TMNT2_Debug_Tools_Controller_Calibration_Camera_v5_US.pdb`
- MAP: `TMNT2_Debug_Tools_Controller_Calibration_Camera_v5_US.map`

Final controller direction, camera feel, and gameplay compatibility still
require hands-on acceptance in the separate test installation.
