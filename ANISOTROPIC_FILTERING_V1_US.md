# TMNT2 Anisotropic Filtering v1 US

This controlled EXE-only build adds **ANISOTROPIC** to the game's existing
**Options > Display** menu. The available values are **OFF**, **2X AF**,
**4X AF**, **8X AF**, and **16X AF**.

## What v1 changes

- Attaches and links RenderWare 3.7's official `RpAnisot` texture plugin.
- Queries the D3D9 device's maximum supported anisotropy and clamps the chosen
  value to the nearest supported menu level.
- Applies anisotropic filtering only to textures that contain mip levels.
- Uses trilinear mip selection for those eligible textures while AF is active.
- Restores the retail game's linear filtering and anisotropy 1 when set to
  **OFF**.
- Leaves non-mipmapped textures on the original linear filtering path. This
  keeps UI, HUD, fonts, most particles, movies, and render targets out of the
  anisotropic pass while improving receding world and model textures.
- Saves the selected active level through the existing PC settings backend.
- Adds lightweight buffered breadcrumbs for requested, supported, and active
  levels plus configured texture counts. There is no per-frame logging.

The default is **16X AF** on a new settings file. Unsupported values are
automatically reduced; for example, a GPU limited to 8X will display and save
8X after the setting is applied.

## Project scope

- **Project:** `TMNT2_Anisotropic_Filtering_v1_US`
- **Target:** US/NA Win32 Release
- **Archive changes:** none
- **Required game data:** keep the existing final modded `TMNT.DAT`
- **Renderer:** Direct3D 9

No `.anm`, `.chr`, LPAC, AFS, `TMNT.DAT`, or `TMNTE.DAT` file is included or
modified.

## Preserved cumulative baseline

- Confirmed MSAA frame-state-lock v4 behavior and its in-game selector.
- Playable Slashuur's five boss moves, final controls, direct bone ID 3 crash
  fix, teleport cancel restrictions, vulnerable scythe, and short teleport
  invulnerability.
- One-handle buffered runtime/crash logging, flushed only on crash or shutdown.
- Home Station/Nexus animation pool increased from 384 to 512.
- Correct Hor+ widescreen 3D FOV and frustum/culling.
- Original intentionally stretched widescreen UI.
- Final brutal-v3 Very Hard, Extreme, and Souls Like balance and safe mapping
  to the original Hard table row.
- Nexus's original forced-Normal behavior and the proven incoming-damage hook
  in `CCharacterAttackCalculator::CalcDamage`.
- No cold-breath restoration and no developer-menu boot.

## Download with GitHub Actions

1. Open the repository's **Actions** page.
2. Select **Build TMNT2 Anisotropic Filtering v1 US**.
3. Open the newest successful run from
   `feature/anisotropic-filtering-v1-us`.
4. Download the artifact beginning with
   `TMNT2-Anisotropic-Filtering-v1-US-run-`.
5. Confirm that the extracted files include:
   - `TMNT2_Anisotropic_Filtering_v1_US.exe`
   - `TMNT2_Anisotropic_Filtering_v1_US.pdb`
   - `TMNT2_Anisotropic_Filtering_v1_US.map`
   - `BUILD_INFO_TMNT2_Anisotropic_Filtering_v1_US.txt`

Keep the PDB and MAP with the package for exact crash analysis. They do not need
to be copied into the game directory.

## GitHub Desktop source instructions

1. Open the existing local `tmnt2` repository in GitHub Desktop.
2. Click **Fetch origin**.
3. Open **Current branch**, search for
   `feature/anisotropic-filtering-v1-us`, and select it.
4. Click **Pull origin** if shown.
5. For a local Windows build, open a Visual Studio 2022 Developer PowerShell in
   the repository folder and provide the legacy DirectX SDK through
   `DXSDK_DIR`.
6. Run `cmake --preset vs2022 -DOPT_EU_BUILD=OFF`.
7. Run `cmake --build --preset vs2022-release --parallel`.

GitHub Actions is the easier build path because the workflow stages and
verifies the DirectX headers and x86 libraries automatically.

## Installation

1. Back up the currently working `TMNT2.exe` and `TMNT2.ini`.
2. Keep the current final modded `TMNT.DAT`; do not replace or rebuild it.
3. Copy `TMNT2_Anisotropic_Filtering_v1_US.exe` into the game directory.
4. Rename that copied file to `TMNT2.exe`.
5. Start the game normally.

## Controlled test

Keep the resolution and MSAA value unchanged for both passes.

1. Load a familiar stage with a long floor, road, brick wall, or tiled surface
   extending away from the camera.
2. Set **ANISOTROPIC** to **OFF**, select **OK**, return to the fixed view, and
   take a screenshot.
3. Set it to **16X AF**, select **OK**, return to exactly the same view, and take
   another screenshot.
4. Compare distant texture detail and the transition between mip levels. AF
   should sharpen angled surfaces; it does not smooth polygon silhouettes or
   replace MSAA.
5. Exit normally so the buffered trace is flushed.

The trace should include lines similar to:

```text
AF apply requested=16 supported=16 active=16 textures=... mipmapped=... anisotropic=...
AF first_texture name=... mip_levels=... active=16
```

If `supported` or `active` is below the selected value, the setting was safely
clamped to the GPU limit. If `anisotropic=0` after applying 16X in a loaded
stage, attach `TMNT2_Slashuur_trace.log`; that would mean the current texture
dictionaries contain no eligible mipmapped textures or need a separate
runtime sampler-state investigation.
