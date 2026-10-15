#pragma once
#include <array>
#include <cstdio>
#include "Field.hpp"
#include "OrbitPanel.hpp"
#include "OrbitView.hpp"

namespace cxo {

constexpr double C4 = 261.6256;

// Everything the Orbit display needs to know about the oscillator at one moment.
struct OrbitState {
	OrbitShape shape{Ring{{}, {}, 1.2, 0.8}};
	double pitch = 0, roll = 0; // radians: the ring plane's orientation before rotation, which the camera rides with
	Ring::Axis axis = Ring::NORMAL;
	StockField field;
	double hz = C4, phase = 0;
	Vec3 velocity; // the center's, in units per second
	int channels = 1;
	orbit::Range range = orbit::AUDIO;
	std::array<float, orbit::MODS> values = defaults(); // each knob with its CV, as the display shows it: Hz, degrees, percent

	static std::array<float, orbit::MODS> defaults() {
		std::array<float, orbit::MODS> v;
		for (int i = 0; i < orbit::MODS; ++i)
			v[i] = i == orbit::FREQ_PARAM ? float(C4 * std::exp2(orbit::MOD[i].def)) : orbit::MOD[i].def * orbit::MOD[i].scale;
		return v;
	}
};

// A frequency in at most six characters.
inline std::string hertz(double hz) {
	char s[16];
	std::snprintf(s, sizeof s, hz < 10 ? "%.3f" : hz < 100 ? "%.2f" : hz < 1000 ? "%.1f" : hz < 1e4 ? "%.2fk" : "%.1fk", hz < 1000 ? hz : hz / 1000);
	return s;
}

// A param's value as the display shows it where the param's wire meets the screen.
inline std::string readout(int param, const OrbitState& s) {
	char text[16];
	switch (param) {
		case orbit::FIELD_PARAM: return StockField::NAMES[s.field.kind];
		case orbit::OCTAVES_PARAM: std::snprintf(text, sizeof text, "%d OCT", s.field.octaves()); return text;
		case orbit::MODE_PARAM: return OrbitShape::MODE_NAMES[s.shape.mode];
		case orbit::AXIS_PARAM: return Ring::AXIS_NAMES[s.axis];
		case orbit::RANGE_PARAM: return orbit::RANGE_NAMES[s.range];
		case orbit::FREQ_PARAM: return hertz(s.values[param]);
		default: std::snprintf(text, sizeof text, orbit::MOD[param].format, s.values[param]); return text;
	}
}

// The Orbit display's composition: the 3D view beside a side panel of the unrolled waveforms, the XY scope or both,
// the frequency in the corner, and every tapped param's value where its wire meets the screen.
// Shared by the module's widget and tools/render.
struct OrbitScene {
	enum Side { OFF, WAVES, XY, BOTH, SIDES };
	static constexpr const char* SIDE_NAMES[SIDES] = {"Off", "Waveforms", "XY scope", "Both"};

	OrbitView view;
	Side side = BOTH;
	bool equalXY = false;
	std::vector<Panel::Tap> taps;
	double camAzimuth = -1.05, camElevation = 0.5; // radians, relative to the ring plane

	void paint(Painter& out, const OrbitState& s) const {
		const Theme& theme = view.theme;
		const float u = out.height / 108, font = 6.4f * u;
		bool edges[3] = {};
		for (const Panel::Tap& t : taps)
			edges[t.edge] = true;
		// Readouts take a strip along each edge that has any; the view and the side panel share what's left, from x0.
		const float x0 = edges[Panel::LEFT] ? 33 * u : 0, w = out.width - x0 - (edges[Panel::RIGHT] ? 33 * u : 0),
		            h = out.height - (edges[Panel::BOTTOM] ? 14 * u : 0), viewW = side != OFF ? 0.62f * w : w;

		const std::vector<Trace> traces = s.shape.traces(s.phase);
		const Ring& ring = s.shape.ring;
		Camera cam = Camera::orbit(ring.center, 2.35 * reach(ring.center, traces), camAzimuth, camElevation, viewW, h, Ring::orient(s.pitch, s.roll));
		cam.offset = {x0, 0};
		view(out, cam, s.field, ring, traces, s.velocity);

		// Each panel below occludes whatever of the view spills into it, then draws its divider.
		const Rgba rule = theme.guide * 0.8f;
		if (side != OFF) {
			out.rect(x0 + viewW, 0, w - viewW, h, theme.glass);
			out.line({x0 + viewW, 5 * u}, {x0 + viewW, h - 5 * u}, rule, 0.6f * u);
			const OrbitView::Cycle waves = OrbitView::cycle(s.field, s.shape);
			const float x = x0 + viewW + 5 * u, y = 7 * u, bw = w - viewW - 10 * u, bh = h - 14 * u, wavesH = side == BOTH ? 0.34f * bh : bh;
			if (side != XY)
				view.strip(out, x, y, bw, wavesH, waves, s.phase);
			if (side == BOTH)
				out.line({x, y + wavesH + 3 * u}, {x + bw, y + wavesH + 3 * u}, rule, 0.6f * u);
			if (side != WAVES)
				view.xy(out, x, side == BOTH ? y + wavesH + 6 * u : y, bw, side == BOTH ? bh - wavesH - 6 * u : bh, waves, s.phase, equalXY);
		}
		if (edges[Panel::LEFT]) {
			out.rect(0, 0, x0, h, theme.glass);
			out.line({x0, 5 * u}, {x0, h - 5 * u}, rule, 0.6f * u);
		}
		if (edges[Panel::RIGHT]) {
			out.rect(x0 + w, 0, out.width - x0 - w, h, theme.glass);
			out.line({x0 + w, 5 * u}, {x0 + w, h - 5 * u}, rule, 0.6f * u);
		}
		if (edges[Panel::BOTTOM]) {
			out.rect(0, h, out.width, out.height - h, theme.glass);
			out.line({4 * u, h}, {out.width - 4 * u, h}, rule, 0.6f * u);
		}
		for (const Panel::Tap& t : taps)
			switch (t.edge) {
				case Panel::LEFT: out.label({4 * u, t.at * out.height}, readout(t.param, s), font, t.color, -1); break;
				case Panel::RIGHT: out.label({out.width - 4 * u, t.at * out.height}, readout(t.param, s), font, t.color, 1); break;
				default: out.label({t.at * out.width, 0.5f * (h + out.height)}, readout(t.param, s), font, t.color);
			}

		const Rgba ink = theme.guide * 2.6f;
		out.label({x0 + 5 * u, 7 * u}, hertz(s.hz) + " Hz", 7 * u, ink, -1);
		if (s.channels > 1) {
			char text[24];
			std::snprintf(text, sizeof text, "%d VOICES", s.channels);
			out.label({x0 + 5 * u, 15 * u}, text, 6 * u, ink * 0.7f, -1);
		}
	}
};

} // namespace cxo
