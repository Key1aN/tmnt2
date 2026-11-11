# TMNT2 Core: Karai Boss Variant v1.4.1 (US)

T2 Core EXE, priority 133, cumulative on Karai v1.4.0 and Debug Tools v1.2.7. LPAC v2 unchanged.

## Fixes (variant only)

- **A, A, B -> B delay.** Boss AAB (and BB, AAA, AAC) end the boss's own combos, so their motion
  parameters have no chain connect time; `CCharacter::GetNextChainMotionConnectTime` then returned the
  motion end and the next hit waited for the whole animation. New
  `CCharacter::SetChainConnectFallbackRatio`: the boss variant connects chainless motions at 45 % of
  the motion. Default 0 keeps retail for every other character. Also used by the B, B follow-up.
- **A, A, B, B, A+B Thunder finisher did nothing.** Boss C3 is only the recovery after the Thunder
  dash and has no hit. AABBC now launches the Thunder Special dash (C2 into the nearest enemy, then
  C3), skipping the charge. It neither needs nor starts the Hold B cooldown.

B, B Wind Press and the A, A, B, B slam share `ReleaseWindPress` (v1.4.0 retarget + blow-away).

## Status

Built by CI; not yet tested in game.
