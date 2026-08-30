# TMNT2 Slashuur Adaptive Teleport v1 US

This controlled EXE-only build upgrades playable Slashuur's existing
**Guard + Dash** teleport without changing its controls, animation, effects, or
invulnerability timing.

## Exact behavior

When teleport starts, the game scans for the nearest enemy that is:

- alive and actively running;
- currently drawn rather than hidden, spawning, retreating, or dying;
- visible on the gameplay camera;
- within **10.0 world units** of Slashuur.

If a target qualifies, Slashuur uses boss `enemy087`'s rear-teleport geometry:

1. Read the target's current foot position and facing direction.
2. Calculate a point exactly **1.0 world unit behind** the target.
3. Check the short target-to-rear line against stage collision.
4. Use the closest valid collision point when a wall blocks the rear point.
5. Snap the destination to map height.
6. Make Slashuur face the same direction as the target.
7. Reappear with the existing warp effect, sound, collision restoration, and
   teleport strike hit sphere.

If no enemy qualifies, the final existing **5.0-unit forward teleport** runs
unchanged. The same fallback is used if the chosen enemy dies, disappears, or
its enemy slot is replaced during Slashuur's vanish window.

When several enemies qualify, the nearest one is selected, matching boss
Slashuur's nearest-target preference.

## Project scope

- **Project:** `TMNT2_Slashuur_Adaptive_Teleport_v1_US`
- **Target:** US/NA Win32 Release
- **Archive changes:** none
- **Required game data:** keep the existing final modded `TMNT.DAT`
- **Base:** cumulative Intro Skip v1 US baseline
- **Anisotropic filtering:** not included

No `.anm`, `.chr`, LPAC, AFS, `TMNT.DAT`, or `TMNTE.DAT` file is
included or modified.

## Preserved cumulative baseline

- Intro Skip v1: save-data check goes directly to the normal title screen while
  title attract movies and all story movies remain available.
- Confirmed MSAA frame-state-lock v4 and its in-game Display selector.
- Playable Slashuur's five boss moves and final control mapping.
- Teleport may cancel only normal combo and charged-attack states.
- Teleport retains its short hidden/collisionless invulnerability phase.
- Spinning scythe remains vulnerable.
- Purple energy ball and ground blast keep the direct bone ID 3 crash fix.
- One-handle buffered runtime/crash logging, flushed only on crash or shutdown.
- Home Station/Nexus animation pool increased from 384 to 512.
- Correct Hor+ widescreen 3D FOV and frustum/culling.
- Original intentionally stretched widescreen UI.
- Final brutal-v3 difficulties, safe Hard-row mapping, Nexus forced-Normal
  behavior, and the proven incoming-damage hook.
- No cold-breath restoration and no developer-menu boot.

## Diagnostic breadcrumbs

Targeted teleport:

```text
MOVE teleport target selected enemy=... handle=... distance=... radius=10.000
MOVE teleport relocate mode=enemy_rear enemy=... wall_adjusted=... destination=(...)
```

Normal fallback:

```text
MOVE teleport target none radius=10.000 fallback=forward
MOVE teleport relocate mode=forward destination=(...)
```

If the selected enemy becomes invalid during the vanish window:

```text
MOVE teleport target lost enemy=... expected_handle=... fallback=forward
```

These are emitted once per teleport event through the existing buffered handle;
there is no per-frame file opening or flushing.

## Download with GitHub Actions

1. Open the repository's **Actions** page.
2. Select **Build TMNT2 Slashuur Adaptive Teleport v1 US**.
3. Open the newest successful run from
   `feature/slashuur-adaptive-teleport-v1-us`.
4. Download the artifact beginning with
   `TMNT2-Slashuur-Adaptive-Teleport-v1-US-run-`.
5. Confirm that it contains:
   - `TMNT2_Slashuur_Adaptive_Teleport_v1_US.exe`
   - `TMNT2_Slashuur_Adaptive_Teleport_v1_US.pdb`
   - `TMNT2_Slashuur_Adaptive_Teleport_v1_US.map`
   - `BUILD_INFO_TMNT2_Slashuur_Adaptive_Teleport_v1_US.txt`

Keep the matching PDB and MAP in the downloaded package for crash analysis.
They do not need to be copied into the game directory.

## GitHub Desktop and local build

1. Open the existing `tmnt2` repository in GitHub Desktop.
2. Click **Fetch origin**.
3. Open **Current branch**, search for
   `feature/slashuur-adaptive-teleport-v1-us`, and select it.
4. Click **Pull origin** if shown.
5. For a local build, open a Visual Studio 2022 Developer PowerShell in the
   repository folder and set `DXSDK_DIR` to a compatible DirectX SDK.
6. Run `cmake --preset vs2022 -DOPT_EU_BUILD=OFF`.
7. Run `cmake --build --preset vs2022-release --parallel`.

GitHub Actions is easier because the workflow stages and verifies the required
DirectX headers and x86 libraries automatically.

## Installation

1. Back up the currently working `TMNT2.exe`.
2. Keep the existing final modded `TMNT.DAT`; do not replace or rebuild it.
3. Copy `TMNT2_Slashuur_Adaptive_Teleport_v1_US.exe` into the game directory.
4. Rename the copied file to `TMNT2.exe`.
5. Start the game normally.

## Controlled test

Use **Guard + Dash** and test only this feature:

1. With no enemy nearby, confirm Slashuur still teleports about five units
   forward.
2. Put one visible enemy within ten units, face any direction, and teleport.
   Confirm Slashuur appears behind the enemy and adopts its facing.
3. Move the visible enemy beyond ten units and confirm the forward fallback.
4. Put a nearby enemy off screen and confirm the forward fallback.
5. Put two visible enemies within ten units and confirm the nearer one is used.
6. Confirm normal combo and charged-attack cancel still work.
7. Confirm damage, knockdown, grab, and stun states still block the cancel.
8. Exit normally to flush the trace and attach
   `TMNT2_Slashuur_trace.log` only if a case behaves unexpectedly.
