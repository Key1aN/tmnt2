# TMNT2 Enhanced Graphics v1.1.2 (US) — `TMNT2_Enhanced_Graphics_v1.1.2.t2mod`

Water and shadow fix release, from your Leatherhead (m42nb) and Nexus arena (m60x) feedback.

## Changes

| Problem (your report / screenshot) | Cause | Fix |
| --- | --- | --- |
| Far, dark end of the Leatherhead sewer turned bright with stylized water ON | The overlay painted its own colour (sky reflection + base colour) with normal alpha blending, ignoring how dark the stage is there | The overlay now **multiplies** the game's own water (modulate 2x): 0.5 = unchanged. Waves, glints, sky sheen and foam brighten or darken the stage's water texture *relative to its lighting*, so dark areas stay dark and murky water stays murky. The game's default water textures are kept and stay visible (debris under the sewer water shows through again). Far water fades slightly darker with distance. |
| Foam too strong | Default 0.70 | **Shore foam default 0.10** (High preset 0.25). |
| Waves too busy for TMNT2's calm or still water | Default wave strength 0.35 and fast drift | Wave strength **0.20**, drift speed halved, waves calm down with distance (no far tiling shimmer). |
| (from the m41n log) a tile floor under the sewer's animated water got a second overlay | Static floor accepted for lying over collision water | When a stage has real (named or animated) water, wet floors are skipped. |
| Nexus: soft shadow of enemies in the pool sits behind them, even with offset 0 | v1.1.1 lifted the shadow 0.40 straight up so it would not be hidden by the water sheets; seen from the angled camera a lifted shadow appears shifted backwards | The shadow now sits on the water surface directly under the character and gets the same depth margin by moving toward the camera **along the view ray**, which does not change where it appears on screen. Shadow quads are no longer back-face culled (water collision polygons can face down). |
| You asked to see the water textures | — | New REMASTER row **"Export stage water textures"** (A / Enter): writes the current stage's water textures as .tga into `TMNT2_water_textures\` next to the game (detected water + every water-named or animated candidate). |

## Install

Remove/disable v1.1.1 in T2 Mod Manager, add `TMNT2_Enhanced_Graphics_v1.1.2.t2mod` from
`C:\TMNT2-CMM\Mod-Packages`. If you saved water edits in `TMNT2_graphics.ini`, use *Clear edits* once so
the new defaults (foam 0.10, waves 0.20) apply. Core EXE SHA-256
`67cad87c6a3e55d90563a4a51450858171734daede52ca9706ebd6b6f9b6c5c3`.

## Test results

| Check | Result |
| --- | --- |
| Full MinGW release build (forced full rebuild) | PASS |
| CPU unit tests | PASS |
| Shader render tests incl. the new modulate water shader against an independent C++ reference | PASS |
| Manager code: T2Delta.ApplyAsync on your genuine retail TMNT2.exe rebuilds the core byte-for-byte | PASS |
| In game, Leatherhead stage m42nb with your TMNT.DAT (Wine): water found via the `suimen` stage models, far end stays dark, game texture kept | PASS (`leatherhead_original_vs_v112.png`) |
| In game, Nexus m60x: modulate water, no crash; texture export writes all 8 water textures | PASS |
| Shadow alignment on water | **Not verifiable here**: under Wine's software renderer the in-water shadows are not visible in any version (v1.1.1 included), while you see them on your PC. The fix is geometric (same depth margin as v1.1.1, no screen shift); please confirm in game. |

Test-only changes used in the Wine runs (sound-loader guard, a Leatherhead Combat Lab arena) are not in
the shipped EXE.

## Notes

* `m60x_water1` is the pink sky/cloud backdrop, not water (its name contains "water"); it is correctly
  not used.
* Waterfalls and sloped water keep their original look.
