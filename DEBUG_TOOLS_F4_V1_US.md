# TMNT2 Debug Tools F4 v1 US

Target: TMNT 2: Battle Nexus, US/NA 32-bit PC.

This is an EXE-only, composable T2 Mod Manager feature. It does not contain,
rebuild, or replace `TMNT.DAT` or any LPAC entry. The release build keeps the
normal retail startup flow; it does not boot into the recovered developer menu.

## Access and controls

- Press `F4` during a playable stage, Home Station, a Nexus stage, a ride stage,
  or a playable demo to open or close Debug Tools.
- `Escape`: close the menu.
- `Q` / `E`: previous or next page.
- Up / Down: select an item.
- Left / Right: adjust a value or toggle the selected option.
- Page Up / Page Down: adjust numeric values by five steps.
- Enter: run the selected action.
- Backspace: reset the selected setting.
- Delete: reset the current page.

Gameplay is paused while the menu is open. The Stage page can advance it by one
or ten frames without requiring a separate boot-time debug selection.

## Pages

### Difficulty Lab

The Game difficulty preset uses the difficulty selected in the normal game UI.
Vanilla, Very Hard, Extreme, and Souls Like can be loaded as runtime presets.
Changing any individual value switches the preset label to Custom.

Available controls:

- Enemy damage received: live damage scale applied to enemies.
- Enemy HP: scale used for enemies spawned after the change. Existing HP is
  deliberately preserved.
- Player damage received: live incoming damage scale.
- Attack, guard, projectile, and projectile-range frequency controls for the
  standard enemy parameter tables.
- AI thinking rate, activity, and front/rear awareness controls.
- Attack interval scale, applied when an enemy next assigns an AI interval.
- Knockback threshold scale.

Runtime overrides reset when the game closes. No save data is modified.

### Player

God mode, HP and shuriken refills, HP adjustment, save/restore P1 position, and
move all active players to P1.

### Enemy/AI

Freeze and resume enemy AI without restarting AI that was already stopped, and
request normal death for all active enemies.

### Stage

One-frame and ten-frame advance, normal stage-clear A/B paths, and game over.

### Camera

Gameplay camera zoom and the recovered manual, automatic, and introduction
camera modes. Widescreen projection behavior is not changed.

### Hitboxes

Attack, catch/grab, and body-collision visualization through the recovered
RenderWare debug-shape renderer. Collision data is not modified.

### Telemetry

Optional always-visible telemetry for frame rate, frame time, stage tick,
difficulty state, active enemies, P1 HP/position/status/motion, and the nearest
enemy. It can also copy a game-window screenshot to the clipboard.

## Cumulative compatibility

The build is based on the cumulative composable US core containing Ultimate
Slashuur and its buffered crash/runtime trace fix, Intro Skip v1, MSAA Frame
State Lock v4, final brutal-v3 difficulties, and Hor+ widescreen with the
intentionally stretched original UI. Anisotropic filtering, cold breath,
developer-menu boot, and the obsolete-disc-check fallback are not enabled.

The T2 Mod Manager feature flag is `debug-tools-f4`.
