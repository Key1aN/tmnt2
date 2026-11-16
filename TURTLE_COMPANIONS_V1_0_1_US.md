# TMNT2 Turtle Companions v1.0.1 (Debug Tools v1.2.26, US)

Fix release: v1.2.25 never ran on your setup. Each package carries a whole game EXE and T2 Mod
Manager runs only the highest-priority one that supports every enabled mod. Enhanced Graphics
v1.1.2 (priority 160) won over Turtle Companions v1.0.0 (151), so there was no F9 and no TEAM tab.

v1.0.1 is the same Turtle Companions code built on top of the Enhanced Graphics v1.1.2 source,
in one EXE:

- Package `Debug_Tools_Turtle_Companions_v1.2.26_US.t2mod`, core priority **161**, capabilities =
  Enhanced Graphics core's list (incl. `enhanced-graphics`). Core EXE SHA-256
  `da78e3c4a1d518fac67436a956417249ed6f17bc16da917942379e534d43c35b`.
- Debug Tools tabs: ... OVERHAUL, **TEAM**, REMASTER (REMASTER stays last; it only shows while the
  Enhanced Graphics package is enabled). Hotkeys: F6 combat, F7 camera, F8 graphics, **F9 companions**.
- Graphics stay off unless the Enhanced Graphics package is enabled, as before.
- Source: `claude/turtle-companions-v1-0-1-eg-us` = `claude/enhanced-graphics-v1-1-2-us` +
  merge of `claude/turtle-companions-v1-0-0-us`. Built with the same MinGW i686 cross-build as
  Enhanced Graphics (`tools/EnhancedGraphics/mingw/build.py`, GCC 13 win32 threads).

Feature behaviour, TEAM tab rows and limits: see `TURTLE_COMPANIONS_V1_0_0_US.md` (unchanged).

## Install

In T2 Mod Manager add `Debug_Tools_Turtle_Companions_v1.2.26_US.t2mod` (replaces 1.2.25) and keep
Enhanced Graphics 1.1.2 enabled. After applying, the manager log's launch line should show SHA-256
`da78e3c4…`.

## Test results

| Check | Result |
| --- | --- |
| Enhanced Graphics source rebuilt from the v1.0.0 bundle + v1.1.0/1.1.1/1.1.2 patches; MinGW build is byte-identical to your installed Enhanced Graphics v1.1.2 EXE (`67cad87…`) | PASS |
| Merge with Turtle Companions (one conflict file, Debug Tools page lists; TEAM placed before REMASTER), full build 608 TUs | PASS |
| Manager core choice simulated with the manager's own rule (highest priority supporting all enabled flags) on your actual enabled packages + 1.2.26: picks 1.2.26 (161) over Enhanced Graphics (160) | PASS |
| T2DELTA payload re-applied to your retail `TMNT2.exe` rebuilds the core byte for byte | PASS |
| Real game under Wine with your `TMNT.DAT` and your feature flags (incl. enhanced-graphics), test build allowing the team in Combat Lab: four turtles, Enhanced Graphics water/toon active, companions fighting, 11 switches, no crash; F4 shows v1.2.26 with TEAM and REMASTER tabs | PASS |
| Story stage, F9 / OFF path, physical pad | **NOT VERIFIED** here |
