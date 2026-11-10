# TMNT2 Core: Karai Boss Variant v1.2.0 + Combo Assist v1.0.0 (US)

One EXE, two packages. Both ship the same T2 Core EXE (priority 128) with capabilities
`intro-skip, msaa, brutal-difficulties, widescreen, ultimate-slashuur, debug-tools-f4,
karai-boss-variant, combo-assist`. Each package turns on only its own feature flag:

| Package | Feature flag | Contents |
| --- | --- | --- |
| `Combo_Assist_v1.0.0_US.t2mod` | `combo-assist` | EXE delta only; affects every character |
| `Karai_Boss_Variant_v1.2.0_US.t2mod` | `karai-boss-variant` | EXE delta + AFS entry 145 (LPAC v2, unchanged) |

## Combo Assist (`feature=combo-assist`, all characters)

- **A+B (C) window:** retail needs both presses on the same frame (16 ms). The second press
  is also accepted up to 85 ms after the first while the first is still held. The first press
  is never delayed (C replaces its queued request).
- **Combo late window:** retail evaluates a combo chain once, exactly at the motion's connect
  time; a press after that is dropped. The chain now stays open for 0.30 s after the connect
  time (or until the motion ends) and connects on the late press. Early presses are buffered as
  in retail. Combo unlocks (AAC, AABB, AABC, ... gated by Secret levels) are unchanged.

Without the flag both behave exactly as retail.

## Karai Boss Variant v1.2.0 (`feature=karai-boss-variant`)

| Input | Move | Change from v1.1.1 |
| --- | --- | --- |
| Hold B | Thunder Special | was Guard + A+B: C1 charge (0.75 s, three glows, electric aura); release when ready, or hold to 1.5 s, to dash (C2) and finish (C3). Released early: plain B. 3 s cooldown (was 6 s); during cooldown B is a plain B |
| A, A, A | Boss AAA | new: AA -> A plays the boss third hit (retail player chain has no AAA; it was only reachable as A,A,B,B) |
| B, B | Wind Press | unchanged |
| Guard + B | Wind Press | no longer waits for A |
| Guard + A+B | (nothing) | removed |
| Dash / Shuriken / Guard + Jump / Guard + Shuriken / Guard + Dash | unchanged | |

The Hold B Thunder finisher (C3 on release) from v1.1.1 is removed. The A+B window is now
the separate Combo Assist package.

## Breadcrumbs

`KARAI_BOSS thunder charge ready`, `KARAI_BOSS thunder from_charge=`, plus the v1.1.1 ones.

## Status

Built by CI; not yet tested in game.
