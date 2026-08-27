# TMNT2 final Nexus + Widescreen + Extreme + Slashuur build

This is a US/NA Win32 Release build based on the stable Nexus-loading fix.

## Included

- Global animation pool increased from 384 to 512, preventing the confirmed
  Home Station/Nexus loading crash with expanded character animation packs.
- 16:9 resolutions are labeled `[16:9]` in
  **Options > Display > Resolution**.
- 3D cameras use Hor+ widescreen projection. Vertical FOV is preserved while
  horizontal FOV and the RenderWare frustum expand to the active aspect ratio.
- The original 2D HUD/menu renderer is left unchanged. UI elements stretch
  from their 4:3 layout at widescreen resolutions; this avoids the broken
  positions, clipping, transition coverage, and menu layouts caused by forcing
  the many independent 2D systems into a generic 4:3 safe area.
- **Very Hard** appears after Hard in **Options > Game > Difficulty**.
- Very Hard uses Hard enemy AI/parameter tables and multiplies every positive
  player damage event by 2.5 before HP is removed.
- **Extreme** appears immediately after Very Hard. It uses the same Hard enemy
  AI/parameter tables, halves damage dealt by player-owned melee, shots, and
  magic, and triples every positive damage event received by a player.
- The fourth and fifth difficulty values are stored in the existing difficulty
  field, so the save layout and size are unchanged. Existing
  Easy/Normal/Hard/Very Hard saves stay valid.
- Playable Slashuur's imported boss moves use the revised controls below.
- Guard + Dash can cancel Slashuur's normal combo/charged-attack states into
  the teleport strike. It cannot bypass damage, knockdown, grab, or stun states.
- Slashuur can receive damage during spinning scythe. Teleport keeps its brief
  invulnerability because the character is hidden and has no collision while
  relocating.

## Playable Slashuur boss-move controls

- Guard + Weak Attack: spinning scythe
- Guard + Strong Attack: area HP drain
- Guard + Jump: aerial ground blast
- Guard + Shuriken: purple energy ball
- Guard + Dash: teleport strike

These source changes use the boss assets already installed in the existing
modded `TMNT.DAT`. This update does not require rebuilding or replacing that
archive.

## Slashuur combo audit

Playable Slashuur registers the complete standard player combo tree. The later
branches are progression-gated, not dead code:

- A -> AA
- AA + Strong -> AAB
- AA + Weak+Strong -> AAC (Attack level 1)
- AAB + Strong -> AABB (Attack level 2)
- AAB + Weak+Strong -> AABC (Defence level 2)
- AABB + Strong -> AABBB (Aerial level 3)
- AABB + Weak+Strong -> AABBC (Charge level 3)

Boss Slashuur uses a shorter A/AA/AAB-or-AAC chain plus separate enemy-only
AI states. The boss source contains a disabled throw observer, but its Nage
animations overlap the normal playable throw system and do not form a unique
hidden combo worth enabling here.

## Explicitly excluded

- Unused cold-breath effect restoration.
- Developer/debug-menu boot mode.

The workflow builds Release configuration only. Existing playable Slashuur
source changes and release crash diagnostics already in the repository remain
available because this overlay does not replace those unrelated files.

## In-game use

1. Open **Options > Display > Resolution**.
2. Select a resolution labeled `[16:9]`, such as 1920 x 1080.
3. Confirm the Display settings.
4. Open **Options > Game > Difficulty**.
5. Move right once past Hard to select **Very Hard**, or twice to select
   **Extreme**.
6. On Extreme, verify that player-owned attacks deal half normal damage and
   every positive player damage event removes three times the normal HP.
7. Select playable Slashuur and test the revised Guard combinations above.
