# Handoff

2026-10-15:
- Phase 4 (Orbit controls: radius/eccentricity, pitch/roll, no ANGLE, A/B pair, XY scope) merged as 2.3.3 (PR #5).
  Vic merged without saying it ran in Rack; once he confirms, a commit bumping to 2.4.0 marks phase 4 done in TODO.md.
- Phase 5 (mesh floor rows/grid/dots, hue shift bands, velocity arrows) is 2.3.4 in PR #6, waiting on Vic.
- Phase 6 next: SIMD across poly first, then fractal controls, cellular, domain distortion, value shaper, Field expander
  (see the repo's DESIGN.md).

Typical session start:
  tools/setup-rack.sh ~/rack-src && make RACK_DIR=~/rack-src && make -C tools
  tools/render --out x --panel light --rack ~/rack-src && node tools/rasterize.mjs x-panel.svg x-view.svg
  harness/drawcount.cpp counts the display's draw calls per floor setting.
