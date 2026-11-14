# TMNT2 Combat Overhaul v1.0.0 (Debug Tools v1.2.16, US)

Fifteen combat changes with a BEFORE / AFTER switch, built on Combat Lab v1.2.0 /
Debug Tools v1.2.15 (`ea19c64`). Status: **compiles in CI; not yet played**. Every number
below is a first guess to be tuned from hands-on testing.

## Switching (for video)

| What | How |
| --- | --- |
| BEFORE (retail) / AFTER (overhaul) | **F6** at any time, or Debug Tools (F4) > **OVERHAUL** > first row, or COMBAT LAB > Ruleset (same switch) |
| One feature on/off | OVERHAUL tab, rows 1-15 (numbered as in the idea list) |
| On-screen badge | Shows "BEFORE - RETAIL COMBAT" / "AFTER - COMBAT OVERHAUL" for 2.5 s after a switch; OVERHAUL > BEFORE / AFTER badge = ALWAYS keeps it on screen |
| Hide gauges and markers | OVERHAUL > Gauges and markers (mechanics keep working) |

The game starts on AFTER with every feature ON. With BEFORE, or with a feature OFF, each hook
returns before changing anything, so that code path is the retail one. Switching to BEFORE
clears gauges, locks and pending bursts. Recorded demos (pad stream) keep retail input rules.

## Features

| # | Feature | What changes | Numbers |
| --- | --- | --- | --- |
| 1 | Dash dodges arrows | Shots (arrows, shuriken, kunai...) do nothing to a dashing player | whole dash + first 0.12 s of its recovery |
| 2 | Mikey 360 guard reflect | While Michelangelo guards, enemy shots from any side are reflected and cannot hit his back | reflect sphere radius 1.3 |
| 3 | Mikey faster combos | His ground chain (A ... AABBC) plays faster; chain windows scale with it | 1.25x (1.00 - 1.60 in the menu) |
| 4a | Posture break | Enemy: each guarded player hit +30%, full = retail guard break (stagger; next hit is a counter, 2x). Player: guarded hits fill the gauge; full = guard and parry stop working until landed hits empty it | player +14-24% per guarded hit, -20% per landed hit while broken; decay after 2 s |
| 4b | Combo break | Player: 4 hits taken fill the gauge, **Guard + Jump** blasts nearby enemies away (0.8 s invincible). Enemy: 8 unguarded hits fill theirs; they blast free from a standing hit reaction, then 6 s cooldown | blast radius 3.0 / 2.5 |
| 5 | Attack indicator | Mark over an enemy when it enters an attack: cyan = normal, red = heavy (B, C, run attack, throw, jump attack, AA/AAA strings, repeat fire, every boss attack) | 0.9 s |
| 6 | Parry gives a crystal | Parry (guard just before the hit) grants one random crystal power with its aura: red attack, green defence, white free shuriken, orange instant charge. Parry no longer needs the defence upgrade | 6 s (3 - 15 in the menu) |
| 7 | Don mobile shield | After the barrier appears Donatello returns to neutral; the barrier runs its normal time around him | retail barrier time |
| 8 | Rally | More than 25% of max HP lost within 5 s makes that HP regainable for 5 s; landed hits return 75% of their damage, up to the pool. New damage joins the pool and restarts the clock | 25% / 5 s / 5 s / 75% |
| 9 | Fast guard recovery | Block pushback 0.4 s -> 0.2 s; A, dash and jump work from a held guard; any input cancels the guard-lowering motion | Guard + B is still the lift |
| 10 | Automatic quick rise | The retail quick rise (attack/jump within 0.2 s of landing) happens by itself; wall-crash knockdowns skip the lying phase | keeps the retail 1 s invincibility |
| 11 | Lock-on | **R3** (right stick click) or **T**: lock the enemy you face (16 units). Attacks and shuriken aim at it, brackets mark it. On defeat the lock moves to the next enemy | breaks beyond 24 units |
| 12 | Dash follows the stick | The character turns to the stick before a dash or a dash cancel | - |
| 13 | Attack out of dash | Attack, shuriken, jump, guard cancel the dash recovery; an attack pressed during the dash is used when it ends (0.35 s). Strong attack, run attack, shuriken throw and AABBC can be dash-cancelled after 45% of the motion | - |
| 14 | Free combo steering | Each chain hit snaps to the stick; steering between hits is 540 deg/s instead of 90 deg/s | - |
| 15 | Smart targeting | Each attack turns to the best enemy within 5.5 units: closest to the aim direction (stick, else facing), then nearest; current target is sticky. With a neutral stick the string glides to the next enemy at 360 deg/s | stick: 50 deg cone; neutral: front arc |

## Where the code is

- `src/Game/Component/CombatOverhaul/CombatOverhaul.{hpp,cpp}`: all state, rules, numbers and
  the OVERHAUL menu rows. Tuning constants are at the top of the `.cpp`.
- Hooks (each under `TMNT2_DEBUG_TOOLS`): `Manipulator.cpp` (input rules), `PlayerCharacter.cpp`
  (damage, defence flags, chain timing), `PlayerStatus{Attack,Guard,Damage,Basic}.cpp`,
  `Donatello.cpp`, `EnemyCharacter.cpp` (guard break, indicator, enemy burst), `Shot.cpp`
  (friendly shuriken pass through Mikey's reflect), `Character.cpp` (per-hit bookkeeping),
  `CombatLab.cpp` (Ruleset row = master switch), `DebugToolsProcess.cpp` (page, F6/T, display).

## Known limits and open questions

- Nothing here has been run in the game. Expect tuning and possibly bugs in: enemy burst out of
  hit-stun, Donatello's barrier after leaving the status, Mikey's reflect radius.
- 1 "dodge system" is read as dash invulnerability to projectiles. Explosions from fire arrows
  and sleep gas are separate effects and still apply.
- 5 marks the moment the enemy starts the attack status, which is the frame its AI decision
  takes effect. For non-grunt AIs the red/cyan choice uses the attack type, not predicted damage.
- 11 keyboard lock-on is player 1 only and uses T; if T is bound to a game action, change it.
- 13 "after the active frames" is approximated as 45% of the motion for moves without a retail
  dash cancel (the A chain can already dash-cancel at any time in retail).
- 14 does not shorten the forward travel baked into the attack animations; it only lets the
  player aim each hit.
- Bosses use the same posture and combo-break rules as grunts.
- Settings are not saved between runs.

## Test checklist (Combat Lab, 3 Foot Ninja, then Foot Archers)

1. F6: badge flips; with BEFORE every item below behaves as retail.
2. Archers: dash through an arrow (1); guard as Mikey with an archer behind (2).
3. Mikey A,A,B,B: visibly faster, still chains (3).
4. Difficulty > Guard frequency 3.00x: 4th blocked hit staggers the enemy (4a). Hold guard
   until POSTURE BROKEN, then land 5 hits (4a). Take 4 hits, Guard + Jump (4b). Juggle one
   enemy for 8 hits (4b).
5. Marks appear as enemies start attacks; red on strings and heavies (5).
6. Parry: aura + PARRY text + timer (6). Don hold B, release, walk and attack (7).
7. Lose 25% HP quickly, then hit back: RALLY counts down and HP returns (8).
8. Block, then A / dash / jump immediately (9). Get knocked down (10).
9. R3 or T near enemies; attack and throw shuriken; kill the target (11).
10. Dash with the stick sideways and backwards; dash, then B at once (12, 13).
11. Strings between two enemies with and without the stick (14, 15).
