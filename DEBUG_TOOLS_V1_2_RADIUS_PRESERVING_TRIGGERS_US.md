# TMNT2 Debug Tools v1.2 Radius-Preserving Triggers US

Target: TMNT 2: Battle Nexus, US/NA 32-bit PC.

This release extends the restored Debug Tools v1.2.1 package with one isolated
camera control: physical LT and RT change the showcase camera's complete
eye-to-target radius without changing its yaw or pitch.

It is an EXE-only, composable T2 Mod Manager feature. It does not contain,
rebuild, or replace `TMNT.DAT` or any LPAC entry. No attachment is required.

## Camera behavior

- The existing v1.2 right-stick camera can activate while the Debug Menu is
  closed, as introduced in v1.2.1.
- Physical LT increases camera radius.
- Physical RT decreases camera radius.
- Radius control works in showcase/manual camera mode with the Debug Menu open
  or closed.
- The horizontal-distance and vertical-height components are multiplied by the
  same scale factor. Their ratio remains constant, preserving pitch exactly.
- Trigger input does not write to camera yaw.
- Existing safety limits remain in force for camera radius and height.
- R3 retains the v1.2 behavior that restores the normal game camera.

## Deliberately unchanged

All other source remains the restored v1.2 implementation: Debug Menu pages,
layout, telemetry, graphics/MSAA behavior, right-stick axis mapping, controller
bindings, Difficulty Lab, player tools, enemy/AI controls, stage actions,
hitboxes, telemetry, and screenshots.

This release does not include the rejected v1.3 raw-axis diagnostics,
calibration profiles, Input page, spherical-camera rewrite, controller
remapping, or UI changes.

## Cumulative compatibility

The build preserves Ultimate Slashuur and its runtime/FPS fixes, Intro Skip v1,
MSAA Frame State Lock v4, final brutal-v3 difficulties, Hor+ widescreen with
the intentionally stretched original UI, and the complete accepted v1.2 Debug
Tools feature set. Anisotropic filtering, cold breath, developer-menu boot, and
the obsolete-disc-check fallback remain disabled.

The T2 Mod Manager feature flag remains `debug-tools-f4`.

## Unique build identities

- Branch: `codex/debug-tools-v1-2-radius-preserving-triggers-us`
- Workflow: `build-debug-tools-v1-2-radius-preserving-triggers-us.yml`
- Artifact: `TMNT2-Debug-Tools-v1.2-Radius-Preserving-Triggers-US-run-N`
- EXE: `TMNT2_Debug_Tools_v1_2_Radius_Preserving_Triggers_US.exe`
- PDB: `TMNT2_Debug_Tools_v1_2_Radius_Preserving_Triggers_US.pdb`
- MAP: `TMNT2_Debug_Tools_v1_2_Radius_Preserving_Triggers_US.map`

Final trigger direction and camera feel require hands-on testing with the
user's controller in the separate test installation.
