# TMNT2 Core: Karai Boss Variant v1.3.0 (US)

T2 Core EXE, priority 129, capabilities `intro-skip, msaa, brutal-difficulties, widescreen,
ultimate-slashuur, debug-tools-f4, karai-boss-variant, combo-assist`. Combo Assist v1.0.0
(core priority 128) keeps working: the manager picks this higher-priority core, which still
contains the `combo-assist` code. LPAC v2 unchanged.

## Controls (variant only)

| Input | Move | Change from v1.2.0 |
| --- | --- | --- |
| B (tap) | plain B | the B windup is held like retail; the charge only starts after 0.25 s held |
| Hold B | Thunder Special | Thunder charge (C1, three glows, aura) starts at 0.25 s held; ready 0.75 s later; release (or hold 1.5 s) to dash + finish |
| B, B | Wind Press | only input for Wind Press now |
| Guard + A | Counter Stance | new: G1/G2 guard pose, 0.4 s window. A hit in the window does nothing to her; she answers with Counter Slash (final phase: Thunder Special, which starts its cooldown) |
| Guard + B | Kunai Fan | new (boss C2 volley): hop, 6 kunai over a 162 deg fan aimed at the nearest enemy, land |
| A, A, A / Dash / Shuriken / Guard + Jump / Guard + Shuriken / Guard + Dash | unchanged | |

B swing sound fixed: the B motion now starts before the base attack attach, which picks the
swing sound from the current motion name (it was the charge pose).

## HP phases (her own HP, boss Karai thresholds)

| Phase | HP | Aura (boss effects) | Thunder cooldown | Kunai Rain | Damage |
| --- | --- | --- | --- | --- | --- |
| Base | > 70% | none | 5.0 s | 12 | 60% of boss |
| Warning | <= 70% | `i_cryshine_r` (red stars), flash on entry | 3.5 s | 18 | 60% |
| Final | <= 35% | `stup_attack` (red circle), flash on entry | 1.5 s | 18 | 80% |

Healing (pizza) drops her back a phase. Wind Press power is 10 x the phase scale. Both aura
effects are in the common effect set, so they appear on every stage. As with the boss, the
red circle replaces the red stars in the final phase.

## Thunder countdown

`THUNDER x.x` (orange) above her head while recharging, then `THUNDER READY` (cyan) for 1.5 s.
Drawn from the HUD pass (`CGaugeManager::Draw`), for boss-variant Karai only.

## Breadcrumbs

`KARAI_BOSS phase= hp_pct=`, `KARAI_BOSS counter hit phase=`, `KARAI_BOSS thunder ...`.

## Status

Built by CI; not yet tested in game.
