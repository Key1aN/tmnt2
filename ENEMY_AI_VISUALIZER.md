# Live Enemy AI Visualizer (Combat Lab v1.2.0 / Debug Tools v1.2.14, US)

This build adds a read-only view of what each enemy's AI actually evaluates. It does not change
enemy behaviour, timing or random numbers. It is off by default, and turning it on adds no RNG
calls and no AI calls.

## How to use it

1. Press F4 to open Debug Tools and go to the **AI VIEW** tab (it comes after COMBAT LAB).
2. Turn **AI visualizer** ON. A top-down radar appears at the right, a timeline appears along the
   bottom, and the decision panel appears under the menu (it stays on screen when the menu is closed).
3. In Combat Lab (Ctrl+Shift+L or `CombatLab.exe`), **Six Foot Ninjas test** spawns six sword Foot
   Ninjas against P1 with the lab's fixed seed and clears the statistics. Pick the AI mode
   (Vanilla / Experimental) and the Difficulty multipliers first.
4. Use **Inspect enemy** (Left/Right) to choose whose decisions are shown. The selected enemy has a gold ring on the radar.
5. **Record to TMNT2_aitrace.jsonl** writes everything to a file next to `TMNT2.exe`.
   **Write overlap summary** adds a summary line. Open the file in
   `tools/AIVisualizer/ai_replay.html` (included in the package) to replay it, scrub, export
   statistics, or compare two recordings (for example vanilla vs your multipliers).

### What is on screen

| Element | Source |
|---|---|
| Marker colour | `CEnemyCharacter::GetStatus()` grouped into categories (`CEnemyAITrace::CategoryOf`). Red = ATTACK_*/FIRE_*/THROW/JUMP_ATTACK, yellow = GUARD, blue = TURN/WALK/RUN/DASH/JUMP, gray = IDLE/THINKING, orange = Foot Ninja back-away (STATUS_SPECIAL), purple = knock/flyaway/down/getup/stun, white = appear/death/other |
| Facing line | `Compositor().GetDirection()` |
| Line to P1 | The enemy currently holds P1 as its target (last `SearchTarget` result) |
| White outline ("in range") | Same distance and front-cone test as `CBaseGrapplerAI::IsTakeAttack` (distance <= suitable distance, angle <= 36 deg x front view). The terrain "movable" test is not repeated, so in rare edge/hole cases the outline can show while the real check fails |
| Inner red ring | Foot Ninja suitable distance 2.0 |
| Think interval / since | `m_fThinkElapseTime` and `m_fTimer - m_fThinkBeforeTime` |
| Back-away cooldown | `CBaseFootNinjaAI::m_fBackAwayTimer` |
| Decision panel lines | Recorded at the real call sites, in the order the game evaluated them (see Hooks) |
| "retail" numbers | The unmodified table row / characteristic stored before multipliers (`GetDebugFrequencyRows`, `GetDebugBaseAICharacteristic`) |
| Overview counts | Per-frame status categories. "In range" uses the outline test above |
| Overlap statistics | An attack start is a change into the attack category. It counts as overlapping if another enemy was already in the attack category in that frame |

There is no "preparing to attack" colour. No status sits between choosing an attack and starting
the attack animation, so nothing real could be shown for it. A Foot Ninja's own attack recovery is
part of its ATTACK_* status, so it is red until the status changes.

### In-world awareness zones (Debug Tools v1.2.12)

AI VIEW -> **Show awareness zones** (OFF / ALL ENEMIES / INSPECTED ENEMY) draws each enemy's real
decision geometry on the ground, from its live values (multipliers included). It works without the
visualizer; "inspected" is the Inspect-enemy choice when the visualizer is on, otherwise the enemy
nearest P1. **Zone labels** adds the numbers.

| Shape | Colour | Code it mirrors |
|---|---|---|
| View / awareness area (teardrop: long in front, short behind) | blue, brighter when P1 is inside | `CAIUtils::IsObjectViewArea2`: reach = suitable + (action radius - suitable) x (front + (rear - front) x angle/180) |
| Attack cone | red fill | `CBaseGrapplerAI::IsTakeAttack`: distance <= suitable, angle <= 36 deg x front view |
| Run-attack reach | orange outline | `IsTakeRunAttack`: 2 x suitable, same cone, only while running |
| Guard cone | yellow outline | `IsTakeGuard`: distance <= suitable, angle <= 90 deg x front view (plus P1 attacking and facing it) |
| Terrain probe ticks | white ahead, orange behind | the "movable" checks at suitable distance ahead (attack) and behind (Foot Ninja back-away) |
| Patrol area | green circle | `CAIUtils::IsInsidePatrolArea` (no circle = unlimited) |
| Line to P1 | blue = seen, red = in attack cone | same measurement as above |

**AI awareness (Difficulty tab), fixed in v1.2.14.** Up to v1.2.13 the multiplier only scaled the
front/rear *view ratios*. That widened the attack and guard cones, but it could not make enemies
notice you from farther away: after the view check, the target code (`ICalcHateOfGeometry`) also
drops any player beyond the enemy's **action radius** (10 for Foot Ninjas), so the extra reach drawn
by the zones was never usable. Awareness now also scales the action radius (relative to the game
difficulty's own awareness value), and the blue zone is drawn limited by that radius, so what you see
is where the enemy can actually pick you as a target. The Difficulty tab shows the result as
"reach N" (front detection distance). A larger action radius also lets enemies keep chasing you
farther (the "player beyond action radius" move refusal uses the same value). Behind an enemy a
rear view of 0 stays 0, so for Foot Ninjas the reach behind stays at the attack distance (2.0).

Attack, run and guard shapes are only drawn for Grappler-family AI (Foot Ninjas, most grunts), where those
checks exist. The view area is also only one of the target conditions: the enemy must also be inside
its patrol area and have a walkable route (collision term) for P1 to become its target.

### ImGui menu (Debug Tools v1.2.14)

The Debug Tools menu now opens as a Dear ImGui window by default (Graphics -> Debug menu style:
ImGui / Game font). It is the same menu, not a second one: tabs, rows, values, help text and
actions all come from the existing menu functions, so every option behaves the same with keys,
pad or mouse. Click a row to select it, use the arrow buttons / checkboxes / sliders, double-click
to run, right-click a Difficulty slider to reset it. On the AI VIEW tab the inspected enemy's
decision history opens in its own movable "Enemy AI decisions" window. The window layout is saved
in `TMNT2_debugtools_imgui.ini`. If ImGui cannot start on the graphics device, the game-font menu
is used automatically.

## How Foot Ninja AI works (source)

Class chain: `CBaseFootNinjaAI` -> `CBaseGrapplerAI` -> `CBaseGeneralEnemyAIModerator`
(`src/Game/Component/Enemy/ConcreteAIModerator/`). Retail Foot Ninja (staff, Normal) values used below:
attack A 60, AA 50, AAA 40, B 60, C 0, run attack 30; guard vs A-chain 10, vs B 10, vs shuriken 10, vs jump 10;
thinking 0.5, activity 0.3, move 0.5, front view 1.0, action radius 10, suitable distance 2.0, impulsiveness 0.

```mermaid
flowchart TD
    R["Run() every frame<br/>BaseGeneralEnemyAI.cpp:873"] --> A{"ThinkOrder answer"}
    A -- "ACCEPT (order finished)" --> T
    A -- "REFUSE (order failed)<br/>target reset" --> T
    A -- WAITING --> E{"EnableThinking() :1286<br/>not THINKING / not knocked<br/>and think interval passed?"}
    E -- no --> X["keep doing current order"]
    E -- yes --> U["UnderThinking() :1173"]
    U --> UW["Foot Ninja OnUnderWait<br/>BaseFootNinjaAI.cpp:102"]
    UW -- "IsTakeBackAway: player attacking in front,<br/>5 s cooldown over (no roll)" --> F["back away (Special1)<br/>FORCED"]
    UW -- else --> GW["Grappler OnUnderWait<br/>BaseGrapplerAI.cpp:396"]
    GW -- "IsTakeGuard -> guard roll<br/>(OnActionOfDefence)" --> F2["GUARD<br/>FORCED"]
    GW -- "SearchTarget + activity roll (30%)<br/>while WAIT, or MOVE with no player target" --> T
    GW -- "roll failed" --> X
    T["Thinking() :1212"] --> S["SearchTarget() :1328<br/>hate coordinator (view, radius, path...)<br/>take new target only if activity roll < 30%<br/>else keep previous target"]
    S --> AS["Foot Ninja ActionSelector<br/>BaseFootNinjaAI.cpp:38"]
    AS -- "IsTakeBackAway (no roll)" --> BA["ATTACK -> back away"]
    AS -- "IsTakeJumpAttack (Foot Ninja: 0%)" --> JA["ATTACK -> jump"]
    AS -- else --> GS["Grappler ActionSelector<br/>BaseGrapplerAI.cpp:40"]
    GS -- "outside patrol area<br/>move roll 50%" --> RUN["RUN HOME"]
    GS -- "no active player" --> W
    GS -- "IsTakeGuard: dist <= 2, within 90 deg,<br/>player attacking toward enemy" --> D["DEFENCE<br/>OnActionOfDefence :325<br/>guard % by player move (10%)"]
    GS -- "IsTakeAttack: dist <= 2, within 36 deg<br/>or IsTakeRunAttack while running" --> AT["ATTACK<br/>OnActionOfAttack :270<br/>rolls: run, C, B, AAA, AA, A"]
    GS -- "last move refused" --> W
    GS -- else --> MV["MOVE<br/>OnActionOfMove :177<br/>no path / beyond radius 10 -> fail<br/>move roll 50% -> run (or walk/side-walk when near, activity 30%)<br/>roll failed: turn if not facing, else fail"]
    D -- "guard roll failed" --> W
    AT -- "every attack roll failed<br/>or player down / unhittable" --> W
    MV -- fail --> W
    W["WAIT<br/>OnActionOfWait :89<br/>wait timer = 2 x thinking (1.0 s)<br/>move roll 50%: wander to random point<br/>else activity 30%: turn to target<br/>else stand (IDLE)"]
```

Key facts:

* **Decisions happen at three moments.** (1) The current order ends (`ACCEPT`), for example when an
  attack animation finishes or the IDLE wait timer runs out
  (`CCommonEnemyObserver::CIdleStatus::Observing`). (2) The order fails (`REFUSE`). (3) A periodic
  check while waiting or moving ends early. That check runs at most once per
  `m_fThinkElapseTime = 1 / (frames-per-second x thinking frequency)`, which is computed once in the
  constructor (`BaseGeneralEnemyAI.cpp:863`). The panel shows the real value.
* **The action selector is driven by geometry, not chance.** Attack is chosen only when the player
  is within 2.0 units and inside a 36 deg front cone (or during a run, within 4.0). Guard is checked
  **before** attack: if the player is mid-attack, within 2.0 and facing the enemy, the enemy goes to
  DEFENCE (or the Foot Ninja backs away) and never ATTACK.
* **The attack table decides two things.** It decides *whether* the attack happens and *which*
  attack. Rolls are made in order (run attack only if eligible, then C, B, AAA, AA, A); the first
  that passes wins (`freq > RandRange(0,100)`). With retail numbers the enemy attacks about 95% of
  the time ATTACK is chosen (fails only when every roll fails: 0.4 x 0.6 x 0.5 x 0.4 = 4.8%). The
  mix is about 60% B, 16% AAA, 12% AA, 7% A. When all rolls fail, it waits.
* **Back-away has no roll.** When guard conditions hold and the 5 s cooldown is over, the Foot
  Ninja backs away (`IsTakeBackAway`) unless the spot behind it is a fall or hole.
* **No coordination code exists.** Nothing in the General AI, Grappler AI or Foot Ninja AI reads other enemies.
  The hate calculators only consider players. The `FORMATION` / `APPROACH` frequency columns are
  not read by Foot Ninja AI. There are no attack slots, no tokens and no director.

## Answers (code evidence; check the numbers with your own recordings)

**Why do Foot Ninjas stand around?** The code gives these paths to IDLE, all of which the panel labels:

1. *Move roll failed while already facing the player.* `OnActionOfMove` rolls move (50%). On a
   fail, it turns if the player is outside the 36 deg cone; otherwise it returns false
   ("Move refused: already facing"), and the enemy falls back to WAIT. So about half of all
   "approach" decisions out of range turn into a wait when the enemy already faces you.
2. *Guard chosen but the guard roll failed.* While you attack in front of it, the selector picks
   DEFENCE before ATTACK. Guard vs your move is 10% retail, so about 90% of those decisions fall
   back to WAIT. Foot Ninjas facing a player who keeps attacking mostly stand and wait. They do not
   counterattack until your attack ends or the back-away cooldown fires.
3. *Every attack roll failed* (4.8% retail) -> WAIT.
4. *Move refused* because the player is farther than the action radius (10) or unreachable
   ("no path") -> WAIT, then the next decision stays WAIT (`ActionSelector`: "last move refused").
5. *The wait itself.* A WAIT lasts up to 1.0 s (2 x thinking 0.5). It ends earlier only through the
   periodic 30% activity check (with a target), the guard reaction, or the Foot Ninja back-away. A
   wait can also be a short wander or a turn instead of standing (move 50% / activity 30% rolls in
   `OnActionOfWait`).

The decision line shows "prev order ran X s" and how many wait checks and failed activity rolls
came before each decision, so standing time can be attributed per cause.

**Do they wait for each other?** No. There is no code that reads other enemies, so an enemy never
waits because another one is attacking. Any apparent turn-taking comes from geometry (only enemies
within 2.0 and facing you can choose ATTACK, and bodies push each other apart) plus the independent
rolls.

**Can all six attack at once?** Yes. Nothing limits simultaneous attacks. The overview shows "Max at
once" and seconds with 2+ attacking. In practice, only enemies within 2.0 and facing you can choose
ATTACK.

**What prevents attacks?** In order: being outside the patrol area, having no target (the view
check, radius 10, or the activity roll when acquiring a target), guard taking priority while you
attack, distance > 2.0 or angle > 36 deg x front view, a "movable" check in front of the enemy
(wall, fall or hole), the player being down or in an unhittable state, and every attack roll
failing. Each one has its own line in the panel.

**Guard vs attack?** No roll decides between them. Guard eligibility (player attacking, within 2.0,
within 90 deg, facing each other within 72 deg) is checked first and **blocks attack** for that
decision. When guard is not eligible, the attack table applies.

**Does activity reduce idle time?** Partly. Activity (30%) is used for: taking a target when a new
one is found, the periodic "stop waiting and re-decide" roll, turning instead of standing in a wait,
and running instead of walking when close. Raising it ends waits sooner and makes waits more often
a turn. It does not touch the move-roll fail path (#1), the guard-fail path (#2) or the 1.0 s wait
length (that comes from thinking frequency).

**Does attack frequency increase attacks or only the mix?** Both, but only **after** ATTACK is
chosen. It raises the chance that a chosen ATTACK becomes an attack (retail already about 95%) and
changes the mix heavily: B is rolled before AAA/AA/A, so a 3x multiplier (capped at 99%) makes
almost every attack a B. It does not make enemies choose ATTACK more often; that comes from distance
and facing. C stays 0 because 0 x anything = 0.

**How often do attacks overlap?** The code allows any amount. Measure it with the overview ("Attack
starts N, X% while another attacked", "2+ attacking Y% of span") or the replay page's statistics. I
cannot give a number from source alone.

### Code quirks seen while tracing (not changed)

* The angle tests use `fabs(targetAngle - myDirection)` with no wrap-around. If both values are in
  -pi..pi, a target just across the +/-180 deg seam reads as nearly 360 deg off. That fails the attack
  cone and leads to a turn instead.
* `FREQUENCY_COMMON_2` (the jump-attack check) is 0 for Foot Ninjas, so the jump-attack branch never fires for them.

## Hooks (all behind `AITRACE`, compiled only with `TMNT2_DEBUG_TOOLS`, active only when enabled)

| File | Function | Records |
|---|---|---|
| `BaseGeneralEnemyAI.cpp` | `Run` | begin/end, why the decision ran (ACCEPT/REFUSE/re-decide/forced), think timers |
| | `Thinking` | selector result, each fallback to WAIT |
| | `IsSatifyFrequency` | table value and the roll it was compared with |
| | `SearchTarget` | target activity roll, impulsiveness, result |
| `BaseGrapplerAI.cpp` | `ActionSelector` | patrol run roll, eligibility flags (the same checks, evaluated once in the retail order) |
| | `OnActionOfWait` / `OnActionOfMove` / `OnActionOfAttack` | rolls and every refusal reason |
| | `OnUnderWait` | guard reaction, activity re-decide roll |
| `BaseFootNinjaAI.cpp` | `ActionSelector`, `OnUnderWait`, `OnUnderAllActions` | back-away, jump attack, cooldown |

The only control-flow edit is in `CBaseGrapplerAI::ActionSelector`. `IsTakeAttack() || IsTakeRunAttack()`
became two locals with the same short-circuit (`bTakeRunAttack` is evaluated only when attack failed),
so the calls, their order and their results are unchanged.

## Limitations

* Only enemies that use the General AI (`CBaseGeneralEnemyAIModerator` and its subclasses, which
  includes all Foot Clan grunts) get decision data. Other enemies show status, position and timeline only.
* Shooter / gunner subclasses override some handlers that are not hooked. Their attack rolls still
  appear (through `IsSatifyFrequency`), but some refusal reasons do not.
* Each decision stores up to 20 checks. A rare longer one is marked "(more checks than the panel stores)".
* 16 enemies are tracked at once; slots of removed enemies are reused.
* The JSONL file is appended to (sessions are separated by header lines) and flushed every 2 s, so
  the last 2 s can be missing if the game is killed.
* Runtime numbers in this document come from source reasoning. Your recordings give the real figures.
