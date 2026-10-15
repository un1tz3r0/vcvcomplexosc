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

## Orbit's sync (phase 6)

The SYNC knob sets how tightly the input holds the oscillator, and acts only while SYNC is patched.

**Triggers** reset the phase only when it is within a window of 0, from half a cycle (hard: always) down to nothing (free). Tuned near a harmonic of the input, the drift per cycle stays inside the window, so it snaps every cycle and stays locked: the window is the capture range. Tuned away, it runs free and snaps only on the cycles where the phases happen to come round. A snap moves the phase by no more than the window, so softer is also quieter, and each reset is band-limited with a minBLEP.

**Ramps** read as phase at 10 V per cycle, so a 0–10 V phasor and a ±5 V saw both work: the saw's edge drops exactly one cycle. A window on a continuous phase would latch on for good after its first snap, so for a ramp the knob sets a pull instead. At hard the phase follows the ramp and FREQ is ignored; softer, it locks when tuned within the pull's reach, with a phase offset that grows with the detune, and slips smoothly otherwise. An input counts as a ramp when it moves steadily one way between jumps; a menu item can force either reading.

**The light** shows the phase error at each sync event, or continuously against a ramp, which makes it a tuner: green on any whole-number ratio to the input, red when the input lags, yellow when it leads, brighter the larger the error. The hues stay distinct rather than blending, since a red-green blend passes through yellow.

## Field engine and the Field expander (phase 7)

**Fields.** Base fields are simplex 3D and 2D, the gyroid, and cellular noise. Cellular noise has F1, F2, F2−F1, smooth F1 and cell value, which is a random value per cell blended smoothly by distance, on a jittered cubic or BCC lattice. Any base field can be stacked into fractal octaves as fBM, ridged, hetero terrain or hybrid multifractal, with a continuous DETAIL whose top octave fades in, ROUGHNESS and LACUNARITY. Hetero terrain is a fractal type, as in Blender: each octave counts in proportion to the value so far, so valleys stay smooth and peaks get rough.

**Levels.** Every setting spans about [−1, 1] whatever its detail and roughness, so sweeping them changes the texture, not the level. fBM is divided by its RMS amplitude rather than its total, which would make it quieter with each octave; the multifractals track their expected value as they sum; ridged gets an empirical correction. Cellular distances are scaled per metric and lattice so they all spread like Euclidean F1 on a cubic lattice.

**Smooth cells.** Smooth F1 is a soft minimum, and cell value a blend, over a compact kernel `(1 − d/s)³` that reaches the smoothness `s` past the nearest point, instead of the usual exponential. The exponential gives weight to points outside the 27 searched cells, so the field stepped each time the search moved: audible clicks. The compact kernel misses a point only when one lies unusually far, under 0.3% of the time at the maximum smoothness of 0.3. Its 1/3 weight on the log makes the minimum smooth where two points tie.

**The second noise.** An independently seeded vector noise at its own scale: three components displace space by WARP, and the fourth scales ROUGHNESS by up to four times either way at TILT 1, for patches of smoother and grittier texture. Modulating the scale instead would move each point by `p·(s − 1)`, which grows with distance from the origin. It drifts by sliding along its own Z rather than through a fourth dimension: the ring and the floor are slices, so a slide reads as evolution at a third of a 4D read's cost.

**Distortion.** Twist, swirl, mirror-fold and a sine warp bend space before sampling; only Orbit can do these. A bypassable value shaper stage follows the field: fold, sine, clip and a rectifier with rounded corners, which keep its spectrum finite.

**Aliasing.** Each setting's spectrum is measured along lines, and `bandwidth()` gives where it falls to −60 dB, in cycles per unit: simplex 4.4, gyroid 1.4, F1 15, F2 22, F2−F1 beyond 32 (its creases are sharp), smooth F1 6.5 to 10 and cell value 6 to 24, both by smoothness. Shapers multiply it by 1.5 to 4 at drive 2. The octave fade and oversampling use it, so creased features oversample sooner.

**CPU.** At 880 Hz with auto oversampling, one voice costs about 7% of a core on simplex, 10% with warp and tilt, 14% on smooth cellular, and 25% on cellular with 4 octaves. SIMD across poly channels was the plan, but the noise hash's 64-bit multiply has no SSE4 vector form, so it would need a 32-bit hash that changes every seed's sound. 3D simplex is branch-free instead, with identical output, which saves about 40% of a read where the reads jump around, as cellular and warped reads do.

**The expander.** Orbit's panel is full, so the field's controls go on a separate Field module. Placed to Orbit's right, it supplies Orbit's field. Standalone, it reads its field at its X/Y/Z inputs, so patching Orbit's probe outputs into it plays Orbit's sound. Epicycle and Curve can use it too. Without it, Orbit keeps a default field.
