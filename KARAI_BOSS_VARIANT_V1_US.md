# TMNT2 Karai Boss Variant v1 US (milestone 1: variant only)

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

- Grab: the boss data has no catch spheres or lift/carry offsets and no lift or
  throw animations, so grabs cannot succeed; the grab input plays the aliased boss
  attack instead. Decision: replace grab with a boss move in milestone 2.
- The B charge attack is still the vanilla Karai charge; milestone 2 maps the boss
  versions.

## Not in milestone 1

Guard + button boss moves (Dash Strike, Wind Press, Thunder Special, Kunai Rain,
Counter Slash, Back Away) are milestone 2.

## Unique build identities

- Branch: `claude/karai-boss-variant-v1-us`
- Workflow: `build-karai-boss-variant-v1-us.yml`
- Artifact: `TMNT2-Karai-Boss-Variant-v1-US-run-N`
- EXE / PDB / MAP: `TMNT2_Karai_Boss_Variant_v1_US.{exe,pdb,map}`
- Feature flag: `karai-boss-variant`; package `Karai_Boss_Variant` 1.0.3 (1.0.1-1.0.2 jump attack hung in the air; 1.0.0 froze on jump attack; 1.0.0-1.0.1 attacks could not hit enemies)

Status: implemented; requires hands-on testing.
