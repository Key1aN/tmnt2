# T2Mod Composable Core v1 US

This US/NA x86 executable is the shared runtime used by composable `.t2mod`
packages. The launcher selects this core when any compatible standard feature
package is active and writes `.t2-mod-manager/features.txt` beside the game.

The following features are compiled into the core but remain disabled unless
their matching package flag is present:

- `intro-skip`: preserves the save check while skipping only the startup logos
  and opening movie.
- `msaa`: exposes the confirmed Frame State Lock v4 OFF/2X/4X/8X setting.
- `brutal-difficulties`: exposes the final Very Hard, Extreme, and Souls Like
  modes.
- `widescreen`: enables Hor+ 3D rendering with the intentionally stretched
  original UI.

Missing or invalid feature state disables every optional feature. The build
keeps the buffered crash/runtime diagnostics shared by the confirmed feature
builds. It does not include playable Slashuur, anisotropic filtering, cold
breath, the developer-menu boot, or an obsolete-disc-check replacement.

`TMNT.DAT` is not built or changed by this project.
