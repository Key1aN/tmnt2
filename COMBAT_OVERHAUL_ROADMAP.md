# TMNT2 Combat Overhaul - Source Findings and Roadmap

Baseline: `224106c` (Karai v1.4.1 / Debug Tools v1.2.7) + Combat Lab v1.1.0.
**S** = confirmed in source. **H** = gameplay hypothesis to verify in Combat Lab. Nothing here has
been measured in game yet.

## 1. Repetitive combos

Files: `Component/Player/ConcretePlayerStatus/PlayerStatusAttack.cpp`, `PlayerCharacter.cpp`
(`CheckAttackConnect`, `RequestAttackA/B/C`), `System/Character/Character.cpp`
(`GetNextChainMotionConnectTime`), `Component/Player/Manipulator.cpp` (`RunAttackCommon`).

- **S** The ground string is one fixed tree: A -> AA -> {AAB, AAC} -> {AABB, AABC} -> {AABBB, AABBC}.
  Each status hard-codes its successors. B (charge) and knife are separate entries; there are no
  branches from B, run, dash or jump into the A tree.
- **S** Branches are gated by the *save file*: AAC needs red crystal level >= 1, AABB red >= 2,
  AABC green >= 2 (`CSecretRecord::GetAttackLevel/GetDefenceLevel` = crystals / 10). Red level 3
  also doubles attack power. A low-crystal save literally has fewer combos. Combat Lab now forces
  level 3 by default ("Crystal upgrades") so tests are comparable.
- **S** Chains connect only at the motion-parameter connect time; chain-ender motions without one
  fall back to motion end (Karai v1.4.1 added a per-character fallback ratio).
- **H** "Repetitive" is mostly the single A-tree plus no launcher/air/directional variants.
- Recommendation: data-driven chain table per character (successor by button + stick direction),
  first adding 1-2 directional branches using existing motions only. Side effects: animation
  blending, hitbox timing of reused motions. Difficulty: medium. AI params: no. Expose as an
  Experimental switch "Extended combos". Test: same 3 Foot Ninja, count distinct chains used and
  hit chain max over 60 s, Vanilla vs Experimental.

## 2. Poor targeting

Files: `Manipulator.cpp` (`RunGrounding`, `RunAttackCommon`), `PlayerCharacter.cpp`
(`RotateDirection`), enemy body hit data.

- **S** No lock-on or auto-aim exists. The first attack snaps facing to the stick direction only if
  the stick is held; otherwise the current facing is used.
- **S** During a chain, steering is `RotateDirection`, clamped to `elapsed * PI/2`, i.e. **90 deg per
  second**. A combo cannot follow an enemy that sidesteps or is knocked sideways.
- **H** Most whiffs in multi-enemy fights come from that clamp plus no target memory, not from
  hitbox size.
- Recommendation: Experimental "Improved targeting": at chain start and each connect, rotate
  toward the nearest valid enemy within a cone/radius (reuse the Adaptive Teleport v2 target
  search and visibility checks). Side effects: can pull the player toward the wrong enemy; keep a
  stick override. Difficulty: low-medium. AI params: no. Test: lab hitbox view + "ineffective" and
  hit counts with 3 enemies, Original AI, fixed seed.

## 3. Unresponsive attacks

- **S** `CheckAttackConnect` evaluates once at the connect time and then clears
  `FLAG_ENABLE_ATTACK_CONNECT`: presses after that instant are dropped (Combo Assist's late window
  already addresses this when enabled). Early presses are buffered (request flags latch).
- **S** Every attack returns to IDLE only at motion end (`CAttackCommon::OnRun`): full recovery lock.
- **S** Cancels: Dash can cancel any ground A-chain or B attack (`CDashCancel` enable list). Guard
  (`CGuardReady`) and a new A only start from IDLE/WALK/RUN, so guard/jump cannot cancel recovery.
- Recommendation (Experimental "Responsive attacks"): late-buffer window (reuse Combo Assist
  logic), and a recovery cancel into guard/new attack after the active frames. Side effects:
  exploitable loops; tune per move. Difficulty: medium. Test: lab log HIT timestamps between
  consecutive player hits, and hit chain max, Vanilla vs Experimental.
- Instrumentation gap: per-move startup/active/recovery times need a hook on motion-parameter
  attack timing; not added yet (no estimates are shown).

## 4. Exploits and balance

- **S** Damage: guard = 0.2x, counter = 2x, minimum 1 (`CCharacterAttackCalculator::CalcDamage`);
  player power x2 for charge phase 3, attack power-up item and red crystal level 3 (stacking).
- **S** Enemy knockback uses accumulated damage over a timer (`CEnemyUtils::CKnockBackControl`).
- **H** Charge-3 B and stacked x2 multipliers trivialise grunts. Measure first: lab SUMMARY lines
  (damage per hit by move) on Foot Elite with infinite enemy HP.
- Recommendation: no blanket nerfs; collect per-move damage from the lab log, then adjust only
  outliers. Difficulty: low (data) / medium (rules).

## 5. Enemy AI and engagement

Files: `Enemy/ConcreteAIModerator/BaseGeneralEnemyAI.cpp`, `BaseGrapplerAI.cpp`, `BaseGunnerAI.cpp`,
enemy parameter tables (`ConcreteEnemyCharacter/0xx*.cpp`), `EnemyParameter.cpp`.

- **S** Foot Ninja sword/staff use `CBaseFootNinjaAI : CBaseGrapplerAI : CBaseGeneralEnemyAIModerator`.
  Foot Ninja (staff) Normal row: attack A 60, AA 50, AAA 40, B 60, run attack 30, guard 10.
  Thinking frequency 0.5, activity 0.3, front view 1.0, rear view 0.0.
- **S** General AI computes its decision interval `1 / (fps * thinkingFrequency)` **once, in the
  constructor**. The Debug Tools "AI thinking rate" therefore changes live enemies' wait lengths but
  not their decision cadence; respawn (lab Reset scenario) to apply it fully.
- **S** `CBaseGrapplerAI::OnActionOfWait` waits `2 * thinkingFrequency` seconds, so raising
  "thinking rate" makes grappler waits **longer**. The knob is not monotonic "more aggressive" for
  grunts.
- **S** "Attack interval" only feeds the 6045-style decision units (bosses/elite); it does not
  affect General-AI grunts.
- **H** Grunts feel passive because of activity 0.3 (target acceptance), long waits and frequency
  rolls, not missing behaviours. Test this first with existing knobs: activity 1.5-2.0, attack
  frequency 1.5, thinking rate *lower* (shorter waits), Reset scenario, compare hits received/min.
- Recommendation: if knobs suffice, ship an AI preset; otherwise fix the cadence/wait coupling in
  General AI (Experimental "Improved enemy AI"). Difficulty: low (preset) / medium (code).

## Priority

| # | Change | Impact | Ease | Safety | Visible A/B |
| --- | --- | --- | --- | --- | --- |
| 1 | AI tuning with existing knobs (activity, frequencies, thinking) | High | Very easy | High | Yes |
| 2 | Improved targeting (rotate to nearest enemy at chain start/connect) | High | Easy-med | Med | Yes |
| 3 | Responsive attacks (late buffer + guard/attack recovery cancel) | High | Medium | Med | Yes |
| 4 | General-AI cadence fix (recompute think interval live) | Med | Easy | High | Yes |
| 5 | Extended combos (directional branches) | High | Med-hard | Med | Yes |
| 6 | Data-driven balance pass from lab logs | Med | Easy | High | Numbers |

Each item becomes one switch under the lab's Experimental ruleset; Vanilla stays retail.
