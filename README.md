# scratch

Orphan branch for vcvcomplexosc session tooling: never merged into main. Fetch it into a worktree beside the repo:

```sh
git fetch origin scratch && git worktree add ../scratch scratch
```

- `tools/setup-rack.sh [dir]`: builds the Rack 2 headers and a stub libRack the plugin compiles against
  (vcvrack.com and its SDK zip are blocked here). Prints the RACK_DIR to pass to make.
- `HANDOFF.md`: where the last session left off, when there is one.
