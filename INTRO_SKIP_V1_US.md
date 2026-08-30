# TMNT2 Intro Skip v1 US

This controlled EXE-only build skips the mandatory publisher/developer logo
sequence and the startup opening movie, then enters the normal title screen.

## Exact behavior

The retail startup route is:

```text
Save-data check -> Konami logo -> Studio logo -> OP_TMNT2.sfd -> Title
```

This build changes only the post-save-check branch:

```text
Save-data check -> Title
```

The save-data check still runs normally. The title screen, title music, menu,
idle attract-mode movies, story cutscenes, stage movies, endings, and credits
remain available.

A buffered release breadcrumb confirms the redirect without opening or flushing
the trace file every frame:

```text
INTRO_SKIP startup logos=skipped opening_movie=skipped next=title
```
## Project scope

- **Project:** `TMNT2_Intro_Skip_v1_US`
- **Target:** US/NA Win32 Release
- **Archive changes:** none
- **Required game data:** keep the existing final modded `TMNT.DAT`
- **Base commit:** cumulative pre-AF MSAA Frame State Lock v4 baseline
- **Anisotropic filtering:** not included

No `.anm`, `.chr`, LPAC, AFS, `TMNT.DAT`, or `TMNTE.DAT` file is
included or modified.

## Preserved cumulative baseline

- Confirmed MSAA frame-state-lock v4 and its in-game Display selector.
- Playable Slashuur's five boss moves, final controls, direct bone ID 3 crash
  fix, teleport cancel restrictions, vulnerable scythe, and short teleport
  invulnerability.
- One-handle buffered runtime/crash logging, flushed only on crash or shutdown.
- Home Station/Nexus animation pool increased from 384 to 512.
- Correct Hor+ widescreen 3D FOV and frustum/culling.
- Original intentionally stretched widescreen UI.
- Final brutal-v3 Very Hard, Extreme, and Souls Like balance and safe mapping
  to the original Hard table row.
- Nexus's original forced-Normal behavior and the proven incoming-damage hook
  in `CCharacterAttackCalculator::CalcDamage`.
- No cold-breath restoration and no developer-menu boot.

## Download with GitHub Actions

1. Open the repository's **Actions** page.
2. Select **Build TMNT2 Intro Skip v1 US**.
3. Open the newest successful run from `feature/intro-skip-v1-us`.
4. Download the artifact beginning with
   `TMNT2-Intro-Skip-v1-US-run-`.
5. Confirm that it contains:
   - `TMNT2_Intro_Skip_v1_US.exe`
   - `TMNT2_Intro_Skip_v1_US.pdb`
   - `TMNT2_Intro_Skip_v1_US.map`
   - `BUILD_INFO_TMNT2_Intro_Skip_v1_US.txt`

Keep the matching PDB and MAP in the downloaded package for crash analysis.
They do not need to be copied into the game directory.

## GitHub Desktop and local build

1. Open the existing `tmnt2` repository in GitHub Desktop.
2. Click **Fetch origin**.
3. Open **Current branch**, search for
   `feature/intro-skip-v1-us`, and select it.
4. Click **Pull origin** if shown.
5. For a local build, open a Visual Studio 2022 Developer PowerShell in the
   repository folder and set `DXSDK_DIR` to a compatible DirectX SDK.
6. Run `cmake --preset vs2022 -DOPT_EU_BUILD=OFF`.
7. Run `cmake --build --preset vs2022-release --parallel`.

GitHub Actions is easier because the workflow stages and verifies the required
DirectX headers and x86 libraries automatically.

## Installation

1. Back up the working `TMNT2.exe`.
2. Keep the current final modded `TMNT.DAT`; do not replace or rebuild it.
3. Copy `TMNT2_Intro_Skip_v1_US.exe` into the game directory.
4. Rename the copied file to `TMNT2.exe`.
5. Start the game normally.

## Controlled test

1. Start the game from a full shutdown.
2. Confirm that the save-data check still completes.
3. Confirm that the game goes directly to the normal title screen without the
   Konami logo, studio logo, or startup opening movie.
4. Confirm title input and the main menu work.
5. Optionally wait at the title screen long enough for attract mode to verify
   that its movie still plays.
6. Exit normally to flush the buffered trace.

The trace should begin with:

```text
BUILD TMNT2_Intro_Skip_v1_US
INTRO_SKIP startup logos=skipped opening_movie=skipped next=title
```
