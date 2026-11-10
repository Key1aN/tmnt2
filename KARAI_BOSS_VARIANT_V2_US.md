# TMNT2 Karai Boss Variant v2 US (milestone 2: boss moves)

## Boss moves (variant only)

| Input | Move | Boss source | Notes |
| --- | --- | --- | --- |
| Guard + A | Dash Strike | Dash1 -> held Dash2 lunge (17 u/s, 5 u) | dash trails; invincible during the lunge |
| Guard + B | Wind Press | B -> BB + `wind_press` magic | replaces grab (retail Guard+B); magic parented to Karai so it hits enemies; power 6 (boss 10 x 0.6) |
| Guard + A+B | Thunder Special | C1 charge (0.75 s, 3 glows, electric aura) -> C2 dash (24 u/s) -> C3 finisher | faces nearest enemy within 12 u / 70 deg; 6 s cooldown |
| Guard + Jump | Kunai Rain | J1 -> spinning JKunai jump -> J3 | 12 player kunai radially, pitched 22 deg down |
| Guard + Shuriken | Counter Slash | D | faces nearest enemy within 4 u |
| Guard + Dash | Back Away | BackAway | backflip, invincible |
| Charged B | Thunder finisher | C3 + electric burst | replaces the vanilla charge wave; game charge power bonus still applies |

All boss motion hit data is retargeted to enemies at 60% power (milestone 1 remap).
Karai's custom move statuses can be interrupted by damage (no super armor); Slashuur's
custom statuses are unchanged.

LPAC v2 additionally appends `common_ef` + `wind_press` and `common_ef` + `shrd_elec`
from entry 87 (96 chunks, 1,254,272 bytes, SHA-256
`489d05e0b5147686614f00a109e10279819a0c2fcd804d0f5a69e2c3df821b04`).

# Milestone 1 (included): variant

Target: TMNT 2: Battle Nexus, US/NA 32-bit PC. Cumulative on Debug Tools v1.2.6
(boss labels, perf telemetry, v1.2.4 controller line), Ultimate Slashuur, Intro Skip,
MSAA v4, brutal difficulties, and Hor+ widescreen.

## Why a variant

Playable Karai (`karai`, AFS entry 145) and boss Karai (`enemy084`) do not share a
skeleton: 24 vs 21 HAnim bones with different IDs and hierarchy. Boss animations can
only play on the boss skeleton, so the boss moves require Karai to run on the boss
model. (Slashuur 21/21 and Splinter 25/25 do match; Karai does not.)

## Selection

Hold **R1** while confirming Karai on character select (secret row). Without the
`karai-boss-variant` feature flag, or if the `karai_boss` model/parameters are not
loaded, Karai falls back to the normal model.

## Assets (lossless LPAC splice)

`scripts/lpacview/build_karai_boss_variant_lpac.py` reads the verified retail DAT
(entry 145 player Karai, entry 87 containing `enemy084`), keeps all 49 original
player Karai chunks byte-for-byte, and appends 43 records named `karai_boss`:
texture dictionary, toon model (texture-set reference renamed), motion set, the 39
boss motions, and the boss motion parameters. Output: 92 chunks, 1,231,744 bytes,
SHA-256 `6b6269118c735a4d4fac869a14a5257659d677f97e484d67aca876194ca9834e`.

## Motion fallbacks

`CCharacter::ChangeMotion` resolves names through an optional per-character alias
table (no table = retail behavior). A missing motion previously dereferenced null.
The variant maps player-only motions to boss motions:

| Player motion | Boss motion |
| --- | --- |
| AABB, AABBB | AAA |
| AABC, AABBC | AAC |
| JAttack | JAttack2 (airborne attack; JAttack1 is the boss ground wind-up, fixed in 1.0.1) |
| JWall | J2 |
| Shuriken / JShuriken | D / JKunai |
| Nage1 / Nage2 / Nage3 / NageBack | B / BB / AAC / BackAway |
| NageIdle / NageWalk | Idle / Walk |
| NagerarePose / NagerareTag1 / NagerareTag2 | YFrontFuttobi1 / 2 / 3 |
| Piyori / BiriBiri | Idle / YFront |

Anything else unknown falls back to `Idle`. Bone IDs used by player code (3 knife,
10 body) exist on the boss skeleton.

## Attack data remap (1.0.2)

Boss motion parameters store each hit sphere's target mask, and boss Karai's
spheres target players. On the variant, `CCharacter::UpdateCollisionParameterForAttack`
replaces the player target bit with enemy+gimmick and scales power to 60%
(`SetEnemyAttackDataRemap`). Without the remap, combos and the jump attack produced
hit spheres that could only hit players (1.0.0/1.0.1: no damage to enemies).

## Jump attack (1.0.3)

Boss Karai's jump attack is `JAttack1` (ground wind-up) -> `JAttack2` (airborne) ->
`JAttack3` (landing strike). The variant registers its own jump-attack and touchdown
statuses: `JAttack2` plays while diving at -9 units/s, and landing from the jump
attack plays `JAttack3` instead of the generic `J3`. 1.0.1-1.0.2 hung in the
`JAttack2` pose with no vertical drive (looked frozen).

## Known limits

- Grab (Guard + B) is replaced by Wind Press on the variant (no grab data or animations).

## Unique build identities

- Branch: `claude/karai-boss-variant-v2-us` (milestone 1: `claude/karai-boss-variant-v1-us`)
- Workflow: `build-karai-boss-variant-v2-us.yml`
- Artifact: `TMNT2-Karai-Boss-Variant-v2-US-run-N`
- EXE / PDB / MAP: `TMNT2_Karai_Boss_Variant_v2_US.{exe,pdb,map}`
- Feature flag: `karai-boss-variant`; package `Karai_Boss_Variant` 1.1.0 (milestone 2; 1.0.1-1.0.2 jump attack hung in the air; 1.0.0 froze on jump attack; 1.0.0-1.0.1 attacks could not hit enemies)

Status: implemented; requires hands-on testing.
