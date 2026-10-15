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

## Phase 3: Orbit revisions (done)

- Drop SPIN, which only duplicates ANGLE on a circular ring, for ROTATE: a rotation rate about an AXIS selector's choice of the ring's own axes or the world's.
- Spread modes beyond phase and pinch: stack (along the normal), radial (concentric) and fan (hinged on a diameter).
- Tested in Rack on linux-x64.

## Phase 4: Orbit controls (done)

The knob set reworked so every knob has its own audible effect (see `DESIGN.md`).

- RADIUS and signed ECCENTRICITY replace R MAJOR and R MINOR.
- PITCH and ROLL, turns about world X then Y, replace TILT and AZIMUTH, which overlapped at tilt 0.
- ANGLE goes: it was a phase offset. Pinch and fan hinge on the ring's phase-0 point.
- Outputs 1–4 become an A/B pair: A reads the ring as the knobs set it, B the ring moved by a bipolar SPREAD in MODE's way.
- An XY scope of A against B beside the 3D view.
- Tested in Rack.

## Phase 5: Orbit display (done)

- A brighter floor drawn as a mesh: rows, a grid or dots, with density and brightness in the context menu.
- An optional hue shift with height.
- Velocity arrows on the corner axes for the center's motion.
- Tested in Rack.

## Phase 6: Orbit layout and sync

- Global controls on the left of the screen: the selectors, V/OCT and SYNC. Smaller knobs under the screen make it taller; the side knobs grow.
- A SYNC knob from hard to soft, with a light that shows the oscillator's phase against the input's (see `DESIGN.md`).
- RANGE becomes a three-position switch: VLFO, LFO and audio.

## Phase 7: field engine and the Field expander

- Fractal controls: continuous DETAIL, ROUGHNESS, LACUNARITY, and a fractal type (fBM, ridged, hetero terrain, hybrid multifractal).
- Cellular noise: F1, F2, F2−F1, smooth F1 and smoothly blended cell values on a jittered cubic or BCC lattice.
- A second noise: warp with its own seed, scale and drift, and TILT, which varies the roughness from place to place.
- Domain distortion: twist, swirl, mirror-fold, sine warp.
- A bypassable value shaper stage.
- A Field expander module that supplies Orbit's field and its controls, and works standalone as a noise field with X/Y/Z inputs.

## Phase 8: Epicycle and Curve

- Epicycle: sums of rotating vectors.
- Curve: maps a 0–1 CV onto a cycloid or Hilbert curve, optionally driving 2D noise.

## Phase 9: polish

- DC handling, CPU profiling, and CI.
