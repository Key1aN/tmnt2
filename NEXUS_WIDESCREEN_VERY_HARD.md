# TMNT2 Nexus + Widescreen + Very Hard build

This is a US/NA Win32 Release build based on the stable Nexus-loading fix.

## Included

- Global animation pool increased from 384 to 512, preventing the confirmed
  Home Station/Nexus loading crash with expanded character animation packs.
- 16:9 resolutions are labeled `[16:9]` in
  **Options > Display > Resolution**.
- 3D cameras use Hor+ widescreen projection. Vertical FOV is preserved while
  horizontal FOV and the RenderWare frustum expand to the active aspect ratio.
- **Very Hard** appears after Hard in **Options > Game > Difficulty**.
- Very Hard uses Hard enemy AI/parameter tables and multiplies every positive
  player damage event by 2.5 before HP is removed.
- The fourth difficulty value is stored in the existing difficulty field, so
  the save layout and size are unchanged. Existing Easy/Normal/Hard saves stay
  valid.

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
5. Move right once past Hard to select **Very Hard**.
