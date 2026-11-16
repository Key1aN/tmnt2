# TMNT2 Turtle Companions v1.0.2 + Enhanced Graphics fix (Debug Tools v1.2.27, US)

Package `Debug_Tools_Turtle_Companions_v1.2.27_US.t2mod`, core priority **162** (beats Enhanced
Graphics 160 and v1.2.26 161), core EXE SHA-256
`3b3acf8bf57f1ec9c2f10dbec95980bd8ef2b44a7ee1164d00c757bafbe9e769`. Source
`claude/turtle-companions-v1-0-1-eg-us` (commit after `e364bdd`). Same MinGW i686 build as Enhanced Graphics.

## Fixes

| Your report | Cause | Fix |
| --- | --- | --- |
| Enhanced Graphics makes the game very white (your screenshots) | RenderWare's render-state cache starts every entry at `0xFFFFFFFF` ("never set"). The Enhanced Graphics water and post passes save states, change them, and restore them. For `D3DRS_SRGBWRITEENABLE` (never used by the game) they restored `0xFFFFFFFF`, which a real GPU reads as ON: every later draw, the HUD and the F4 menu included, was gamma-encoded a second time (your screenshot pairs fit a gamma of about 0.45; even the menu title bar went 20 -> 79). Wine's software renderer ignores sRGB writes, so every earlier test missed it. | Unknown cache entries are restored to the Direct3D 9 default (sRGB write OFF); same for sampler states and the fog read. This bug was in every Enhanced Graphics version; v1.2.27 carries the fixed graphics code (Enhanced Graphics v1.1.3 code). |
| Companions don't work | You were in a Battle Nexus arena (stage 69); v1.0.0/1 left Battle Nexus and Combat Lab one-turtle on purpose. | TEAM > **Battle Nexus and Combat Lab** = ON (default): the team is on the field there too. |

## Install

Add `Debug_Tools_Turtle_Companions_v1.2.27_US.t2mod` in T2 Mod Manager (replaces 1.2.26), keep
Enhanced Graphics 1.1.2 enabled. The launch line in the manager log should show `3b3acf8b…`, and
the game trace `BUILD TMNT2_Core_Turtle_Companions_v1_0_2_EG_v1_1_3_US`.

Note: the standalone Enhanced Graphics 1.1.2 package still has the white-screen bug; it is only fixed
while this Debug Tools package is enabled (its core wins).

## Test results

| Check | Result |
| --- | --- |
| RenderWare library disassembly: `_rwD3D9RenderStateOpen` fills the cache with `0xFFFFFFFF`; `RwD3D9GetRenderState` returns the cache entry | CONFIRMED |
| Test build, without the fix, Wine, your `TMNT.DAT`: after the passes RW's cache holds `SRGBWRITEENABLE = 0xFFFFFFFF` (sent to the GPU) | REPRODUCED |
| Same with the fix: cache holds `0` | PASS |
| Combat Lab (Nexus arena) with the default settings, no test switch: team of 4 created, 11 control switches in 2 min, no crash | PASS |
| Manager core choice on your enabled packages: 1.2.27 (162) selected | PASS |
| T2DELTA payload re-applied to retail `TMNT2.exe` rebuilds the core byte for byte | PASS |
| On your GTX 1660 Ti: the Enhanced look no longer washed out | **NOT VERIFIED** (needs your PC) |
