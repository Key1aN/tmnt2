# TMNT2 MSAA Active Surface Fix v3 US

This EXE-only controlled build keeps the in-game **Options > Display >
ANTI-ALIASING** selector and corrects the runtime path that must make the real
Direct3D 9 rendering surfaces multisampled.

## What v3 changes

- Keeps **OFF**, **2X MSAA**, **4X MSAA**, and **8X MSAA** in Display Settings.
- Forces `D3DRS_MULTISAMPLEANTIALIAS` through RenderWare's own render-state
  cache after startup and every live MSAA device reset.
- Cycles the RenderWare device through disabled MSAA before enabling a new
  level. This prevents RenderWare from returning early when its remembered
  selection does not match the real D3D9 surface.
- Reads and logs the actual D3D9 render target, backbuffer, and depth-buffer
  `MultiSampleType` and `MultiSampleQuality` values.
- Attempts one controlled disable-then-enable repair at startup if the real
  render target does not match the requested level.

The real surface result, not merely the menu selection, is recorded in
`TMNT2_Slashuur_trace.log` on lines beginning with `MSAA surface`.

## Project scope

- **Project:** `TMNT2_MSAA_Active_Surface_Fix_v3_US`
- **Target:** US/NA Win32 Release
- **Archive changes:** none
- **Required game data:** keep the existing final modded `TMNT.DAT`
- **Renderer:** Direct3D 9, exclusive fullscreen

No `.anm`, `.chr`, LPAC, AFS, `TMNT.DAT`, or `TMNTE.DAT` file is included or
modified.

## Preserved cumulative baseline

- Playable Slashuur's five boss moves, final controls, direct bone ID 3 crash
  fix, teleport cancel restrictions, vulnerable scythe, and short teleport
  invulnerability.
- One-handle buffered runtime/crash logging, flushed only on crash or shutdown.
- Home Station/Nexus animation pool increased from 384 to 512.
- Correct Hor+ widescreen 3D FOV and frustum/culling.
- Original intentionally stretched widescreen UI.
- Final brutal-v3 Very Hard, Extreme, and Souls Like balance and safe mapping
  to the original Hard table row.
- Nexus's original forced-Normal table behavior and the proven incoming-damage
  hook in `CCharacterAttackCalculator::CalcDamage`.
- No cold-breath restoration and no developer-menu boot.

## Download with GitHub Actions

1. Open the repository's **Actions** page.
2. Select **Build TMNT2 MSAA Active Surface Fix v3 US**.
3. Open the newest successful run from
   `feature/msaa-active-surface-fix-v3-us`.
4. Download the artifact beginning with
   `TMNT2-MSAA-Active-Surface-Fix-v3-US-run-`.
5. Confirm that the extracted files include:
   - `TMNT2_MSAA_Active_Surface_Fix_v3_US.exe`
   - `TMNT2_MSAA_Active_Surface_Fix_v3_US.pdb`
   - `TMNT2_MSAA_Active_Surface_Fix_v3_US.map`
   - `BUILD_INFO_TMNT2_MSAA_Active_Surface_Fix_v3_US.txt`

Keep the PDB and MAP with the package for exact crash analysis. They do not need
to be copied into the game directory.

## GitHub Desktop source instructions

1. Open the existing local `tmnt2` repository in GitHub Desktop.
2. Click **Fetch origin**.
3. Open **Current branch**, search for
   `feature/msaa-active-surface-fix-v3-us`, and select it.
4. Click **Pull origin** if shown.
5. For a local Windows build, open a Visual Studio 2022 Developer PowerShell in
   the repository folder and provide the legacy DirectX SDK through `DXSDK_DIR`.
6. Run `cmake --preset vs2022 -DOPT_EU_BUILD=OFF`.
7. Run `cmake --build --preset vs2022-release --parallel`.

GitHub Actions is the easier build path because the workflow stages and verifies
the DirectX headers and x86 libraries automatically.

## Installation

1. Back up the currently working `TMNT2.exe` and `TMNT2.ini`.
2. Keep the current final modded `TMNT.DAT`; do not replace or rebuild it.
3. Copy `TMNT2_MSAA_Active_Surface_Fix_v3_US.exe` into the game directory.
4. Rename that copied file to `TMNT2.exe`.
5. Start the game normally without the `-wnd` argument.

## Controlled visual and runtime test

1. Use exclusive fullscreen mode.
2. Open **Options > Display**, select **ANTI-ALIASING**, choose **OFF**, then
   select **OK**.
3. Load one familiar stage and capture a fixed view containing thin diagonal
   geometry or a strong silhouette edge.
4. Return to Display, choose **4X MSAA**, select **OK**, and repeat exactly the
   same view.
5. Compare only edge stair-stepping. Do not combine another new graphics mod
   with this test.
6. Exit normally to flush the buffered trace.

For the OFF pass, the relevant `MSAA surface` line should report
`expected=0 actual=0 rt_type=0`. For the 4X pass it should report
`expected=4 actual=4 rt_type=4`, with `state_after=1`.

If the image still does not change, attach the exact file
`TMNT2_Slashuur_trace.log` from the game directory. The RT, backbuffer, depth,
and render-state fields in that one file identify whether the remaining cause
is RenderWare, the Direct3D driver/wrapper, or the visual comparison itself.
