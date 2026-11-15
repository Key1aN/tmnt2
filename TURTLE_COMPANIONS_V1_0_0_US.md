# TMNT2 Turtle Companions v1.0.0 (Debug Tools v1.2.25, US)

AI-controlled turtle companions and instant character switching, on top of Camera Overhaul v1.0.1 /
Combat Overhaul v1.1.5 / Debug Tools v1.2.24 (`e69ca81`). Status: **builds in CI; ran in the real game under Wine (Combat Lab test build); not yet played on your PC**.
Every number is a first guess and can be changed live in the new **TEAM** tab.

## What it does

- **The whole team is on the field.** In a single-player story stage, every turtle of the team
  appears at the stage start. You control one; the others are run by an AI.
- **Instant switching.** Change Character (L2 / LT by default) moves your control to the next turtle
  on the field at once, from any state: mid-combo, mid-air, even while being hit. The turtle you
  leave carries on under AI. "YOU: RAPHAEL" shows for a moment and the retail change flash plays on
  the new turtle.
- **Team.** By default the turtles you did not pick at character select join as companions, up to
  four characters. TEAM > Team = *Selected only* uses just the characters you picked.

## What the companions do

| Situation | Behaviour |
| --- | --- |
| Nothing to fight | Follow you in a formation behind you (left, right, centre) at the Follow distance; start moving when 1.4 out of place, stop at 0.6 |
| Enemies near you | Fight enemies within the Fight radius **of you** (never a fight of their own). Each picks the nearest enemy, spreading out (an enemy already taken by another companion counts as 3 units further). Retargets every 0.4 s, sticky unless a target is clearly better |
| In reach | Real combo strings with the retail chain: AABBB, AABBC, AAB, AAC, AA or a strong B. Bosses get shorter strings. Each hit is steered at the target. Pause between strings about 0.45 s / Aggression |
| Enemy attack starting next to them, facing them | Guard for 0.5 s (bosses 0.7 s) with the Guard skill chance, one roll per attack |
| Enemy 5-15 units away | Shuriken now and then (own shuriken count, every 2.5 s at most) |
| Knocked down, grabbed, stunned | Mash recover / quick rise |
| You are higher up (ledge, platform) | Jump and double jump toward you |
| Stuck while far from you | Jump after 1 s, teleport after 2.5 s. Stuck near you (follow spot behind a wall or over water): just stay. Target out of reach for 2 s: pick another |
| Further than the Teleport distance, or 4+ units above/below you for 3 s | Reappear behind you with the blink effect, once you are on the ground |
| Carrying an enemy | Throw it |

## Rules kept at single player

The team is built on the game's own co-op path (each turtle is its own game player), so enemies,
items, gimmicks, the HUD and the camera all know about every turtle. To keep the game balanced as
single player:

- Team health stays the 1-player maximum (shared health gauge, as in co-op).
- Boss health, the small heal item and the Self Recovery / Poison codes use the 1-player values.
- No co-op movement leash: you can always walk away; companions catch up.
- The camera uses the single-player camera paths and frames **you**; companions are kept in frame
  only while they are within "Camera keeps companions" (9 units, hysteresis 1.3x).
- "1P" is drawn over the turtle you control; a companion only gets the coloured arrow while it is
  off screen.
- A companion that falls off the map is put back without costing health.
- "Companion damage taken" (100% default) scales damage that lands on a companion.
- Stage results (MVP, technique points) count only the characters you picked; companions that
  joined from the fill-in are not ranked.

Companions are **not** used in co-op, Battle Nexus, vehicle (ride) stages, the lair, Combat Lab,
demos, or with the Debug Tools mod disabled in the manager: those stages are built exactly as retail.

## Switching (for video)

| What | How |
| --- | --- |
| Companions ON / OFF | **F9** at any time, or Debug Tools (F4) > **TEAM** > AI companions. Takes effect at the next stage start or retry; a badge confirms |
| Instant switching ON / OFF | TEAM > Instant switching. OFF = retail rule |
| Take control of a turtle | Change Character, or TEAM > select a turtle's row and press A / Enter |
| What each companion is doing | TEAM > team rows: YOU / AI - FOLLOW / FIGHT / GUARD / CATCH UP / RECOVER / JUMP |

With companions OFF, Instant switching still lets the retail one-turtle-at-a-time swap happen while
walking, running, dashing, guarding or attacking (retail: standing still only). The turtle that
leaves is put back to idle first; swapping in the air is not allowed (the next turtle would start
idle in mid-air).

## TEAM tab

| Row | Default | Range |
| --- | --- | --- |
| AI companions (F9) | ON | next stage |
| Instant switching | ON | live |
| Team | All four turtles | next stage |
| Switch effect | ON | live |
| Companions throw shuriken | ON | live |
| Aggression | 1.00x | 0.25 - 2.00 |
| Guard skill | 50% | 0 - 100 |
| Follow distance | 3.0 | 1.5 - 8.0 |
| Fight radius | 10.0 | 4 - 20 |
| Teleport distance | 18.0 | 8 - 40 |
| Companion damage taken | 100% | 0 - 100 |
| Camera keeps companions | 9.0 | 0 - 20 (0 = you only) |
| Team rows (4) | live state | A / Enter = take control |
| Reset companion AI amounts | - | - |

Also: the keyboard lock-on key (T) now locks for the turtle you control.

## Where the code is

- `src/Game/Component/TurtleCompanions/TurtleCompanions.{hpp,cpp}`: team setup, AI, switching,
  menu rows. Own random generator, so the game's random sequence is untouched.
- Hooks (each inside `TMNT2_DEBUG_TOOLS`): `Manipulator.cpp` (input port: human pad or AI),
  `Player.cpp` (switch input, idle before a retail instant swap), `PlayerCharacter.cpp` (retail
  switch rule, companion damage), `GameStage.cpp` (team creation, play context, lifecycle),
  `StageSeqState.cpp` (loads the fill-in turtles, single-player camera paths), `GameProperty.cpp`
  (no leash), `GameStageCameraUpdater.cpp` (camera targets, both enhanced and retail camera),
  `GaugeStatus.cpp` (1P marker), `Enemy.cpp` / `GameEvent.cpp` / `AutoHpCtrlObj.cpp` (1-player
  balance, free falls), `DebugToolsProcess.cpp` (TEAM tab, F9, badge).

## Known limits and open questions

- Only tested in the Combat Lab arena under Wine (see Test results). A real story stage (start
  positions, loading the extra turtles, gimmicks, cutscenes) has not been run yet; please try a few
  different stages first.
- The AI has no path finding: around walls it walks straight, then jumps, then teleports.
- Companions do not lift or throw objects, use gimmicks, or pick a fight with breakable boxes.
- Enemies spread their attention over every turtle, as in co-op, so fights are easier than solo
  even at single-player boss health. Lower Aggression or Companion damage taken changes the feel;
  there is no "enemies ignore companions" option yet.
- Settings are not saved between runs.

## Test checklist

1. Story mode, pick Leonardo only, start a stage (e.g. 1-1): all four turtles appear; "1P" over Leo.
2. Walk around: companions follow behind; walk far away: they catch up or blink in behind you.
3. Fight a group: companions pick different enemies, string combos, guard sometimes.
4. Press Change Character mid-combo: control jumps at once, "YOU: ..." badge, flash; old turtle keeps
   fighting.
5. Climb to a ledge: companions jump after you or blink in.
6. F9 (badge), retry the stage: retail one-turtle stage; Change Character only when allowed by the
   Instant switching row.
7. Boss stage: boss health bar the same as solo; camera stays on you.

## Test results

| Check | Result |
| --- | --- |
| MSVC Win32 release build in CI (`claude/turtle-companions-v1-0-0-us`), retail-path hooks all inside `TMNT2_DEBUG_TOOLS` (CI check) | PASS |
| MinGW syntax check of every changed file, with and without Debug Tools | PASS |
| Package: T2DELTA payload re-applied to your retail `TMNT2.exe` rebuilds the core byte for byte | PASS |
| Real game under Wine with your `TMNT.DAT`, test build that allows the team in Combat Lab (`-companiontest`, never shipped): team of 4 created (Leo human, Raph/Mikey/Don AI), companions pick different enemies, play A / AA / AAB / AABB / AABBB / B strings, guard, recover from knockdowns, follow; automatic switch every 6 s, 44 switches in 2.3 min, "YOU: DONATELLO" badge, 1P marker moves; no crash | PASS |
| First run found companions hopping at an unreachable follow spot (water edge); fixed and re-run: no hopping | FIXED |
| Story stage, F9 / OFF path, instant switching with companions off, physical pad | **NOT VERIFIED** here |

The release EXE itself stops in the game's sound code (`SdGetBgmPosition`) under Wine because the test
used placeholder voice files; the shipped v1.2.24 stops at the same place in that setup, so it is the
test environment, not this change.
