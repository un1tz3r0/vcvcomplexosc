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

## Phase 4: Orbit controls

The knob set reworked so every knob has its own audible effect (see `DESIGN.md`).

- RADIUS and signed ECCENTRICITY replace R MAJOR and R MINOR.
- PITCH and ROLL, turns about world X then Y, replace TILT and AZIMUTH, which overlapped at tilt 0.
- ANGLE goes: it was a phase offset. Pinch and fan hinge on the ring's phase-0 point.
- Outputs 1–4 become an A/B pair: A reads the ring as the knobs set it, B the ring moved by a bipolar SPREAD in MODE's way.
- An XY scope of A against B beside the 3D view.

## Phase 5: Orbit display

- A brighter floor drawn as a mesh: rows, a grid or dots, with density and brightness in the context menu.
- An optional hue shift with height.
- Velocity arrows on the corner axes for the center's motion.

## Phase 6: field engine and the Field expander

- SIMD across poly channels first, to pay for what follows.
- Fractal controls: continuous DETAIL, ROUGHNESS, LACUNARITY, and a fractal type (fBM, ridged, hetero terrain, hybrid multifractal).
- Cellular noise: F1, F2, F2−F1, smooth F1 and smoothly blended cell values on a jittered cubic or BCC lattice.
- Domain distortion: noise warp with its own seed and scale, twist, swirl, mirror-fold, sine warp.
- A bypassable value shaper stage.
- A Field expander module that supplies Orbit's field and its controls, and works standalone as a noise field with X/Y/Z inputs.

## Phase 7: Epicycle and Curve

- Epicycle: sums of rotating vectors.
- Curve: maps a 0–1 CV onto a cycloid or Hilbert curve, optionally driving 2D noise.

## Phase 8: polish

- DC handling, CPU profiling, and CI.
