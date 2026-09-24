# TMNT2 Debug Tools v1.2.4 Horizontal lZ Fix US

Target: TMNT 2: Battle Nexus, US/NA 32-bit PC.

This release extends the user-tested Debug Tools v1.2.3 package with one narrow
horizontal right-stick correction. It is an EXE-only, composable T2 Mod Manager
feature. It does not contain, rebuild, or replace `TMNT.DAT` or any LPAC entry.
No attachment is required.

## Corrected horizontal channel

The recovered PC DirectInput padfix identifies `lZ` as the physical horizontal
right-stick channel for the affected controller. Debug Camera now captures that
channel directly and locally:

- Right stick right orbits counter-clockwise/to the left.
- Right stick left orbits clockwise/to the right.
- Right stick up still raises the camera.
- Right stick down still lowers the camera.
- All camera controls remain available while the Debug Menu is closed.

The retail analog table and ordinary gameplay controls are unchanged. The raw
`lZ` channel is exposed only to Debug Camera, including its menu-closed showcase
activation check.

## Preserved accepted behavior

- LT/RT change the complete eye-to-target radius without changing yaw or pitch.
- R3 restores the normal game camera.
- Debug Tools v1.2 pages, layout, telemetry, MSAA control, Difficulty Lab,
  player tools, enemy/AI controls, stage actions, hitboxes, and screenshots are
  unchanged.
- The v1.2.3 vertical-axis correction remains unchanged.

This release does not include the rejected v1.3 diagnostics, calibration
profiles, Input page, spherical-camera rewrite, controller remapping, or UI
changes.

## Cumulative compatibility

The build preserves Ultimate Slashuur and its runtime/FPS fixes, Intro Skip v1,
MSAA Frame State Lock v4, final brutal-v3 difficulties, Hor+ widescreen with
the intentionally stretched original UI, and the accepted v1.2 Debug Tools
feature set. Anisotropic filtering, cold breath, developer-menu boot, and the
obsolete-disc-check fallback remain disabled.

The T2 Mod Manager feature flag remains `debug-tools-f4`.

## Unique build identities

- Branch: `codex/debug-tools-v1-2-horizontal-lz-fix-us`
- Workflow: `build-debug-tools-v1-2-horizontal-lz-fix-us.yml`
- Artifact: `TMNT2-Debug-Tools-v1.2-Horizontal-LZ-Fix-US-run-N`
- EXE: `TMNT2_Debug_Tools_v1_2_Horizontal_LZ_Fix_US.exe`
- PDB: `TMNT2_Debug_Tools_v1_2_Horizontal_LZ_Fix_US.pdb`
- MAP: `TMNT2_Debug_Tools_v1_2_Horizontal_LZ_Fix_US.map`

Final axis directions require hands-on testing with the user's controller in
the separate test installation.
