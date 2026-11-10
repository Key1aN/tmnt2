# TMNT2 Karai Boss Variant v1.1.1 US (milestone 2 fixes)

Package `Karai_Boss_Variant_v1.1.1_US.t2mod`. Same LPAC as v1.1.0 (LPAC v2, 96 chunks,
SHA-256 `489d05e0b5147686614f00a109e10279819a0c2fcd804d0f5a69e2c3df821b04`); EXE-only changes.
Cumulative on Karai Boss Variant v2 (v1.1.0) and Debug Tools v1.2.6.

## Controls (variant only)

| Input | Move | Change from v1.1.0 |
| --- | --- | --- |
| Dash | Dash Strike | was Guard + A; plain Dash now lunges (Dash1 -> Dash2, invincible) |
| B, B | Wind Press | new combo: second B during B goes into the BB slam + `wind_press` |
| Guard + B | Wind Press | unchanged (waits 85 ms for A so Guard + B then A still gives Thunder) |
| Guard + A+B | Thunder Special | unchanged (6 s cooldown) |
| Guard + Jump | Kunai Rain | spin fixed: stale FIXED_DIRECTION / FIXED_MODEL_ROTATION flags are cleared |
| Guard + Shuriken | Counter Slash | unchanged |
| Shuriken | Shuriken throw | fixed: was aliased to D (Counter Slash); now a quick A swing that throws at 0.12 s |
| Guard + Dash | Back Away | unchanged |
| Hold B | Thunder finisher | boss charge: C1 pose, TAME1-3 glows, electric aura; release plays C3 + electric burst. Retail accumulate glow and Karai charge wave are not used |
| Guard + A | nothing | swallowed so Guard + A then B forms Thunder |

The boss-variant shuriken throws from chest height (boss bone IDs differ from the
player skeleton's knife bone 3).

## Global: A+B (C) chord window (all characters, PC build)

Retail reads C only when A and B trigger on the same frame (16 ms). The manipulator now
also accepts the second press up to 85 ms after the first while the first is still held.
The first press is never delayed; C replaces its queued combo request
(`RequestAttackC` clears A/B requests), so combos keep retail latency. Requiring the first
button to be held keeps fast A, B taps (AAB) from turning into C.

## Breadcrumbs (crash report / log)

`KARAI_BOSS charge phase=`, `KARAI_BOSS attack_b level= finisher=`,
`KARAI_BOSS wind_press combo=`, `KARAI_BOSS kunai_rain landed spin_ms=`, `KARAI_BOSS move status=`.

## Status

Built by CI; not yet tested in game.
