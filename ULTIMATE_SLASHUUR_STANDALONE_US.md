# Ultimate_Slashuur standalone US build

This branch exists only to generate the EXE delta used by
`community-mod-manager-v0.1` package `Ultimate_Slashuur`.

## Included

- Playable Slashuur and all five confirmed boss moves.
- Final Guard-based controls.
- Direct bone ID 3 fix for purple-ball and ground-blast stability.
- Buffered crash/runtime logging.
- Nexus/Home Station animation pool capacity of 512.
- Adaptive Teleport v2, radius 25, rear offset 1, collision and height
  correction, original forward fallback, handle revalidation, attack cancel
  rules, and the eight confirmed flying-archetype exclusions.

## Deliberately excluded

- MSAA.
- Intro Skip.
- Widescreen changes.
- Very Hard, Extreme, or Souls Like difficulty changes.
- Anisotropic filtering.
- Cold breath discovery changes.
- Developer-menu boot.

The workflow produces uniquely named EXE, PDB, MAP, build-information, and
artifact outputs. The retail executable is not committed or distributed.
