# TMNT2 MSAA Frame State Lock v4 US

This EXE-only controlled build keeps the in-game **Options > Display >
ANTI-ALIASING** selector and extends the verified v3 surface fix with a runtime
frame-state lock.

The v3 trace proved that OFF created non-multisampled surfaces and 8X created
real 8-sample render-target, backbuffer, and depth surfaces. V4 addresses the
remaining runtime uncertainty: RenderWare or game setup code changing
`D3DRS_MULTISAMPLEANTIALIAS` after the device reset and before gameplay draws.

## What v4 changes

- Overrides the PC render-begin boundary.
- Waits until after `RwCameraBeginUpdate` succeeds.
- Immediately before the game's draw dispatcher, synchronizes RenderWare's
  render-state cache and directly forces the real D3D9 multisample state ON
  whenever a supported MSAA level is active in exclusive fullscreen.
- Repeats the lightweight state enforcement each rendered frame.
- Logs only the first frame after startup/change or a detected correction, so
  it does not introduce per-frame file logging or flushing.
- Preserves v3's real surface verification, disable-then-enable reset, and
  startup repair.

## Project scope

- **Project:** `TMNT2_MSAA_Frame_State_Lock_v4_US`
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
- Nexus's original forced-Normal behavior and the proven incoming-damage hook
  in `CCharacterAttackCalculator::CalcDamage`.
- No cold-breath restoration and no developer-menu boot.

## Download with GitHub Actions

1. Open the repository's **Actions** page.
2. Select **Build TMNT2 MSAA Frame State Lock v4 US**.
3. Open the newest successful run from
   `feature/msaa-frame-state-lock-v4-us`.
4. Download the artifact beginning with
   `TMNT2-MSAA-Frame-State-Lock-v4-US-run-`.
5. Confirm that the extracted files include:
   - `TMNT2_MSAA_Frame_State_Lock_v4_US.exe`
   - `TMNT2_MSAA_Frame_State_Lock_v4_US.pdb`
   - `TMNT2_MSAA_Frame_State_Lock_v4_US.map`
   - `BUILD_INFO_TMNT2_MSAA_Frame_State_Lock_v4_US.txt`

Keep the PDB and MAP with the package for exact crash analysis. They do not need
to be copied into the game directory.

## GitHub Desktop source instructions

1. Open the existing local `tmnt2` repository in GitHub Desktop.
2. Click **Fetch origin**.
3. Open **Current branch**, search for
   `feature/msaa-frame-state-lock-v4-us`, and select it.
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
3. Copy `TMNT2_MSAA_Frame_State_Lock_v4_US.exe` into the game directory.
4. Rename that copied file to `TMNT2.exe`.
5. Start the game normally without the `-wnd` argument.

## Controlled test

1. Use exclusive fullscreen at the same resolution for both passes.
2. Set **ANTI-ALIASING** to **OFF**, select **OK**, load a familiar stage, and
   capture a fixed view with thin diagonal geometry.
3. Set it to **8X MSAA**, select **OK**, return to the identical view, and take
   another capture.
4. Compare edge stair-stepping at 300-400% zoom. Do not combine another new
   graphics feature with this test.
5. Exit normally to flush the trace.

The 8X gameplay pass should contain a line similar to:

```text
MSAA frame_guard active=8 fullscreen=1 should_enable=1 ... after=1
```

If `corrected=1`, something disabled MSAA before the draw and v4 restored it.
If `corrected=0` with `before=1` and `after=1`, the state was already active and
the limited visual difference is not caused by the D3D9 MSAA state. In that
case, attach `TMNT2_Slashuur_trace.log` and the two comparison screenshots; the
next separate project should evaluate post-process AA or supersampling rather
than another MSAA-state patch.
