# TMNT2 Debug Tools v1.2 Right-Stick Axis Fix US

Target: TMNT 2: Battle Nexus, US/NA 32-bit PC.

This release extends the accepted Debug Tools v1.2.2 package with a local
right-stick axis correction based on the user's observed controller behavior.

It is an EXE-only, composable T2 Mod Manager feature. It does not contain,
rebuild, or replace `TMNT.DAT` or any LPAC entry. No attachment is required.

## Corrected right-stick behavior

- Moving the right stick right orbits the camera counter-clockwise/to the left
  without changing camera height.
- Moving the right stick left orbits the camera clockwise/to the right without
  changing camera height.
- Moving the right stick up raises the camera.
- Moving the right stick down lowers the camera.
- Right-stick camera activation remains available while the Debug Menu is
  closed.

The recovered PC controller layer reports this controller's physical vertical
right-stick movement through the internal right-stick X identity and its
physical horizontal movement through the internal right-stick Y identity. The
Debug Camera now swaps those two inputs and corrects the vertical sign locally.
The global controller table and normal gameplay controls are unchanged.

## Preserved v1.2.2 radius control

- Physical LT increases the complete eye-to-target camera radius.
- Physical RT decreases the complete eye-to-target camera radius.
- Radius control works in showcase/manual mode whether the Debug Menu is open
  or closed.
- Horizontal distance and vertical height are scaled proportionally, so LT/RT
  do not change the current yaw or pitch.
- R3 restores the normal game camera.

## Deliberately unchanged

All other source remains the restored v1.2 implementation: Debug Menu pages,
layout, telemetry, graphics/MSAA behavior, controller bindings, Difficulty Lab,
player tools, enemy/AI controls, stage actions, hitboxes, telemetry, and
screenshots.

This release does not include the rejected v1.3 raw-axis diagnostics,
calibration profiles, Input page, spherical-camera rewrite, controller
remapping, or UI changes.

## Cumulative compatibility

The build preserves Ultimate Slashuur and its runtime/FPS fixes, Intro Skip v1,
MSAA Frame State Lock v4, final brutal-v3 difficulties, Hor+ widescreen with
the intentionally stretched original UI, and the accepted v1.2 Debug Tools
feature set. Anisotropic filtering, cold breath, developer-menu boot, and the
obsolete-disc-check fallback remain disabled.

The T2 Mod Manager feature flag remains `debug-tools-f4`.

## Unique build identities

- Branch: `codex/debug-tools-v1-2-right-stick-axis-fix-us`
- Workflow: `build-debug-tools-v1-2-right-stick-axis-fix-us.yml`
- Artifact: `TMNT2-Debug-Tools-v1.2-Right-Stick-Axis-Fix-US-run-N`
- EXE: `TMNT2_Debug_Tools_v1_2_Right_Stick_Axis_Fix_US.exe`
- PDB: `TMNT2_Debug_Tools_v1_2_Right_Stick_Axis_Fix_US.pdb`
- MAP: `TMNT2_Debug_Tools_v1_2_Right_Stick_Axis_Fix_US.map`

Final axis directions require hands-on testing with the user's controller in
the separate test installation.
