# TMNT2 Core: Debug Tools v1.2.7 GPU diagnostics (US)

T2 Core EXE, priority 130, capabilities `intro-skip, msaa, brutal-difficulties, widescreen,
ultimate-slashuur, debug-tools-f4, karai-boss-variant, combo-assist`. Contains everything in
Karai Boss Variant v1.3.0 and Combo Assist v1.0.0 (each still needs its own package enabled).

## Dedicated GPU on hybrid-graphics laptops (always on, every package using this core)

`TMNT2.exe` exports `NvOptimusEnablement = 1` and `AmdPowerXpressRequestHighPerformance = 1`.
NVIDIA Optimus and AMD switchable-graphics drivers read these at process start and run the game
on the dedicated GPU instead of the integrated one. No effect on desktops with one GPU.

## Display diagnostics

- Crash trace (`TMNT2_Slashuur_trace.log`): `DISPLAY engine_start ...` and `DISPLAY display_apply ...`
  with GPU name, vendor (NVIDIA/AMD/Intel) and PCI IDs, driver version, adapter, back buffer size,
  fullscreen/windowed, refresh rate, present interval (vsync), buffer count, swap effect, MSAA.
- `TMNT2_perf.log`: the same `DISPLAY start` line under the SESSION header, and `DISPLAY changed`
  whenever the resolution changes.

## Frame log fixes

- The perf SESSION header is written and flushed when Debug Tools starts (v1.2.6 wrote it only at
  the first drop, so runs with no drop over 20 ms left no trace).
- New `SUMMARY` line every 10 s of gameplay: shown FPS, game speed, average and worst frame,
  counts over 17.5 / 20 / 33 ms, average update / render / present, sequence, stage, resolution.
  Steady slowdowns below the 20 ms drop threshold now show up.
- Logs are opened shared (`_fsopen`, `_SH_DENYNO`) so other programs can read them while the game
  writes. A failed open is retried every 2 s instead of disabling the log for the session, and is
  recorded in the crash trace (`DEBUGLOG open_failed` / `open_recovered`).
- The moves log is flushed every frame cycle (it was only flushed during gameplay).
- `DEBUGTOOLS attach` breadcrumb in the crash trace confirms Debug Tools started.

## Status

Built by CI; not yet tested in game.
