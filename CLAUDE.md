# Notes for Claude

## Versioning

`plugin.json`'s `version` is `2.MILESTONE.PHASE.PATCH`, and it is the only place the version lives: the Makefile and the
`.vcvplugin` package name read it from there.

Bump it in every commit pushed to GitHub (so every commit CI builds) that changes what ships in the plugin package:
anything under `src/` or `res/`, `plugin.json`, the `Makefile`, or the license. Commits that only touch docs, `tools/`,
`README.md`, `TODO.md` or this file leave it alone. Bump once per commit, at the highest level that applies:

1. **MILESTONE**: a milestone is reached or a stable release goes out. It is the number of the last milestone reached,
   as numbered in `TODO.md`; a stable release that isn't one of those takes the next number. Celebrate it.
2. **PHASE**: a phase of `TODO.md` is completed. It is the number of the last completed phase. A phase counts as
   complete only once its work is merged and tested, which means running in Rack, not just compiling.
3. **PATCH**: anything else that changes the package: bug fixes, tests passing that failed before, work in progress
   on a phase, or any other change to built code or distributed files. It counts up from 0 and goes back to 0 whenever
   MILESTONE or PHASE changes.

MILESTONE and PHASE track numbers in `TODO.md`, so they never reset. For example, 2.0.1.2 → phase 2 completed → 2.0.2.0
→ a fix → 2.0.2.1 → milestone 1 reached after phase 4 → 2.1.4.0.

The leading 2 never changes. Rack 2 refuses to load a plugin whose version doesn't start with its own major version
(`src/plugin/Plugin.cpp` in Rack), so the scheme starts at the second number. Rack compares any number of parts
numerically, but the VCV Library may expect three-part versions, so check its rules before submitting to it.
