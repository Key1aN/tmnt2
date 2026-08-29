# TMNT2 MSAA Display Menu v2 US

This EXE-only update moves the MSAA control into the game's existing
**Options > Display** screen. Manual INI editing is no longer required.

## In-game control

1. Open **Options > Display**.
2. Select **ANTI-ALIASING**.
3. Choose **OFF**, **2X MSAA**, **4X MSAA**, or **8X MSAA** with Left/Right.
4. Return to the Display list and select **OK** to apply it.

The selected value is saved automatically through the same PC settings backend
used by the resolution option. The original game-save layout is unchanged.

MSAA is enabled only in exclusive fullscreen mode in this controlled version.
Windowed mode preserves the selected preference but renders with MSAA disabled.
The renderer clamps 2x/4x/8x requests to the selected fullscreen mode's reported
capability and falls back to disabled MSAA if a live device change fails.

## Project scope

- **Project:** `TMNT2_MSAA_Display_Menu_v2_US`
- **Target:** US/NA Win32 Release
- **Archive changes:** none
- **Required game data:** keep the existing final modded `TMNT.DAT`
- **Default when no previous setting exists:** 4X MSAA

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
- Nexus's original forced-Normal table behavior and the proven central incoming
  damage hook in `CCharacterAttackCalculator::CalcDamage`.
- No cold-breath restoration and no developer-menu boot.

## Download with GitHub Actions

1. Open the repository's **Actions** page.
2. Select **Build TMNT2 MSAA Display Menu v2 US**.
3. Open the newest successful run from
   `feature/msaa-display-menu-v2-us`.
4. Download the artifact beginning with
   `TMNT2-MSAA-Display-Menu-v2-US-run-`.
5. Confirm that the extracted files include:
   - `TMNT2_MSAA_Display_Menu_v2_US.exe`
   - `TMNT2_MSAA_Display_Menu_v2_US.pdb`
   - `TMNT2_MSAA_Display_Menu_v2_US.map`
   - `BUILD_INFO_TMNT2_MSAA_Display_Menu_v2_US.txt`

Keep the PDB and MAP with the downloaded package for exact crash analysis. They
do not need to be copied to the game directory.

## GitHub Desktop source instructions

1. Open the existing local `tmnt2` repository in GitHub Desktop.
2. Click **Fetch origin**.
3. Open **Current branch**, search for
   `feature/msaa-display-menu-v2-us`, and select it.
4. Click **Pull origin** if shown.
5. For a local Windows build, open a Visual Studio 2022 Developer PowerShell in
   the repository folder and provide the legacy DirectX SDK through `DXSDK_DIR`.
6. Run `cmake --preset vs2022 -DOPT_EU_BUILD=OFF`.
7. Run `cmake --build --preset vs2022-release --parallel`.

GitHub Actions is the easier path because the workflow stages and verifies the
required DirectX headers and x86 libraries automatically.

## Installation

1. Back up the currently working `TMNT2.exe` and `TMNT2.ini`.
2. Keep the current final modded `TMNT.DAT`; do not replace or rebuild it.
3. Copy `TMNT2_MSAA_Display_Menu_v2_US.exe` into the game directory.
4. Rename that copied file to `TMNT2.exe`.
5. Start the game normally without the `-wnd` argument.

## Controlled test

1. Open **Options > Display** and verify that **ANTI-ALIASING** appears after
   Resolution and before OK.
2. Select **OFF**, choose **OK**, load one familiar stage, and capture a fixed
   camera view containing diagonal geometry.
3. Return to Display, select **4X MSAA**, choose **OK**, and repeat the same view.
4. Compare only edge stair-stepping. Do not combine another new graphics mod
   with this test.
5. Exit normally so the buffered trace is flushed.
6. Check `TMNT2_Slashuur_trace.log` for the build identifier and lines beginning
   with `MSAA display_apply`, `MSAA display_menu`, or `MSAA mode_change`.

If applying a value causes a black screen or device failure, restart, choose
**OFF** in Display Settings, exit normally, and preserve the trace plus the
matching EXE/PDB/MAP package for analysis.
