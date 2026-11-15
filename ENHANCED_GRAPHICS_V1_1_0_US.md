# TMNT2 Enhanced Graphics v1.1.0 (US) — `TMNT2_Enhanced_Graphics_v1.1.t2mod`

Switchable graphics remaster for TMNT 2: Battle Nexus (PC, US/NA) that keeps the 2003 cel-shaded look:
crisper toon lighting, soft contact shadows, stylized water overlay, light bloom / colour grade.
v1.1 adds a controller-friendly **REMASTER** tab to the Debug Tools menu and fixes the issues found in
your first real-PC log. Built on the Debug Tools v1.2.24 / Camera Overhaul v1.0.1 core (`e69ca81`).

**Status:** played in the real game (your `TMNT.DAT`, Combat Lab, under Wine on Linux) with the REMASTER
tab driven by keyboard and mouse; v1.0 ran on your GTX 1660 Ti at 59.9 fps (vsync cap) with
0.11–0.40 ms GPU post cost. A real Xbox pad was not connected here: the pad uses the same Debug Tools
input path as every other tab, but it was not pressed on a pad.

---

## 1. REMASTER tab (Debug Tools add-on)

Open Debug Tools (**F4** / your pad binding), then **LB/RB** (Q/E) until **REMASTER** (last tab, only
shown when this package is enabled). Same controls as the OVERHAUL tab:

| Pad | Keyboard / mouse | Action |
| --- | --- | --- |
| D-pad / left stick up-down | ↑ ↓ / wheel | Move |
| D-pad / left stick left-right (hold = repeat) | ← → / drag slider | Change value, flip ON/OFF, change preset |
| A | Enter / double-click / *Run* | Run an action (Save, Reload, Clear edits) |
| X | *Reset item* / right-click slider | Reset this setting to the preset value |
| Y | *Reset tab* | Clear all edits (back to the preset) |
| LB / RB | Q / E / click tab | Switch tab |

Rows: **Enhanced graphics (F8)** master ON/OFF (OFF = original 2003 paths exactly), **Preset** (Classic+ /
Enhanced / High, shows `+ N edits`), live **Performance** readout (fps, GPU post and water ms), then
Toon lighting (contrast, shadow tint, highlight, light rig, ambient, key light, warmth), Shadows (soft
shadows, opacity, size, contact shadow, contact opacity, sun offset), Water (on/off, *detected on this
stage* readout, strength, reflection, specular, foam, wave scale/strength, detail), Screen (bloom,
amount, threshold, colour grade, contrast, saturation, split tone, vignette, FXAA, trilinear), and
Settings file (**Save to TMNT2_graphics.ini**, **Reload**, **Clear edits**). Each row has a help line
(scope + description) at the bottom of the menu.

Every change applies live (log line `APPLY reason=menu-edit`). Edits are kept until you Save; Save
writes the preset, the ON/OFF side and every edit as `[Overrides]`. In the ImGui menu style the tab
draws real sliders in one aligned column so labels stay readable at any window size; the game-font
menu style shows the native slider bar.

## 2. What changed since v1.0

* **New:** REMASTER tab (above). Live readouts (fps EMA, GPU post / water EMA). Edits switch the mode to
  ENHANCED automatically.
* **Fix — false water on the Colosseum floor** (your log, `m60x_colosseum_floor1`): a material is now a
  water candidate only if it is ≥85% horizontal **and** (named like water **or** UV-animated and ≥25% over
  collision water). Plain floors over collision water are no longer overlaid. Log lines now also show
  each candidate's height range and average normal.
* **Fix — flat toon ramp (`pat05`)** is left unchanged (it was being given a gradient); highlight is
  driven by luminance.
* **Honest trilinear:** your log showed 0 mipmapped textures, so trilinear has no visible effect on the
  stock game; the menu help says so.
* Toon gradient report only for named texture sets; DXT textures skipped by the water/toon scanners.

## 3. Installation

Requirements: retail US/NA `TMNT2.exe` (SHA-256 `b827478461e1…a89564`) and your current T2 Mod Manager
(composable-core support). No loader or manager update needed.

1. Copy `TMNT2_Enhanced_Graphics_v1.1.t2mod` to `C:\TMNT2-CMM\Mod-Packages` (already done).
2. In the manager, remove / disable v1.0 and add + enable v1.1 (same id `Enhanced_Graphics`, version 1.1.0).
3. Keep **Debug Tools** enabled to get the REMASTER tab (F8 works without it).

Core priority **160**, capabilities = v1.2.24 core + `enhanced-graphics`, so it combines with Debug
Tools, Combo Assist, Karai, MSAA, widescreen etc. `TMNT.DAT` is never modified. Your existing
`TMNT2_graphics.ini` keeps working.

## 4. Uninstall / restore

Disable the package; the manager falls back to the next core your packages need or restores the
verified vanilla EXE. Optionally delete `TMNT2_graphics.ini` / `TMNT2_graphics.log`.

## 5. Building

Branch `claude/enhanced-graphics-v1-0-0-us`; MSVC workflow `.github/workflows/build-enhanced-graphics-v1-1-0-us.yml`
(checks the REMASTER wiring), package with `python tools/EnhancedGraphics/pack_t2mod.py <core exe> TMNT2_Enhanced_Graphics_v1.1.t2mod`.
The delivered core is the MinGW i686 cross-build (`tools/EnhancedGraphics/mingw/build.py`, stripped,
reproducible), core EXE SHA-256 `5e3f34e74f4b26667c3ee9e7dc2dfada11a1bdfd1eee4edf33365c10e7b96e5b`.

## 6. Test results (v1.1)

| # | Check | Result |
| --- | --- | --- |
| 1 | Full EXE builds and links (MinGW, release stripped + unstripped test build) | PASS |
| 2 | CPU unit tests incl. new flat-ramp test | PASS |
| 3 | Shader render tests (Wine D3D9 vs C++ reference) | PASS |
| 4 | Manager code: package opens/validates, payload verified, **T2Delta.ApplyAsync on your genuine retail TMNT2.exe** rebuilds the core byte-for-byte | PASS |
| 5 | In game with your real `TMNT.DAT` + your feature set (Combat Lab, Wine/llvmpipe): toon 5/5, shadows, post, no crash, 39–48 fps logged on a software GPU (llvmpipe) | PASS |
| 6 | Colosseum floor no longer detected as water (`WATER none on this stage`) | PASS |
| 7 | REMASTER tab: appears last, keyboard navigation, value change → `APPLY reason=menu-edit`, preset shows `+ 1 edit`, Save writes `ToonContrast=0.70` to the INI, tooltips / help lines | PASS (screenshots in `v1.1-test-results/`) |
| — | Physical Xbox pad on the tab; GTX 1660 Ti perf of v1.1; other stages' water | **NOT VERIFIED** here |

Note: the Wine run needed a test-only guard in the sound loader (Wine-specific missing VAG bank). It is
**not** in the shipped EXE; the release build is from clean sources.

## 7. Limitations

* SSAO not implemented (D3D9 / RenderWare depth access conflicts with MSAA); no rim light, no
  character-shaped shadow maps, no refraction / planar reflection, no anisotropic filtering.
* Water detection is heuristic; use `[Water] Include / Exclude` in the INI and check the log.
* Stock textures have no mip levels, so trilinear does nothing on them.
* Delivered EXE is a MinGW build, not MSVC; build the MSVC one with the workflow if anything unrelated
  to graphics behaves differently.
