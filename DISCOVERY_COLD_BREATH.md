# TMNT2 Cold Breath and Developer Discovery Builds

This source update produces two separate US/NA Win32 executables. Both retain
the existing playable Slashuur and crash-reporting changes in this repository.

## Executables

### `TMNT2_Cold_Breath_US.exe`

This is the normal Release game. It restores the unused `all_breath` effect for
active player characters in the three Planet Zero stages:

- Frozen Relic (`ST12N`)
- Glacial Valley (`ST13R`)
- Mt. Zero (`ST14N`)

Breaths are one-shot effects emitted approximately every three seconds. Player
timers are staggered in multiplayer to avoid simultaneous particle bursts. The
effect already exists in every retail stage-common LPAC, so no DAT replacement
or rebuild is required.

### `TMNT2_Developer_Discovery_US.exe`

This is a separate Debug build that starts in the recovered developer menu. It
also contains the cold-breath restoration. Do not use it as the everyday game
executable.

Main developer-menu controls:

- Arrow keys or D-pad: move/change a value
- Left/right: change numbers and toggle true/false entries
- Enter, Start or controller confirm: activate a command
- Escape or controller cancel: return from supported test submenus
- `F4`: open/close the in-stage debug menu after enabling **Debug menu** on the
  first developer screen

The first developer menu appears automatically at boot. To activate the F4
runtime menu, highlight **Debug menu** and use left/right to set it to true;
Enter does not toggle boolean entries. Then launch a stage. F4 is available in
normal, ride, Nexus, demo and enemy-test gameplay sequences.

The initial menu provides movie, sound, controller, enemy and victory-animation
tests, direct area/boss launching, normal-game launch, and an all-unlocked test
launch. The in-stage `F4` menu provides God mode, HP adjustment, frame stepping,
manual camera controls, hitbox visualization, gimmick visualization, AI/spawn
information, voice-group playback and screenshots.

## Installation and safety

1. Keep a backup of the currently working `TMNT2.exe`.
2. Keep a backup of your save before running the Developer Discovery build.
3. Copy only the executable you want to test into the retail US game folder.
4. Rename that copied executable to `TMNT2.exe`.
5. Keep your current modified `TMNT.DAT`; this update does not replace it.

The Developer Discovery build can deliberately change progression, invoke test
states, or launch stages with artificial settings. Use a backup/test save and
expect some original developer tests to be unfinished.
