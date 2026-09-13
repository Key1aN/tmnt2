# Slashuur Dual Costumes v1 US

This US/NA x86 build extends the cumulative Ultimate Slashuur mod with two
selectable gameplay costumes:

- Normal selection: the existing playable white Slashuur.
- Hold R1 while confirming Slashuur: the prototype-inspired red, charcoal,
  gold, and burgundy costume with its alternate model.

Both costumes use the existing Slashuur skeleton, motion set, five boss moves,
direct bone ID 3 crash fix, final controls, and Adaptive Teleport v2 with the
25-unit ground-target search and flying-archetype exclusions. The alternate
costume changes only the gameplay model, texture, and bandana color; it does
not change attack behavior or animation data.

The package replaces only the Slashuur LPAC entry inside `TMNT.DAT`; it does
not distribute or replace a complete retail `TMNT.DAT`. The expanded LPAC
keeps the verified 77-entry Slashuur animation/effect payload and adds the
alternate model plus its matching motion-parameter record.

This cumulative source also retains buffered crash/runtime logging, the
512-entry Nexus/Home Station animation pool, and the runtime-gated standalone
Intro Skip, MSAA Frame State Lock v4, final difficulty, and Hor+ widescreen
features. It does not include anisotropic filtering, cold breath, the
developer-menu boot, or an obsolete-disc-check replacement.

The alternate costume applies to gameplay and character selection. Result/
victory scenes continue to use the game's separate `sls` result-scene asset.
