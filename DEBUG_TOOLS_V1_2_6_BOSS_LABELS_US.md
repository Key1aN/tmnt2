# TMNT2 Debug Tools v1.2.6 Boss Labels US

## v1.2.6 changes (on top of v1.2.5 below)

- Move labels now use the game's scalable font instead of the fixed 8x16 debug
  font: default height 16 virtual units (about 36 px at 1080p, versus 16 px).
- Labels are anchored at the top of the enemy (position + collision height +
  0.10) with the status/HP line directly above the head and the move name above
  that, both with a drop shadow, instead of floating about 36 px higher.
- New Telemetry page items: **Move label size** (10-32, default 16) and
  **Move label height above head** (-1.00 to +1.50, default +0.10).
- The build workflow publishes outputs to `build-drops/debug-tools-v1-2-6-boss-labels-us`
  with line-ending conversion disabled, so MAP/MD hashes match the CI checksums.

Everything else is identical to v1.2.5.

# v1.2.5 Perf Telemetry (included)

Target: TMNT 2: Battle Nexus, US/NA 32-bit PC.

Built on Debug Tools v1.2.4 (horizontal `lZ` fix, not yet hands-on accepted).
EXE-only, composable T2 Mod Manager feature (`debug-tools-f4`). It does not
contain, rebuild, or replace `TMNT.DAT` or any LPAC entry.

## Why the old FPS was always ~60

`CScreen::Framerate()` is the configured refresh rate divided by the vsync
interval. It is fixed at startup and never measures anything, so the Debug
Tools panels showed 60 FPS during slowdowns.

The game runs one fixed-step tick per main-loop iteration
(`Move -> Render -> Flip`). When drawing is slow, the retail frame-skip
controller skips up to five draws to keep game speed; when a tick itself is
slow, the whole game slows down. Both are now measured.

## Real frame timing

`CPCFrameStats` (Debug Tools builds only) timestamps every main-loop iteration
with `QueryPerformanceCounter` and records:

- frame time (end of previous frame to end of this frame);
- update (`Framework().Move()`), render (`Framework().Render()`), and present
  (`Framework().Flip()`, including vsync wait) times;
- whether the frame-skip controller skipped drawing.

Displayed FPS counts only frames that reached the screen. **Speed** is the
tick rate divided by the nominal 60 Hz (100% = full game speed).

## Frame-time graph (top middle, default ON)

120 bars of measured time between displayed frames, 0-50 ms scale:
green at or under 17.6 ms, yellow up to the drop threshold, red above it.
A purple mark on top means draws were skipped before that frame. The green
line is the 60 FPS target; the red line is the drop threshold. The text row
shows shown FPS, latest frame time, 1-second worst frame, game speed, and the
number of drop episodes this session. With the menu open below 1400 px wide,
the graph moves to the top right so it does not cover the menu.

## Frame-drop log (default ON, threshold 20 ms)

`TMNT2_perf.log` beside `TMNT2.exe` (shown by the T2 Mod Manager Logs page and
included in Create Report). Per slowdown episode:

- `DROP START` with the frame time, update/render/present split, draws skipped
  before it, 1-second shown FPS / speed / worst frame, sequence, stage, tick,
  enemy count, P1 character and motion, then one line per boss with its motion,
  AI status, and HP;
- `DROP CONT` with the same context every 60 slow frames during long slowdowns;
- `DROP END` after 30 good frames: slow-frame count, worst, average, skipped
  draws, tick duration.

Loading gaps over one second, sequence changes, and the first 45 frames of a
sequence are ignored.

## Move telemetry and labels

- **Move labels** (default BOSSES; OFF / BOSSES / ALL ENEMIES): the current
  animation name above the enemy, with name, AI status, and HP below it. Boss
  labels are gold.
- **Move log** (default ON): `TMNT2_moves.log` records every motion or AI-status
  change for bosses (and every enemy when labels are ALL), plus every P1 motion
  change, with tick, slot, status, and HP.
- The Telemetry panel shows real FPS, the timing split, drop counters, and the
  eight most recent tracked moves.

## Telemetry page

| Item | Default |
| --- | --- |
| Show telemetry when closed | OFF |
| Copy game screenshot to clipboard | action |
| Frame-time graph (top) | ON |
| Log frame drops | ON |
| Frame drop threshold | 20.0 ms (Left/Right 0.5 ms, PgUp/PgDn 2.5 ms, 17-100 ms) |
| Move labels over enemies | BOSSES |
| Log enemy/player moves | ON |
| Move label size | 16 |
| Move label height above head | +0.10 |

Both logs are buffered, flushed at least every 0.5 s and at each drop event,
and restarted when larger than 8 MB.

## Preserved behavior

All accepted v1.2.3 and v1.2.4 controller/camera behavior, pages, MSAA, Difficulty
Lab, hitboxes, and gameplay are unchanged. Retail builds without
`TMNT2_DEBUG_TOOLS` compile the original main loop.

## Unique build identities

- Branch: `claude/debug-tools-v1-2-6-boss-labels-us`
- Workflow: `build-debug-tools-v1-2-6-boss-labels-us.yml`
- Artifact: `TMNT2-Debug-Tools-v1.2.6-Boss-Labels-US-run-N`
- EXE / PDB / MAP: `TMNT2_Debug_Tools_v1_2_6_Boss_Labels_US.{exe,pdb,map}`

Status: implemented; requires hands-on testing in the separate test install.
