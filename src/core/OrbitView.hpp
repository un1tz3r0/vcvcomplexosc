#pragma once
#include <algorithm>
#include <vector>
#include "Camera.hpp"
#include "Painter.hpp"
#include "Ring.hpp"

namespace cxo {

// A ring and the phases currently being read from it.
struct Trace {
	Ring ring;
	std::vector<double> phases;
};

// Radius of the smallest ball about the first ring's center that holds every trace.
inline double reach(const std::vector<Trace>& traces) {
	double r = 0;
	for (const Trace& t : traces)
		r = std::max(r, length(t.ring.center - traces.front().ring.center) + std::max(t.ring.major, t.ring.minor));
	return r;
}

// The field drawn as a relief over the plane of the first ring, with every ring lifted onto that relief.
// A lifted ring is one cycle of its output wrapped around the ring, and each probe's stem is its present value.
struct OrbitView {
	int rows = 26, cols = 72, ringSegments = 128;
	double extent = 1.5;  // radius of the relief, in units of reach()
	double relief = 0.28; // height of a unit field value, in units of reach()
	Theme theme;

	template <class Field>
	void operator()(Painter& out, const Camera& cam, const Field& field, const std::vector<Trace>& traces) const {
		if (traces.empty())
			return;
		const float px = out.height / 240;
		const Ring& base = traces.front().ring;
		const Vec3 c = base.center, n = base.basis.z;
		const double half = extent * reach(traces), lift = relief * reach(traces);
		const auto lifted = [&](Vec3 p) { return p + n * (lift * field(p)); };
		const auto seg = [&](Vec3 p, Vec3 q, Rgba color, float w) { out.line(cam.project(p), cam.project(q), color, w * px); };

		// The relief is a disc cut into rows along the screen's horizontal, painted far to near, each one filled
		// down to a skirt on the far side of the plane so it hides the rows behind it.
		Vec3 a = cam.right - n * dot(cam.right, n);
		a = length(a) > 1e-3 ? normalize(a) : base.basis.x;
		Vec3 b = cross(n, a);
		if (dot(b, cam.forward) < 0)
			b = -b;
		const Vec3 skirt = n * (dot(cam.eye - c, n) >= 0 ? -1.2 * lift : 1.2 * lift);
		std::vector<Vec2f> ridge, body;
		for (int i = rows - 1; i >= 0; --i) {
			const double v = half * ((2.0 * i + 1) / rows - 1), w = std::sqrt(std::max(0.0, half * half - v * v));
			ridge.clear();
			body.clear();
			for (int j = 0; j <= cols; ++j) {
				const Vec3 p = c + a * (w * (2.0 * j / cols - 1)) + b * v;
				ridge.push_back(cam.project(lifted(p)));
				body.push_back(cam.project(p + skirt));
			}
			body.insert(body.begin(), ridge.rbegin(), ridge.rend());
			out.fill(body, theme.glass);
			out.stroke(ridge, theme.field * float(1 - 0.75 * i / (rows - 1)), 1.1f * px);
		}

		seg(c - base.basis.x * base.major, c + base.basis.x * base.major, theme.guide, 0.8f);
		seg(c - base.basis.y * base.minor, c + base.basis.y * base.minor, theme.guide, 0.8f);
		seg(c, c + n * (1.6 * lift), theme.ring * 0.5f, 0.8f);

		std::vector<Vec2f> flat, wave;
		for (const Trace& t : traces) {
			flat.clear();
			wave.clear();
			for (int k = 0; k < ringSegments; ++k) {
				const Vec3 p = t.ring.at(double(k) / ringSegments);
				flat.push_back(cam.project(p));
				wave.push_back(cam.project(lifted(p)));
			}
			out.stroke(flat, theme.guide, 0.8f * px, true);
			out.stroke(wave, theme.ring, 1.6f * px, true);
			for (double phase : t.phases) {
				const Vec3 p = t.ring.at(phase), q = lifted(p);
				seg(t.ring.center, p, theme.guide, 0.8f);
				seg(p, q, theme.probe * 0.7f, 1.2f);
				out.dot(cam.project(q), 2.4f * px, theme.probe);
			}
		}

		// World axes in the corner, with the ring's normal and major axis among them.
		const float g = 15 * px;
		const Vec2f o{10 * px + g, out.height - 10 * px - g};
		const auto arm = [&](Vec3 d, Rgba color, const char* name) {
			const Vec2f e = cam.projectDir(d);
			out.line(o, {o.x + e.x * g, o.y + e.y * g}, color, px);
			if (name)
				out.label({o.x + e.x * (g + 6 * px), o.y + e.y * (g + 6 * px)}, name, 7 * px, color);
		};
		arm({1, 0, 0}, theme.guide * 2.2f, "X");
		arm({0, 1, 0}, theme.guide * 2.2f, "Y");
		arm({0, 0, 1}, theme.guide * 2.2f, "Z");
		arm(base.basis.x, theme.ring * 0.6f, nullptr);
		arm(n, theme.ring, nullptr);
	}
};

} // namespace cxo
