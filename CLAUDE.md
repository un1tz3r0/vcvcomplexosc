# Notes for Claude

## Versioning

`plugin.json`'s `version` is `2.PHASE.PATCH`, and it is the only place the version lives: the Makefile and the
`.vcvplugin` package name read it from there.

Bump it in every commit pushed to GitHub (so every commit CI builds) that changes what ships in the plugin package:
anything under `src/` or `res/`, `plugin.json`, the `Makefile`, or the license. Commits that only touch docs, `tools/`,
`README.md`, `TODO.md` or this file leave it alone. Bump once per commit, at the highest level that applies:

1. **PHASE**: a phase of `TODO.md` is completed. It is the number of the last completed phase, so it never resets.
   A phase counts as complete only once its work is merged and tested, which means running in Rack, not just compiling.
2. **PATCH**: anything else that changes the package: bug fixes, tests passing that failed before, work in progress
   on a phase, or any other change to built code or distributed files. It counts up from 0 and goes back to 0 whenever
   PHASE changes.

For example, 2.2.0 → a fix → 2.2.1 → phase 3 completed → 2.3.0.

**Milestones and stable releases** don't get a number. Celebrate one with a tag `vX.Y.Z` on the commit with that version
and a GitHub release named after the milestone, as numbered in `TODO.md`.

The leading 2 never changes. Rack 2 refuses to load a plugin whose version doesn't start with its own major version
(`src/plugin/Plugin.cpp` in Rack). Three parts keep it in the usual MAJOR.MINOR.REVISION form.
