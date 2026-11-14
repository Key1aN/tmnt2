# TMNT2 Camera Overhaul v1.0.0 (Debug Tools v1.2.23, US)

A reworked gameplay camera with a RETAIL / ENHANCED switch, on top of Combat Overhaul v1.1.5 /
Debug Tools v1.2.22 (`88d64b9`). Status: **compiles in CI; not yet played**. Every number is a
first guess made without seeing the game; the important ones can be changed live in the CAMERA tab.

## Switching (for video)

| What | How |
| --- | --- |
| Enhanced camera / retail camera | **F7** at any time, or Debug Tools (F4) > **CAMERA** > Enhanced camera |
| Live readout | The Enhanced camera row shows the current pull-back, view widening and how many players are outside the safe area |
| Amounts | CAMERA tab rows under it: Max zoom-out, Safe area, Jump window, Look-ahead, Boss pull, Reset |

The game starts on the enhanced camera. With it OFF, or with the Debug Tools mod switched off in
the manager, the retail camera code runs unchanged (checked bit for bit, see Validation).

## What was wrong with the retail camera

| # | Problem | Where |
| --- | --- | --- |
| 1 | Zoom never gets far enough: the zoom request is measured through the already zoomed camera but applied to the authored distance, so players settle outside the frame. It also only reacts after someone is past the edge | `CMapCamera::CalcNiceZoom`, `UpdatePathCamera` |
| 2 | The co-op movement boundary is a circle as wide as the view, so a player walking toward the camera leaves the bottom of the screen before the boundary stops them. It also grows with zoom | `CGameProperty::AdjustWithinMoveBoundary` |
| 3 | A dead player is reported at the map origin and drags the camera there | `CGamePlayer::GetPosition`, both updaters |
| 4 | The look-at is the average of the players; three together and one apart pushes the fourth out | `CDefaultCameraUpdater::Update` |
| 5 | Set (`t`) and fixed (`f`) camera areas ignore zoom completely | `UpdateSetCamera`, `UpdateFixedCamera` |
| 6 | The boss camera never zooms and looks at the midpoint, so past about one screen width both the player and the boss are off-screen | `CBossBattleCameraUpdater::Update` |
| 7 | Camera smoothing is per update and the game updates at the monitor refresh rate: about 2.4x snappier at 144 Hz than at 60 | `LerpFactor` and the bare `0.1f` lerps |
| 8 | Every jump moves the whole picture, including the landing spot | look-at follows body height directly |
| 9 | Fixed cameras snap the field of view while the eye is still gliding | `UpdateFixedCamera` |

## What the enhanced camera does

| # | Change | Numbers |
| --- | --- | --- |
| 1 | Path cameras: the needed pull-back is solved directly against the authored camera position, for the head and feet of every player | safe area 0.80, max zoom-out 1.60x |
| 2 | Pulls back fast, returns slowly after a hold, and does nothing in between (no pumping) | out 6/s, in 1.5/s after 0.5 s, return threshold 0.66 |
| 3 | Look-at is the centre of the players' bounds; dead players are ignored; with nobody left the camera stays put | - |
| 4 | Set and fixed cameras keep their authored position and widen the view instead | up to 1.25x |
| 5 | When a wall blocks the pull-back, the eye stops 0.3 short of it and the view widens for the rest | up to 1.25x |
| 6 | Co-op movement boundary = what the camera can show at max zoom-out, feet included; it no longer grows with zoom | fixed-camera areas keep the retail value |
| 7 | Boss fights: players are always framed; the boss pulls the camera by a limited amount and is framed with the zoom that is left | pull up to 0.45, range 13, boss margin 0.92 |
| 8 | Jumping: the look-at height holds during a jump, follows falls at once, follows up only past the jump window | window 1.2 |
| 9 | Single player: slight look-ahead in the run direction | up to 0.8 |
| 10 | All smoothing is time based; identical to retail at 60 Hz. The fixed-camera view snap becomes a glide | - |
| 11 | Widescreen aware: uses the real horizontal view, so 16:9 needs less pull-back than 4:3 | - |

Scripted cameras (stage intro, teleport cuts, Nexus enemy introductions, lair, ride stages, the
Debug Tools manual/showcase camera) are untouched.

## Where the code is

- `src/Game/System/Map/MapCamera.{hpp,cpp}`: `TUNING` (defaults in `s_tuningDefault`), the framing
  solver (`CalcFrameDollyZoom`, `CalcFrameExtent`, `CalcFrameReach`, `StepFrameZoom`), `UpdateFraming`,
  `IsEnhanced` (master switch and Debug Tools gate), time-based `LerpFactor`.
- `src/Game/Component/GameMain/GameStageCameraUpdater.{hpp,cpp}`: `CPlayerFrameTracker`, enhanced
  default and boss updaters; the retail bodies are kept verbatim as `UpdateOriginal`.
- `src/Game/Sequence/DebugTools/DebugToolsProcess.cpp`: CAMERA tab rows and F7.
- `src/Game/Sequence/Test/DebugMenuProcess.cpp`, `GameStageDebug`: debug-build menu rows and the
  on-screen diagnostics (debug builds only).

## Validation

- The real camera sources were linked against stub engine functions and run for 36,000 randomised
  updates (1-4 players, both updaters, all area types, walls, a death, a cut, 30/60/144 Hz). With the
  switch OFF the result is bit-identical to the unmodified sources.
- Same harness, one synthetic scene (screen position, 1.00 = edge):

| Scenario | Retail | Enhanced |
| --- | --- | --- |
| Two players 12 apart, settled | 1.10 | 0.77 |
| Three together, one 10 away | 1.22 | 0.77 |
| Player on the co-op boundary, toward the camera | 1.33 | 0.81 |
| Boss fight, 14 apart (player / boss) | 1.40 / 1.42 | 0.72 / 1.02 |
| Set or fixed camera, players 14 apart | 1.01 | 0.81 |
| Look-at travel during a normal jump | 1.35 | 0.20 |
| Eye follow time at 30 / 60 / 144 Hz | 0.57 / 0.28 / 0.12 s | 0.30 / 0.28 / 0.29 s |

This checks the maths and the state machine, not how real stages are authored or how it feels.

## Known limits and open questions

- Nothing here has been run in the game.
- The co-op boundary changed size: larger than retail at rest, smaller than retail fully zoomed out.
  If a spot that needs players far apart is now blocked, raise Max zoom-out.
- Height differences are not limited by the boundary, so a player far above or below the group can
  still leave the frame. There is no off-screen player marker in the game to fall back on.
- Set and fixed cameras can only widen by 25%; a player behind an authored camera stays hidden.
- Camera areas are chosen from the new look-at. In lopsided 3-4 player groups it can sit several
  units from the retail average, so an area may switch at a slightly different moment.
- The 1.5 s lock after entering a path area is unchanged.
- If characters look too small in co-op even at Max zoom-out 1.00, the cause is the separately
  authored multiplayer camera paths (`_m`), which this version does not change.
- Settings are not saved between runs.

## Test checklist

1. F7: the Enhanced camera row flips; with OFF everything below behaves as retail.
2. Single player: run, dash, reverse. The readout stays at zoom 1.00x. If reversals feel floaty, lower Look-ahead.
3. Single player platforming: small jumps keep the ground still; a long fall is followed at once. If tall jumps feel late, lower Jump window.
4. Two players: walk apart sideways, then one toward the camera. Both stay visible up to the boundary, feet included.
5. Four players: spread out, then regroup. Half a second of hold, then a slow return, no pumping.
6. One player on a high platform, the others below (the weak case).
7. Boss fights: one that moves a lot, one that is large. The player never leaves the frame.
8. Walk back and forth across camera area borders with players spread out: no zoom or view pops.
9. Spread out under a low ceiling: the camera stops short of it and the view widens slightly.
10. Stage intro, a mid-stage teleport, a Nexus enemy introduction, the lair and a ride stage: unchanged.
11. If you can, compare 60 Hz with a higher refresh rate: enhanced feels the same, retail gets snappier.
