# TMNT2 Enhanced Graphics v1.0.0 (US) — `TMNT2_Enhanced_Graphics_v1.0.t2mod`

A switchable graphics remaster for TMNT 2: Battle Nexus (PC, US/NA) that keeps the 2003 cel-shaded
look: crisper toon lighting, soft contact shadows, stylized animated water, and a light bloom/colour
grade. Built on the Debug Tools v1.2.24 / Camera Overhaul v1.0.1 core (`e69ca81`).

**Status: builds, packages and starts; not yet played.** The package passed the T2 Mod Manager's own
package checks, the shaders were render-tested against a reference, and the EXE was start-up tested
under Wine up to the point where it needs your game data. No gameplay frame has been seen yet, and
every look parameter is a first guess that you can change live (see *Settings*).

---

## 1. Switching (for video)

| Key | What it does |
| --- | --- |
| **F8** | ORIGINAL ⇄ ENHANCED. ORIGINAL runs every original code path unchanged. |
| **Shift+F8** | Next preset: Classic+ → Enhanced → High → Custom (switches to ENHANCED) |
| **Ctrl+F8** | Reload `TMNT2_graphics.ini` (keeps the current ORIGINAL/ENHANCED side) |

A yellow line in the bottom-left corner confirms each switch for 2.5 s (`GRAPHICS: ENHANCED (High)`).
Toggling only swaps pointers and switches: it never creates or destroys GPU resources, so it is safe
mid-fight and gives clean before/after cuts.

## 2. What changes

| Area | ENHANCED | How (and why it is safe) |
| --- | --- | --- |
| **Toon lighting** | Narrower light/shadow transition, cool stage-tinted shadows, lifted top band. Hard toon steps stay hard; ink outlines untouched. | RenderWare's toon shader looks lighting up in the `pat01..pat05` gradient textures. I remap copies of those gradients and point the toon paints at them; F8 points them back at the originals. RenderWare's precompiled toon shaders are not touched. |
| **Light rig** | Slightly lower ambient (more contrast), warm key / cool fill on lit models (characters, gimmicks). | Same two lights the game already uses. The prelit world is not affected. If gameplay is driving the lights (e.g. a boss flash), the rig leaves them alone. |
| **Shadows** | Soft-edged blob that widens and fades with height (never vanishes, so landing spots stay readable), plus a dark contact core under the feet that disappears within ~1 unit of jump height; nudged slightly away from the light. | Same blob-shadow system, same ground probe and slope alignment; only the texture, alpha curve and a second small quad differ. |
| **Water** | Animated stylized overlay: two scrolling wave layers (+ a detail layer on High), Fresnel sky reflection, crisp anime specular glints, shallow/deep colour from the stage's own water texture, animated shoreline foam bands, stage fog. | Drawn only on render polygons classified as water (below). Collision, wading, swimming/“dobon”, splashes and ripples are untouched; splashes and ripples still draw on top. Time comes from the game's elapsed time, so it pauses with the game and runs the same at any frame rate. |
| **Post** | Subtle bloom (quarter resolution), colour grade (slight contrast/saturation, cool shadows / warm highlights, light vignette), optional FXAA-lite. | Applied to the 3D scene only, before radar, gauges, text and menus are drawn. |
| **Textures** | Trilinear filtering for textures that have mip levels. | OFF restores the game's own `rwFILTERLINEAR` on exactly the textures it set. |

### How water is found (no blind planes)
Render polygons carry no water flag, so each render material is scored at stage load:
horizontal share (≥ 85 %), share lying over **collision** polygons tagged `water` or `dobon` (the
same tags splashes use), UV animation and texture name. A material becomes water if it is mostly
horizontal and (has a water-like name **or** ≥ 50 % lies over collision water **or** is UV-animated
with ≥ 25 % over collision water). Only those exact triangles are used (welded, subdivided, with the
shoreline taken from the mesh's open edges). Every candidate and the decision are written to
`TMNT2_graphics.log`, and `[Water] Include=` / `Exclude=` in the INI override the decision by texture
name. If a stage's render geometry is unreadable, nothing is drawn unless you opt in to
`CollisionFallback=1`.

## 3. Presets and settings

`TMNT2_graphics.ini` is created next to `TMNT2.exe` on the first run (only when the feature is on).

| Key | Classic+ | Enhanced (default) | High | Range |
| --- | --- | --- | --- | --- |
| `ToonLighting` | 0 | 1 | 1 | 0/1 |
| `ToonContrast` | – | 0.60 | 0.75 | 0–1 |
| `ToonShadowTint` | – | 0.55 | 0.60 | 0–1 |
| `ToonHighlight` | – | 0.35 | 0.50 | 0–1 |
| `LightRig` | 0 | 1 | 1 | 0/1 |
| `KeyLight` | – | 1.00 | 1.04 | 0.5–1.5 |
| `AmbientScale` | – | 0.88 | 0.85 | 0.5–1.2 |
| `LightWarmth` | – | 0.50 | 0.50 | 0–1 |
| `Shadows` | 0 | 1 | 1 | 0/1 |
| `ShadowOpacity` | – | 0.62 | 0.66 | 0.1–1 |
| `ShadowSize` | – | 1.10 | 1.12 | 0.6–1.6 |
| `ContactShadow` / `ContactOpacity` | 0 | 1 / 0.55 | 1 / 0.60 | 0/1, 0–1 |
| `ShadowSunOffset` | – | 0.12 | 0.15 | 0–0.4 |
| `Water` | 0 | 1 | 1 | 0/1 |
| `WaterStrength` | – | 1.00 | 1.00 | 0–1 |
| `WaterReflection` | – | 0.55 | 0.70 | 0–1 |
| `WaterSpecular` | – | 0.80 | 1.00 | 0–1.5 |
| `WaterFoam` | – | 0.70 | 0.80 | 0–1 |
| `WaterWaveScale` / `WaterWaveStrength` | – | 0.18 / 0.35 | 0.18 / 0.35 | 0.02–1, 0–1 |
| `WaterDetail` | 0 | 0 | 1 | 0/1 |
| `Bloom` / `BloomIntensity` / `BloomThreshold` | 0 | 1 / 0.14 / 0.80 | 1 / 0.17 / 0.78 | 0/1, 0–0.5, 0.3–1 |
| `ColorGrade` / `Contrast` / `Saturation` | 0 | 1 / 1.04 / 1.06 | same | 0/1, 0.8–1.3, 0.6–1.4 |
| `SplitTone` / `Vignette` | – | 0.60 / 0.12 | same | 0–1, 0–0.5 |
| `FXAA` | 0 | 0 | 1 | 0/1 |
| `Trilinear` | 1 | 1 | 1 | 0/1 |

`[Graphics]`: `Preset=ClassicPlus|Enhanced|High|Custom`, `StartEnhanced=1`, `Toast=1`, `PerfLog=1`.
`[Overrides]`: any key from the table overrides the active preset (Custom = Enhanced + overrides).
`[Water]`: `Include=`, `Exclude=` (texture names, comma separated), `CollisionFallback=0`.

## 4. Installation

Requirements: retail US/NA `TMNT2.exe` (SHA-256 `b827478461e1…a89564`) and the T2 Mod Manager build
that already installs composable-core packages (the one that runs Debug Tools v1.2.24 and Combo
Assist). **No loader or manager update is needed.**

1. Back up nothing manually — the manager keeps the verified vanilla backup.
2. In T2 Mod Manager, add `TMNT2_Enhanced_Graphics_v1.0.t2mod` (copy it to `C:\TMNT2-CMM\Mod-Packages`
   first if you keep packages there) and enable it.
3. Launch. `TMNT2_graphics.log` and `TMNT2_graphics.ini` appear next to the game EXE.

How it combines with your other packages: this package carries core priority **160** with every
capability of the v1.2.24 core plus `enhanced-graphics`. With Debug Tools, Combo Assist, Karai, MSAA,
widescreen, etc. also enabled, the manager selects this core and each feature stays controlled by its
own package flag (Debug Tools is still off unless its package is on). `TMNT.DAT` is never modified.

## 5. Uninstall / restore

* Disable or remove **Enhanced Graphics** in the manager. It falls back to the next core your other
  packages need (e.g. Debug Tools v1.2.24) or restores the hash-verified vanilla EXE.
* Optional: delete `TMNT2_graphics.ini` and `TMNT2_graphics.log` from the game folder.
* Without the feature flag the code is inert (verified: no log, no INI, `EG feature=off`).

## 6. Performance measurement

With `PerfLog=1`, every 10 s `TMNT2_graphics.log` gets a line like
`PERF mode=ENHANCED preset=Enhanced frames=… fps=… avg_ms=… max_ms=… eg_cpu_ms=… gpu_water_ms=… gpu_post_ms=…`.
GPU times come from Direct3D 9 timestamp queries read back a few frames later (never stalls). A mode
change starts a fresh window, so press F8, wait 10 s in the same spot, press F8 again, and compare.
No FPS numbers are claimed here: nothing has been measured on a GTX 1660 yet.

Expected cost (design, not measured): 1 full-resolution copy + 3 quarter-resolution passes + 1
full-resolution composite for post; one alpha-blended pass over the water triangles; two small quads
per shadow; zero extra cost for toon/light changes.

## 7. Building

* **Reference build (MSVC, your CI):** branch `claude/enhanced-graphics-v1-0-0-us`, workflow
  `.github/workflows/build-enhanced-graphics-v1-0-0-us.yml` (same toolchain staging as the camera
  build; `-DOPT_DEBUG_TOOLS=ON`). Then package with
  `python tools/EnhancedGraphics/pack_t2mod.py TMNT2_Core_Enhanced_Graphics_v1_0_0_US.exe TMNT2_Enhanced_Graphics_v1.0.t2mod`
  (`pip install brotli`).
* **Cross build (used for the delivered package):** `tools/EnhancedGraphics/mingw/build.py` builds the
  same sources, defines and RenderWare/CRI libraries with MinGW-w64 i686 (MSVC struct layout, 4-byte
  callback stack alignment, SSE2 math, a few MSVC CRT helper shims).
* **Shaders:** HLSL sources in `src/System/PC/Shaders/EnhancedGraphics/`; `tools/EnhancedGraphics/build_shaders.py`
  compiles them to SM3 bytecode (`PCEnhancedGraphicsShaders.inl`, committed). Build `hlslc.exe` from
  `hlslc.c` first; it calls `D3DCompile` (Windows, or Wine on Linux).

## 8. Change log

**v1.0.0**
* New feature flag `enhanced-graphics` (`CPCModFeatures`), module `CEnhancedGraphics` (presets, INI,
  F8 hotkeys, toast, logging, GPU timing, device-loss handling via RenderWare release/restore callbacks).
* Toon gradient remap (`ToonGradient`, `CToonManager`), stage-aware shadow tint, light rig (`CWorldMap`).
* Soft contact shadows (`CCircleShadowModule`).
* Water classification + overlay mesh (`WaterSurface`, `WaterSurfaceMesh`) and water shader.
* Post chain: bloom, grade, FXAA-lite; trilinear option (`CTextureManager`).
* Draw-order hooks in `CGameStage::Draw` (water after map clump models; post before radar/gauges).

## 9. Test results

| # | Check | Result |
| --- | --- | --- |
| 1 | All 576 game/system translation units type-check with the changes (MinGW) | PASS (2 pre-existing MinGW-only header gaps, identical on the untouched base) |
| 2 | Full EXE builds and links (MinGW, 606 TUs incl. ImGui) | PASS |
| 3 | CPU unit tests: gradient remap (5 shapes, hard steps stay hard, monotonic), water mesh (seam welding, shoreline, depth field, area, vertex budget, degenerate input) | PASS |
| 4 | Shader render tests on a real D3D9 implementation (Wine wined3d) vs. independent C++ reference: bright, blur H/V, composite OFF math (bit-exact pass-through), Enhanced, High/FXAA, water ×3 | PASS (FXAA: 2 of 12,288 pixels on threshold ties) |
| 5 | Start-up in the game process (Wine, no game data): crash reporter, feature flags, INI, audio, video mode, D3D9 device, RenderWare engine start, Enhanced Graphics shader/texture creation on the game's device, first frame | PASS (`post=ready water=ready`) |
| 6 | Feature off (no feature file): module inert | PASS |
| 7 | Package: manager code opens/validates manifest, verifies payload, rejects a non-retail EXE, rebuilds the core EXE byte-for-byte (format test) | PASS |
| 8 | Manifest schema identical to Debug Tools v1.2.24 and Combo Assist packages; capabilities superset | PASS |
| — | Gameplay: outlines, lighting, shadows following characters, water on real stages, splashes/ripples, HUD readability, presets in play, device reset / resolution change, FPS on GTX 1660, install/uninstall in your manager | **NOT VERIFIED** (needs your game data and PC) |

## 10. Limitations and known issues

* **Not played.** All look values are untuned first guesses.
* **Compiler:** the delivered core EXE is a MinGW cross-build (your CI's MSVC toolchain was not
  reachable from here). It uses the same sources and libraries, but it is a different compiler than
  your previous cores; if anything unrelated to graphics behaves differently, build the MSVC version
  with the included workflow and repackage.
* **Not implemented:** SSAO (Direct3D 9 cannot read the depth buffer RenderWare renders into without
  replacing its depth surface, and that conflicts with MSAA); rim lighting (needs a view-dependent
  term inside RenderWare's precompiled toon shader); character-shaped shadow maps; water refraction and
  planar reflections; anisotropic filtering (needs RenderWare's anisotropy plugin, which the current core does not attach).
* The light rig scales the stage lights; how strongly RenderWare's toon shader responds to light colour
  (vs. only the gradient) is not known until seen.
* Water detection is heuristic. Check `TMNT2_graphics.log` on water stages and use `Include`/`Exclude`.
* If changing the video mode restarts the RenderWare engine, the water overlay comes back at the next stage load (device resets alone keep it).
