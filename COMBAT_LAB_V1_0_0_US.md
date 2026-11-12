# TMNT2 Core: Combat Lab v1.0.0 (US)

Cumulative on Karai Boss Variant v1.4.1 (`224106c`), which already contains Debug Tools v1.2.7.
EXE-only change. No retail EXE, DAT or LPAC data is added or modified. Runtime-gated by the
existing `debug-tools-f4` feature (Combat Lab does nothing in a core without that flag).

## Existing Debug Tools architecture (what the lab plugs into)

- `src/Game/Sequence/DebugTools/DebugToolsProcess.cpp` - `CDebugToolsProcess` (PROCLABEL_DEBUGTOOLS),
  attached by `CGameMainSequence::OnAttach` when `CPCModFeatures::IsDebugToolsEnabled()`.
- One `CImpl` holds all state. Pages are an enum (`PAGE_*`) plus `s_apszPageName[]`; every page is
  served by the same `GetItemCount / FormatItemText / GetItemHelp / AdjustSelected /
  ActivateSelected / ResetSelected / ResetPage` switches and drawn by `DrawMenu` with `CDebugFontCtrl`.
- Open/close: F4 or controller Back; opening sends `TYPE_MOVE_DISABLE` to the current gameplay
  sequence (explicit pause ownership) and is only allowed in `IsGameplaySequence()` labels.
- AI controls: the **Difficulty** tab drives `CDebugDifficulty` (12 global settings incl. *AI thinking
  rate*, activity, awareness, attack/guard/projectile frequency, attack interval, knockback threshold,
  enemy HP, damage scales). Values are global (all enemies), applied live through
  `RefreshLiveEnemies()` / at spawn. The **Enemy/AI** tab has *Freeze enemy AI* (`CEnemy::StopAI`)
  and *Kill all*. **Player** has God mode (`CGamePlayer::SetDamageNegation`). **Hitboxes** uses
  `CHitDebug`. **Telemetry** has move labels, frame graph, perf/move logs.

## What was added

| Area | Change |
| --- | --- |
| Debug Tools | New last tab **COMBAT LAB** (`PAGE_COMBATLAB`, 25 items, scrolls; older tabs unchanged). Same keys/pad navigation. |
| Shortcut | **Ctrl + Shift + L** opens the existing menu directly on COMBAT LAB, or closes it if that tab is showing. Works in any gameplay sequence. F4/Back unchanged. |
| Sequence | `CCombatLabSequence` (`PROCLABEL_SEQ_COMBATLAB`): retail `CLoadStageSeqState` + `CPlayCombatLabSeqState`. |
| Runtime | `CCombatLab` (`src/Game/Component/CombatLab`): scenario, spawn, reset, survivability, AI modes, stats, log. |
| Damage hook | One call in `CCharacter::OnMessageCatchAttack` after `OnDamage` + `CalcDamage`, before `RECVDMG`. Inert unless the lab is playing. |
| Difficulty | `CDebugDifficulty::Snapshot/Restore/SetCustomValue` (no behaviour change for existing callers). |
| Launch | `TMNT2.exe --combat-lab` (or `-combatlab`), and `CombatLab.exe` launcher (`tools/CombatLabLauncher`). |

### Arena

All three choices are the retail **Battle Nexus tournament arena** (map M60X). The stage only decides
which enemy data is packed (read from the retail US TMNT.DAT stage LPACs):

| Arena | Stage | Enemies available |
| --- | --- | --- |
| Nexus Arena - Foot Clan (default) | ST60X_A01 | Foot Ninja (sword), Foot Ninja (staff), Foot Archer |
| Nexus Arena - Foot Elite | ST60X_C05 | Foot Tech Ninja, Foot Sumo Ninja |
| Nexus Arena - Foot Tech/Mech | ST60X_C01 | Foot Tech Ninja, Gunner Foot Mech |

`AddStageObjects()` creates the arena set data (player start, props) but the `G_SET` enemy
generators are never sent their ACTIVATE event (only `CPlayNexusStageSeqState` does that), so there
are no waves, intro camera, Battle Nexus info, timer, mission or result screens. Enemies are created
with `CEnemy::New` + `Start`, the same path the generators use.

### Default scenario

Leonardo, Foot Clan arena, 3 Foot Ninja (sword) in a fixed arc 4.5 units in front of the retail
player start, infinite player and enemy HP, Original (Normal) AI, crystal level 3 (complete retail
combo tree), Vanilla ruleset, fixed RNG seed `0x20041019` at every spawn/reset.

### State the lab changes and restores

On entry: `CGameData::PushRecord()`, game mode -> `GAMEMODE_NEXUS` (table difficulty Normal, save
cheats/secrets ignored, exactly like retail Nexus), option difficulty -> Normal, Debug Difficulty
snapshot, crystal level. On exit (any path): Debug Difficulty, option difficulty, game mode and the
full record are restored (`PopRecord`). The lab never runs a save sequence.

### Survivability

- Infinite player HP ON: the existing damage-negation counter (same mechanism as God mode). Hits,
  reactions, knockback, effects and SE are untouched; theoretical damage is still counted.
- Infinite player HP OFF: HP drains; a lethal hit refills HP first (counted as "KO prevented").
- Infinite enemy HP: the full retail damage message is still sent; HP is refilled only before a hit
  that would be lethal, so knockback thresholds, HP-phase logic and death checks see real damage.
- Known gap: damage that does not pass through `OnMessageCatchAttack` (death floors, Slashuur drain)
  is not intercepted.

### AI modes (reuse, no new sliders)

- **Original**: `CDebugDifficulty` PRESET_GAME with Normal -> unmodified retail Normal row.
- **Passive dummy**: `StopAI` on lab enemies every frame (same call as *Freeze enemy AI*).
- **Custom**: whatever the Difficulty tab holds. *Edit AI values* jumps to Difficulty > AI thinking
  rate. Editing a value while Original is selected relabels the mode as Custom.
- **Aggressive preset**: thinking 1.75, activity 1.50, awareness 1.50, attack frequency 1.50,
  attack interval 0.50 (HP/damage left at 1.0).
- Store/Recall keeps one named-less lab preset of all Difficulty-tab values for the session.
- Scope: `CDebugDifficulty` is global (all enemies), which is why the lab restores it on exit.
  Note: General-AI grunts read *AI thinking rate* for their decision cadence only when created, so
  use **Reset scenario** after changing it (see roadmap).

### Diagnostics

Overlay (right side): ruleset/AI, hits, guarded, ineffective, theoretical damage dealt/received,
hit chain (consecutive player hits with gaps <= 1.0 s) and max, KOs prevented, last attack motion ->
target, result type and damage, P1 motion/status/HP, every enemy's status, AI running flag and HP.
Log: `TMNT2_combatlab.log` (SESSION, SPAWN, RESET, HIT, AIMODE, RULESET, SUMMARY, RESULTS lines).
Hitbox view and enemy state labels are shortcuts to the existing Hitboxes/Telemetry settings.
Not instrumented (not reliably available from the engine without new hooks): missed attacks,
attack startup/recovery frames, input-to-action latency.

### Vanilla / Experimental

`CCombatLab::IsExperimentalCombat()` is true only inside the lab with Experimental selected. Future
features must check it **and** their own switch. No experimental feature exists yet, so the setting
currently only tags logs. It survives Reset scenario and Restore defaults.

## Build

```
cmake --preset vs2022 -DOPT_EU_BUILD=OFF -DOPT_DEBUG_TOOLS=ON
cmake --build --preset vs2022-release --parallel
```
Outputs `TMNT2.exe` and `CombatLab.exe` in `bin/win/d3d9/release/`. CI: `.github/workflows/build-combat-lab-v1-0-0-us.yml`
(branch `claude/combat-lab-v1-0-0-us`) produces `TMNT2_Core_Combat_Lab_v1_0_0_US.exe/.pdb/.map` and `CombatLab.exe`.

## Launch

- `CombatLab.exe` in the game folder (forwards extra arguments, e.g. `CombatLab.exe -wnd`).
- `TMNT2.exe --combat-lab` or `TMNT2.exe -combatlab`.
- From gameplay: Ctrl+Shift+L > *Enter Combat Lab* (press twice; quits the stage like Pause > Quit to title).

The normal save check still runs first; logos, movies, title and menus are skipped. Exit returns to Title.

## Status

Source compiled for syntax only (MinGW i686 `-fsyntax-only` against the repo headers). **Not built
with MSVC and not run in game.** See the manual test list in the delivery notes.
