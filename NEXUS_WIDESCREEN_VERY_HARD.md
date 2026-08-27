# TMNT2 final build: Very Hard, Extreme, and Souls Like

## v2 damage-path correction

Player damage scaling now runs inside `CCharacterAttackCalculator::CalcDamage`
after the game has identified the defender as a player. This is the common
calculation used by enemy melee attacks, projectiles, guarded hits, and damaging
hitboxes. The earlier player-message multiplier was removed, so damage is
scaled exactly once.

The release build writes runtime verification to
`TMNT2_Slashuur_trace.log` beside the executable. Relevant lines begin with
`DIFFICULTY` and record:

- the option value selected and applied;
- the original and scaled HP of every initialized enemy; and
- every player hit's raw option, original Hard table row, attack power,
  multiplier, and calculated final damage.

Raw difficulty values are Easy 0, Normal 1, Hard 2, Very Hard 3, Extreme 4,
and Souls Like 5.

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
- Three new options appear after Hard in **Options > Game > Difficulty**:
  **Very Hard**, **Extreme**, and **Souls Like**.
- The original game has only three authored difficulty rows. Every new option
  therefore uses the original Hard row for boss and enemy tables, then applies
  the safe extensions shown below. This prevents out-of-range table access.

| Parameter | Hard | Very Hard | Extreme | Souls Like |
|---|---:|---:|---:|---:|
| Enemy maximum HP | 1.00x | 1.25x | 1.50x | 1.75x |
| Positive player HP damage | 1.00x | 1.50x | 2.00x | 3.00x |
| EPB attack/guard/fire and activity scale | 1.05x | 1.10x | 1.15x | 1.20x |
| Common enemy attack interval | 1.00x | 0.85x | 0.70x | 0.50x |
| Enemy no-reaction threshold | 1.00x | 0.85x | 0.70x | 0.50x |
| Starting clear-rank base | C | B | A | S |

- A shorter attack interval makes supported enemy and boss AI attack more
  frequently. A lower no-reaction threshold makes enemies enter their normal
  anti-stunlock/super-armor state after less accumulated damage.
- Positive HP damage is multiplied only at the final player damage-delivery
  path, so enemy hits, projectiles, and damaging stage hazards are covered.
  Healing and player attack damage remain unchanged.
- The three new values use the existing difficulty save field. Its layout and
  size are unchanged; existing Easy, Normal, Hard, Very Hard, and Extreme saves
  remain valid.
- Nexus mode keeps the original game's forced Normal table row, but the direct
  HP, incoming-damage, common-interval, and no-reaction extensions still apply.
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
5. Move right once past Hard for **Very Hard**, twice for **Extreme**, or three
   times for **Souls Like**.
6. Player attack damage and healing stay at their original values on all three
   new modes.
7. Select playable Slashuur and test the revised Guard combinations above.
