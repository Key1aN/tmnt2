# Slashuur diagnostic build

This build writes crash diagnostics beside `TMNT2.exe`.

## Files created after a crash

- `TMNT2_Slashuur_trace.log` — the last completed move/resource checkpoint.
- `TMNT2_crash_YYYYMMDD_HHMMSS_mmm.log` — exception code, address, RVA, access type, and x86 registers.
- `TMNT2_crash_YYYYMMDD_HHMMSS_mmm.dmp` — Windows minidump for exact symbol analysis.

The GitHub artifact also contains `TMNT2.pdb` and `TMNT2.map`. They are analysis
files and do not need to be copied into the game directory.

## Reproduction procedure

1. Keep the already-modded `TMNT.DAT`/`TMNT.afs` installed.
2. Back up the previous modded executable.
3. Copy `TMNT2_Playable_Slashuur_Diagnostics_US.exe` to the game directory and
   rename it to `TMNT2.exe`.
4. Start a stage with playable Slashuur.
5. Test only one failing move during that game launch.
6. After the crash, ZIP the trace log, newest crash log, and matching minidump.
7. Upload that ZIP together with the downloaded GitHub artifact.

Start with Guard + Strong Attack (scythe), because it appears to fail earliest
inside one of the newly imported animation/effect paths. Repeat with a fresh
launch for Ground Blast, Shot, and Drain after the first crash is analyzed.
