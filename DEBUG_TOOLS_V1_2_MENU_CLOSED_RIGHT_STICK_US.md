# TMNT2 Debug Tools v1.2 Menu-Closed Right Stick US

Target: TMNT 2: Battle Nexus, US/NA 32-bit PC.

This release is the previously working Debug Tools Graphics Camera v1.2.0
source with one change only: the existing right-stick showcase-camera handler
can activate while the Debug Menu is closed.

It is an EXE-only, composable T2 Mod Manager feature. It does not contain,
rebuild, or replace `TMNT.DAT` or any LPAC entry. No attachment is required.

## Deliberately unchanged from v1.2.0

- All Debug Menu pages, layout, telemetry, graphics options, and MSAA behavior.
- The previous controller bindings and on-screen controller legend.
- The previous showcase-camera implementation and axis mapping.
- `Back/View` menu access, `Start` pause behavior, and `R3` camera reset.
- Difficulty Lab, player tools, enemy/AI tools, stage actions, hitboxes,
  telemetry, screenshots, and cumulative mod compatibility.

This release does not include the v1.3 raw-axis diagnostics, controller
profiles, Input page, spherical yaw/pitch/radius camera, remapped shoulder
buttons, revised telemetry layout, or any other v1.3 change. Right-stick axis
directions behave exactly as they did in the working v1.2.0 build; this release
only removes the requirement to have the Debug Menu open before that existing
camera handler can activate.

## Cumulative compatibility

The build preserves Ultimate Slashuur and its runtime/FPS fixes, Intro Skip v1,
MSAA Frame State Lock v4, final brutal-v3 difficulties, Hor+ widescreen with
the intentionally stretched original UI, and the complete v1.2.0 Debug Tools
feature set. Anisotropic filtering, cold breath, developer-menu boot, and the
obsolete-disc-check fallback remain disabled.

The T2 Mod Manager feature flag remains `debug-tools-f4`.

## Unique build identities

- Branch: `codex/debug-tools-v1-2-menu-closed-right-stick-us`
- Workflow: `build-debug-tools-v1-2-menu-closed-right-stick-us.yml`
- Artifact: `TMNT2-Debug-Tools-v1.2-Menu-Closed-Right-Stick-US-run-N`
- EXE: `TMNT2_Debug_Tools_v1_2_Menu_Closed_Right_Stick_US.exe`
- PDB: `TMNT2_Debug_Tools_v1_2_Menu_Closed_Right_Stick_US.pdb`
- MAP: `TMNT2_Debug_Tools_v1_2_Menu_Closed_Right_Stick_US.map`

Final behavior still requires hands-on testing with the user's controller in
the separate test installation.
