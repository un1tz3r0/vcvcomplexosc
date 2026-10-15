# scratch

Orphan branch for vcvcomplexosc session tooling: never merged into main. Fetch it into a worktree beside the repo:

```sh
git fetch origin scratch && git worktree add ../scratch scratch
```

- `tools/setup-rack.sh [dir]`: builds the Rack 2 headers and a stub libRack the plugin compiles against
  (vcvrack.com and its SDK zip are blocked here). Prints the RACK_DIR to pass to make.
- `tools/build-dist.sh <repo> <rack-src> [outdir]`: portable linux-x64 `.vcvplugin` (glibc 2.27 floor) built in
  Ubuntu 18.04 with gcc-8 under Docker, copied to `/mnt/project-files/builds` by default.
- `tools/rasterize.mjs [--scale N] a.svg ...`: SVG to PNG with Playwright's Chromium, for tools/render's views and panels.
- `HANDOFF.md`: where the last session left off, when there is one.
