# TMNT2 Enhanced Graphics v1.1.1 (US) — `TMNT2_Enhanced_Graphics_v1.1.1.t2mod`

Fix release for "most settings don't change anything, the water doesn't change (Nexus arena)".
Everything in v1.1 (REMASTER tab, presets, INI, F8) stays the same; this release fixes why most
settings had no visible effect.

## What was wrong (measured in game, A/B with the menu paused so the camera does not move)

| Area | Cause | Fix |
| --- | --- | --- |
| **Water (all stages)** | The overlay read the camera from `D3DTS_VIEW/PROJECTION`, which RenderWare's D3D9 driver never sets (it keeps them in `_RwD3D9D3D9ViewTransform/ProjTransform` for its shaders). The matrices were identity, so the water was drawn off screen on every stage since v1.0 (GPU cost 0.02 ms = nothing drawn). | Uses RenderWare's own camera matrices. Water now draws (≈2 ms on a software GPU, far less on a 1660 Ti). |
| **Water (Nexus arena)** | The pool is not world geometry: it is drawn as separate alpha/UV-animated stage models (`water1/2/3`, material `m60x_colosseum_water1`). v1.0/v1.1 only scanned world polygons. v1.1 also dropped the pool floor as "not water". | Stage models are now scanned too; stacked layers of the same pool are merged into one overlay (no double drawing). Log: `WATER map model …`, `layer skipped`, `mesh ready`. |
| **Toon lighting** | The ramp edit reached the GPU (verified with a red test ramp) but was too small to see, even at maximum (largest change about 10%). | Contrast now deepens the shadow side up to about 45% and moves the split; tint is stronger; "Highlight" (now *Lit-side width*) lifts mid tones. Default Enhanced changes the ramp by up to 62/255. |
| **Light rig (Ambient / Key / Warmth)** | No effect at all: TMNT2's cel shader ignores light colour (it only uses the light direction) and the world is prelit. | The rig now acts on the character toon ramps: shadow level, lit level and warm/cool split (needs Toon lighting ON). |
| **Shadows in water** | Shadows were drawn on the pool floor, under the water sheets, so they were hidden. | In water the shadow is placed on the game's water surface (the height its ripples use) plus a measured lift. |
| Bloom / grade / vignette / FXAA | Were working (clearly visible at higher values); defaults are deliberately subtle. | Unchanged. |
| Trilinear | No effect on stock content: the game's textures have no mip levels. | Unchanged (menu says so). |

## Install

1. In T2 Mod Manager remove/disable Enhanced Graphics v1.1, add `TMNT2_Enhanced_Graphics_v1.1.1.t2mod`
   from `C:\TMNT2-CMM\Mod-Packages`, enable it (same id `Enhanced_Graphics`, version 1.1.1).
2. Keep Debug Tools enabled for the REMASTER tab. Your `TMNT2_graphics.ini` still works; if you saved
   edits made while the light rig did nothing, use *Clear edits* once.

Core: priority 160, same capabilities as v1.1. Core EXE SHA-256
`262608a7a605cf56e9e590ac76f4b703cbb710f369099d724ac1a755c0fd2aa5`.

## Test results

| Check | Result |
| --- | --- |
| Full MinGW build, stripped release from clean sources | PASS |
| CPU unit tests (new: light rig on the ramp, visible default change) | PASS |
| Shader render tests | PASS |
| Manager code: T2Delta.ApplyAsync on your genuine retail TMNT2.exe rebuilds the core byte-for-byte | PASS |
| In game (your TMNT.DAT, Nexus arena m60x, Wine): pool found via stage models, overlay drawn and visible (A/B), toon/light visible (A/B), shadows visible in the pool, no crash | PASS (screenshots in `v1.1.1-test-results/`) |
| Other water stages, your GTX 1660 Ti timings, a physical pad | NOT VERIFIED here |

The Wine runs used a test-only sound-loader guard that is not in the shipped EXE.

## Limitations

* Waterfalls and sloped water keep their original look (only flat water gets the overlay).
* Water detection is still heuristic: check `TMNT2_graphics.log` (`WATER …` lines) on a stage whose
  water does not change, and use `[Water] Include=` with the texture name shown there.
* SSAO, rim light, character-shaped shadows, refraction and anisotropic filtering are not implemented.
