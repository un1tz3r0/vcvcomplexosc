# Handoff

2026-10-15 (later):
- Phases 4 and 5 done (Vic confirmed in Rack). PR #7 (2.5.0): field engine core, cellular, warp/tilt, distortion, shaper.
- Phase 6 (left layout, LOCK knob + tuner light, VLFO/LFO/AUDIO switch) is PR #8 (2.5.1); 2.6.0 once Vic confirms in Rack.
- Then phase 7: Field expander exposing the engine.

harness/: calibrate.cpp (fractal|cells|exact ranges), spectrum.cpp + spectrum.py (bandwidth at -60 dB),
fieldcost.cpp (CPU per field setting), synccheck.cpp (sync reset aliasing with exact edge times), voicecost.cpp, parts.cpp, voiceprof.cpp, drawcount.cpp.

Typical session start:
  tools/setup-rack.sh ~/rack-src && make RACK_DIR=~/rack-src && make -C tools
  tools/render --out x --panel light --rack ~/rack-src && node tools/rasterize.mjs x-panel.svg x-view.svg
  harness/drawcount.cpp counts the display's draw calls per floor setting.
