# TMNT2 MSAA Restore v1 US

This is the first controlled graphics upgrade for the US/NA 32-bit PC build.
It restores the dormant Direct3D 9 multisample anti-aliasing path without
changing game data, UI layout, camera projection, combat, difficulty, or
playable Slashuur behavior.

## Project scope

- **Project:** `TMNT2_MSAA_Restore_v1_US`
- **Target:** US/NA Win32 Release
- **Archive changes:** none; keep the existing final modded `TMNT.DAT`
- **Default:** 4x MSAA in exclusive fullscreen mode
- **Available values:** 0, 2, 4, or 8 samples
- **Windowed mode:** MSAA is disabled in v1 for a safer first test

The original PC renderer already queried the maximum multisampling capability
of every fullscreen video mode. However, it left the selected device's level
at zero, so the startup call could never enable useful MSAA. This restoration
connects the saved setting to the selected mode, clamps it to a standard level
supported by that mode, and uses RenderWare's post-start change function when
the resolution changes.

## Configuration

The setting is stored in the INI belonging to the executable. When the test
executable is installed as `TMNT2.exe`, edit `TMNT2.ini` beside it:

```ini
[GRAPHICS]
MSAA=4
```

Use one of these values:

| Value | Result |
|---:|---|
| 0 | MSAA disabled |
| 2 | 2x MSAA |
| 4 | 4x MSAA (default) |
| 8 | 8x MSAA |

Other positive values are normalized downward to 0, 2, 4, or 8. The renderer
then clamps that request to the maximum reported for the selected fullscreen
resolution. For example, an 8x request becomes 4x if the mode reports 4x as
its maximum.

## Preserved cumulative baseline

- Playable Slashuur's five boss moves, final inputs, direct-bone crash fix,
  teleport combo-cancel restrictions, vulnerable scythe, and brief teleport
  invulnerability.
- Buffered runtime/crash logging with flushing only on crash or shutdown.
- Home Station/Nexus animation pool of 512 records.
- Stable Hor+ widescreen 3D FOV and frustum/culling.
- Original intentionally stretched widescreen UI.
- Final brutal-v3 Very Hard, Extreme, and Souls Like modes, including the
  safe original-Hard-row mapping and central player-defender damage hook.
- Nexus's original forced-Normal table behavior.
- No cold-breath restoration and no developer-menu boot.

## Easiest build method: GitHub Actions

1. Open the repository on GitHub and select the
   `feature/msaa-restore-v1-us` branch.
2. Open **Actions**.
3. Select **Build TMNT2 MSAA Restore v1 US**.
4. Open the newest successful run for that branch.
5. Download the artifact whose name starts with
   `TMNT2-MSAA-Restore-v1-US-run-`.
6. Extract the ZIP. Confirm that it contains these matching, uniquely named
   files:
   - `TMNT2_MSAA_Restore_v1_US.exe`
   - `TMNT2_MSAA_Restore_v1_US.pdb`
   - `TMNT2_MSAA_Restore_v1_US.map`
   - `BUILD_INFO_TMNT2_MSAA_Restore_v1_US.txt`

The PDB and MAP are for crash analysis; they do not need to be installed in
the game directory.

## GitHub Desktop source instructions

1. In GitHub Desktop, open the existing local `tmnt2` repository.
2. Click **Fetch origin**.
3. Open **Current branch**, search for `feature/msaa-restore-v1-us`, and select
   that remote branch. GitHub Desktop will create and check out its local
   tracking branch.
4. Click **Pull origin** if it appears.
5. For a local Windows build, open a Developer PowerShell for Visual Studio
   2022 in the repository folder and provide the legacy DirectX SDK through
   `DXSDK_DIR`.
6. Run `cmake --preset vs2022 -DOPT_EU_BUILD=OFF`.
7. Run `cmake --build --preset vs2022-release --parallel`.

GitHub Actions is recommended for this project because its workflow stages and
verifies the required DirectX headers and x86 libraries automatically.

## Installation

1. Back up the current working `TMNT2.exe` and `TMNT2.ini`.
2. Do **not** replace or rebuild `TMNT.DAT`; keep the final expanded Slashuur
   archive already installed.
3. Copy `TMNT2_MSAA_Restore_v1_US.exe` into the game folder.
4. Rename the copied file to `TMNT2.exe`.
5. Leave the PDB and MAP in the downloaded artifact so they stay matched to
   this exact executable.
6. Start the game normally, without the `-wnd` argument.

If `[GRAPHICS]` or `MSAA` is absent, 4x is used and written to the INI after a
normal shutdown.

## One-feature test procedure

1. Set `MSAA=0`, start the game in fullscreen, load one familiar stage, and
   capture a screenshot containing diagonal geometry.
2. Exit normally so the buffered trace is flushed.
3. Set `MSAA=4`, repeat the same stage and camera position, then exit normally.
4. Compare only edge stair-stepping in the two screenshots. Do not combine
   this test with another new graphics change.
5. Check `TMNT2_Slashuur_trace.log` beside the executable. Expected lines begin
   with `BUILD TMNT2_MSAA_Restore_v1_US` and `MSAA`. A successful fullscreen
   start reports the request, mode maximum, and active sample count.
6. After startup is confirmed, change fullscreen resolution once in
   **Options > Display** and check for an `MSAA mode_change ... result=success`
   breadcrumb after normal exit.

If the game cannot start on a particular GPU or compatibility wrapper, set
`MSAA=0` and retry. Preserve the downloaded EXE/PDB/MAP together and provide
the trace plus any generated crash log/minidump for analysis.
