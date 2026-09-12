# T2Mod Composable Core Slashuur FPS Fix v1 US

This higher-priority US/NA x86 core contains the complete confirmed Ultimate
Slashuur implementation and the same runtime-gated features as the standard
Composable Core v1.

Ultimate Slashuur remains active whenever this core is selected. Intro Skip,
MSAA Frame State Lock v4, the final three brutal difficulties, and Hor+
widescreen remain independently controlled by the launcher's feature-state
file. The final expanded 77-chunk Slashuur LPAC is supplied by the
`Ultimate_Slashuur.t2mod` package and is not stored in this source repository.

The Slashuur feature set includes the five boss moves, direct bone ID 3 crash
fix, 512-entry Nexus/Home Station animation pool, final controls, and Adaptive
Teleport v2 with radius 25 and flying-archetype exclusions.

Runtime breadcrumbs now keep their trace handle open and use buffered writes.
The logger flushes on a crash, fatal report, and clean shutdown instead of
forcing a physical disk flush for every special-attack event. This removes the
measured 15-25 ms main-thread stalls without changing any move behavior.

This build does not include anisotropic filtering, cold breath, the developer-
menu boot, or an obsolete-disc-check replacement. It does not build or contain
`TMNT.DAT`.
