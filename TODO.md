# TODO

Phases are numbered here because the plugin's version tracks them, and milestones because releases are named after them (see `CLAUDE.md`).

## Milestones

None reached yet. The first stable release will be milestone 1.

## Phase 1: scaffold and core (done)

- Plugin scaffold, the header-only core (OpenSimplex2 fields, rings, the phasor), the OLED painter and the headless renderer.

## Phase 2: Orbit (done)

- The Orbit oscillator with its 3D OLED display.
- A Fundamental and vcvspeak style panel with knobs wired to their values on the screen.
- Tested in Rack on linux-x64.

## Phase 3: Epicycle and Curve

- Epicycle: sums of rotating vectors.
- Curve: maps a 0–1 CV onto a cycloid or Hilbert curve, optionally driving 2D noise.

## Phase 4: polish

- Anti-aliasing and DC handling, CPU profiling, and CI.
