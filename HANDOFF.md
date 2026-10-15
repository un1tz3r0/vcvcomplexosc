# Handoff

2026-10-15: phase 4 (Orbit controls: radius/eccentricity, pitch/roll, no ANGLE, A/B pair, XY scope) is 2.3.3 on branch
claude/project-thread-svqohi, PR open, waiting on Vic to try it in Rack; once he confirms, bump to 2.4.0 and mark phase 4
done in TODO.md. Phases 5 (display) and 6 (field engine + Field expander) are planned in the repo's DESIGN.md.

Typical session start:
  tools/setup-rack.sh ~/rack-src && make RACK_DIR=~/rack-src && make -C tools
  tools/render --out x --panel light --rack ~/rack-src && node tools/rasterize.mjs x-panel.svg x-view.svg
