# Handoff

2026-10-15 (later):
- Phases 4 and 5 done (Vic confirmed in Rack). PR #7 (2.5.0): field engine core, cellular, warp/tilt, distortion, shaper.
- Next: phase 6 = Orbit left-side layout, SYNC knob (window for triggers, pull for ramps) + tuner light, RANGE 3-way switch.
  See /mnt/project-files/proposals/orbit-round-3.md and the repo's DESIGN.md.
- Then phase 7: Field expander exposing the engine.

harness/: calibrate.cpp (fractal|cells|exact ranges), spectrum.cpp + spectrum.py (bandwidth at -60 dB),
fieldcost.cpp (CPU per field setting), voicecost.cpp, parts.cpp, voiceprof.cpp, drawcount.cpp.

Typical session start:
  tools/setup-rack.sh ~/rack-src && make RACK_DIR=~/rack-src && make -C tools
  tools/render --out x --panel light --rack ~/rack-src && node tools/rasterize.mjs x-panel.svg x-view.svg
  harness/drawcount.cpp counts the display's draw calls per floor setting.
