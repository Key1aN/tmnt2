# TMNT2 Core: Karai Boss Variant v1.4.0 (US)

T2 Core EXE, priority 132 (capabilities `intro-skip, msaa, brutal-difficulties, widescreen,
ultimate-slashuur, debug-tools-f4, karai-boss-variant, combo-assist`). Cumulative on
Karai v1.3.1 and Debug Tools v1.2.7. LPAC v2 unchanged.

## Combo enders (variant only)

The retail player chain past A, A, B had no boss animations and reused AAA / AAC. Each step now
has its own boss move. Unlock requirements are retail (Secret levels) and unchanged.

| Input | Retail status | v1.3.x | v1.4.0 |
| --- | --- | --- | --- |
| A, A, B, B | AABB | AAA motion | **BB slam + Wind Press shockwave** (power 10 x phase scale) |
| A, A, B, B, B | AABBB | AAA motion again | **RunAttack**: faces the nearest enemy (6 u) and lunges 9 u/s for 0.3 s; retail flash + voice kept |
| A, A, B, B, A+B | AABBC | AAC + retail Karai charge wave | **C3 Thunder finisher** + electric aura and flash; no retail wave |

## Wind Press shockwave fix

The `wind_press` magic only had the player-to-enemy retarget that Slashuur's `slaball` has, so with
Karai as its parent it still targeted players and hit Karai herself (and co-op players).
`CMagic` now retargets player-parented `wind_press` to enemies and map objects only
(`FEATURE_ATTACK_TO_ENEMY_MAPOBJ`); enemy Karai keeps the retail target. The shockwave now blows
enemies away (`STATUS_FLYAWAY`, speed 10 / 5, as breakable-gimmick explosions) instead of a light
knock. Applies to B, B Wind Press and the A, A, B, B slam.

## Status

Built by CI; not yet tested in game.
