# Complex Oscillators for VCV Rack 2

Oscillators that read their waveforms off 3D fields by sweeping rings through them, each with an OLED-style 3D view of what it is doing.

## Layout

- `src/core/`: header-only and independent of Rack. OpenSimplex2 noise, fields, rings, the phasor, a pinhole camera, the `Painter` drawing interface, and `OrbitView`, which draws a field's relief with rings lifted onto it.
- `src/OledDisplay.*`: a Rack widget that paints a `Painter` scene with NanoVG on the light layer, so it glows when the room is dimmed.
- `tools/render`: a headless renderer that uses the same core and view code, so sounds and visuals can be checked without Rack.

## Building

```sh
make RACK_DIR=/path/to/Rack-SDK   # the plugin
make -C tools                     # the renderer
```

## Renderer

```sh
tools/render --out demo --orient 30,35,20 --radii 1.4,0.8 --outputs 3 --octaves 2
```

This writes `demo.wav` (32-bit float, one channel per output), `demo-scope.svg` (two cycles) and `demo-view.svg`. With `--frames N` it writes `demo-view-0000.svg` and onward for animation.

| Option | Default | Meaning |
|---|---|---|
| `--freq` `--seconds` `--rate` | 110, 2, 48000 | Audio pitch, length and sample rate |
| `--field` | simplex3 | `simplex2`, `simplex3` or `gyroid` |
| `--seed` `--octaves` | 0, 1 | Noise seed and fractal octaves |
| `--center x,y,z` | 0,0,0 | Ring center |
| `--radii major,minor` | 1,1 | Ellipse radii |
| `--orient spin,tilt,azimuth` | 0,0,0 | Ring orientation in degrees |
| `--drift` | 0 | Center travel along Z, in units per second |
| `--mode` | phase | `phase` spreads outputs around one ring; `pinch` makes rings that meet at `--pinch-phase` |
| `--outputs` `--spread` `--pinch-phase` | 1, 0.5, 0 | Output count, pinch fan-out and meeting phase |
| `--frames` `--fps` | 1, 30 | View frames to write |
| `--cam az,el` `--cam-orbit` | -60,35, 0 | Camera angles in degrees relative to the ring's plane, and orbit speed in degrees per second |
| `--view-rate` | 0.25 | Probe speed in the view frames, in cycles per second |
