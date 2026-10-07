#pragma once
#include <cstdio>
#include "Field.hpp"
#include "OrbitView.hpp"

namespace cxo {

// Everything the Orbit display needs to know about the oscillator at one moment.
struct OrbitState {
	OrbitShape shape{Ring{{}, {}, 1.2, 0.8}};
	double tilt = 0, azimuth = 0; // radians: the ring plane's orientation, which the camera rides with
	StockField field;
	double hz = 0, phase = 0;
	int channels = 1;
};

// The Orbit display's composition: 3D view on top, unrolled waveforms below, readouts in the corners.
// Shared by the module's widget and tools/render so previews are the real thing.
struct OrbitScene {
	OrbitView view;
	bool strip = true;
	double camAzimuth = -1.05, camElevation = 0.5; // radians, relative to the ring plane

	void paint(Painter& out, const OrbitState& s) const {
		const float stripH = strip ? 0.24f * out.height : 0, u = out.height / 130;
		const std::vector<Trace> traces = s.shape.traces(s.phase);
		const Camera cam = Camera::orbit(s.shape.ring.center, 2.35 * reach(traces), camAzimuth, camElevation, out.width, out.height - stripH,
		                                 Ring::orient(0, s.tilt, s.azimuth));
		view(out, cam, s.field, traces);
		if (strip)
			view.strip(out, 0, out.height - stripH, out.width, stripH, s.field, s.shape, s.phase);

		char text[48];
		const Rgba ink = view.theme.guide * 2.6f;
		std::snprintf(text, sizeof text, s.hz < 10 ? "%.3f Hz" : s.hz < 1000 ? "%.1f Hz" : "%.2f kHz", s.hz < 1000 ? s.hz : s.hz / 1000);
		out.label({5 * u, 6 * u}, text, 7 * u, ink, -1);
		if (s.channels > 1) {
			std::snprintf(text, sizeof text, "%d VOICES", s.channels);
			out.label({5 * u, 14 * u}, text, 6 * u, ink * 0.7f, -1);
		}
		std::snprintf(text, sizeof text, "%s x%d", StockField::NAMES[s.field.kind], s.field.octaves);
		out.label({out.width - 5 * u, 6 * u}, text, 7 * u, ink, 1);
		out.label({out.width - 5 * u, 14 * u}, s.shape.pinch ? "PINCH" : "PHASE", 6 * u, ink * 0.7f, 1);
	}
};

} // namespace cxo
