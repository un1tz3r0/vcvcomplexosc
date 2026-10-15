# Design notes

Decisions behind the modules that the code alone doesn't explain. Phases refer to `TODO.md`.

## Orbit's controls (phase 4)

**Every knob has its own audible effect.** A ring in space has eight degrees of freedom: three of position, three of orientation and two of size. But turning a circle within its own plane only shifts its phase, which on a free-running oscillator is inaudible except against SYNC. Phase 3's knobs had three ways to make that turn (AZIMUTH at tilt 0, ANGLE, and ROTATE about the normal), so phase 4 keeps none as a knob:

| Knob | Effect | Quiet when |
|---|---|---|
| FREQ | pitch | never |
| X, Y, Z | slides the ring through the field | never |
| RADIUS | ring size: more field per cycle, so brighter | never |
| ECCENTRICITY | ellipse shape | never |
| PITCH | turns the ring about world X | never |
| ROLL | then about world Y | at PITCH ±90° |
| SPREAD | how far B is from A | 0, by design |
| DRIFT | Z velocity | never |
| ROTATE | turn rate about AXIS | about NORMAL on a circle it's a small linear detune |

Any two angles that point a normal over the sphere have a dead spot somewhere, so a knob may go quiet at an extreme or on a degenerate shape, as long as no two knobs ever do the same thing. Tilt and azimuth put their dead spot at the default; pitch and roll put it at the end of PITCH's travel.

**Radius and eccentricity.** The radii are `R·(1+e)` along the ring's X axis and `R·(1−e)` along its Y, with e from −1 to 1. R is the mean radius, so the ring's detail density, and with it the brightness, holds roughly steady across a sweep of e. At ±1 the ring collapses to a line and plays a mirrored wave. Negative e swaps the axes; that overlaps a quarter turn in the plane, but it lets one bipolar CV sweep a wide ellipse through the circle to the perpendicular one.

**The A/B pair.** A reads the ring as the knobs set it. B reads it moved by SPREAD, from −1 to 1, in MODE's way:

- Phase: the same ring, `spread/2` of a cycle ahead.
- Pinch: scaled by `2^spread` about the ring's phase-0 point, so A and B touch there.
- Stack: shifted `spread·R` along the normal.
- Radial: scaled by `2^spread` about the center.
- Fan: turned `spread·90°` about the diameter through phase 0, so A and B cross at both its ends.

In every mode but phase, B is read in step with A, so they reach phase 0 together, and SYNC resets both to it. SPREAD is the interpolation between A and B: a third output part way between would only be B at a smaller SPREAD.

**XY scope.** A against B over one cycle, computed from the field like the waveform strip, so it stays sharp at any pitch. Each axis is fitted to its box unless equal scaling is on.

Param ids changed from phase 3 and patches made before it load with wrong values. There had been no release, so there's no migration.

## Orbit's display (phase 5)

The floor is a disc of square cells, cut into rows along the screen's horizontal and painted far to near. Each row is filled down to a skirt below the plane, hiding what it stands in front of, and the grid's columns between it and the row behind are drawn just before it, so one fill per row still occludes the whole mesh. A hue shift bins heights into 4 to 8 bands, so each row's lines go out as one batch per band instead of a stroke per segment. At the module's size, the default grid with a subtle shift costs about twice the draw calls of phase 4's rows for the same number of vertices. The velocity arrows show the center's velocity low-passed at 4 Hz, which averages audio-rate CV out to its net motion.

## Field engine and the Field expander (phase 6)

**Fields.** Base fields are simplex 3D and 2D, the gyroid, and cellular noise. Cellular noise has F1, F2, F2−F1, smooth F1 and cell value, which is a random value per cell blended smoothly by distance, on a jittered cubic or BCC lattice. Any base field can be stacked into fractal octaves as fBM, ridged, hetero terrain or hybrid multifractal, with a continuous DETAIL whose top octave fades in, ROUGHNESS and LACUNARITY. Hetero terrain is a fractal type, as in Blender: each octave counts in proportion to the value so far, so valleys stay smooth and peaks get rough.

**Distortion.** Domain distortion bends space before sampling: a vector noise warp with its own seed and scale, twist, swirl, mirror-fold and a sine warp. Only Orbit can do these. A bypassable value shaper stage follows the field.

**Aliasing.** F1, F2 and F2−F1 have creases whose harmonics fall off at only 12 dB per octave, so each feature's spectrum is measured, as simplex's was, before setting its `detail()`. Smooth F1 is the cellular default.

**CPU.** Each octave costs a noise read, a warp three more, and 3D cellular about two to four simplex reads. SIMD across poly channels, four voices per call with Rack's `float_4`, comes first.

**The expander.** Orbit's panel is full, so the field's controls go on a separate Field module. Placed to Orbit's right, it supplies Orbit's field. Standalone, it reads its field at its X/Y/Z inputs, so patching Orbit's probe outputs into it plays Orbit's sound. Epicycle and Curve can use it too. Without it, Orbit keeps a default field.
