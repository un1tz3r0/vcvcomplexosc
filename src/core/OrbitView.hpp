#pragma once
#include <algorithm>
#include <array>
#include <vector>
#include "Camera.hpp"
#include "Orbit.hpp"
#include "Painter.hpp"

namespace cxo {

// The field drawn as a relief over the plane of a base ring, with every traced ring lifted off its own plane by what it reads.
// A lifted ring is one cycle of its output wrapped around the ring, and each probe's stem is its present value.
struct OrbitView {
	// How the relief's floor is drawn, and the context menu's names for its settings.
	enum Floor { ROWS, GRID, DOTS, FLOORS };
	static constexpr const char* FLOOR_NAMES[FLOORS] = {"Rows", "Grid", "Dots"};
	static constexpr const char* DENSITY_NAMES[] = {"Coarse", "Normal", "Fine"};
	static constexpr const char* BRIGHTNESS_NAMES[] = {"Dim", "Normal", "Bright", "Brightest"};
	static constexpr const char* HUE_NAMES[] = {"Off", "Subtle", "Medium", "Strong"};
	static constexpr int DENSITIES = std::size(DENSITY_NAMES), BRIGHTNESSES = std::size(BRIGHTNESS_NAMES), HUES = std::size(HUE_NAMES);

	Floor floor = GRID;
	int density = 1, brightness = 2, hue = 1; // indices into the names above
	int ringSegments = 128;
	double extent = 1.4;  // radius of the relief, in units of reach()
	double relief = 0.28; // height of a unit field value, in units of reach()
	double trail = 0.08;  // length of each probe's fading tail, in cycles
	Theme theme;

	// `velocity` is the center's, in units per second, shown as arrows on the corner axes.
	template <class Field>
	void operator()(Painter& out, const Camera& cam, const Field& field, const Ring& base, const std::vector<Trace>& traces, Vec3 velocity = {}) const {
		const float px = cam.height / 130;
		const Vec3 c = base.center, n = base.basis.z;
		const double r = std::max(reach(c, traces), 1e-3), half = extent * r, lift = relief * r;
		const auto liftedOff = [&](Vec3 p, Vec3 normal) { return p + normal * (lift * field(p)); };
		const auto seg = [&](Vec3 p, Vec3 q, Rgba color, float w) { out.line(cam.project(p), cam.project(q), color, w * px); };

		paintFloor(out, cam, field, c, n, half, lift);

		seg(c - base.basis.x * base.major, c + base.basis.x * base.major, theme.guide, 0.8f);
		seg(c - base.basis.y * base.minor, c + base.basis.y * base.minor, theme.guide, 0.8f);
		seg(c, c + n * (1.6 * lift), theme.ring * 0.5f, 0.8f);

		std::vector<Vec2f> flat, wave;
		for (const Trace& t : traces) {
			const auto lifted = [&](Vec3 p) { return liftedOff(p, t.ring.basis.z); };
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
				constexpr int TAIL = 12;
				for (int k = 0; k < TAIL; ++k) {
					const double p0 = phase - trail * (k + 1) / TAIL, p1 = phase - trail * k / TAIL;
					out.line(cam.project(lifted(t.ring.at(p0))), cam.project(lifted(t.ring.at(p1))), theme.probe * (0.6f * (1 - float(k) / TAIL)), 2.f * px);
				}
				seg(t.ring.center, p, theme.guide, 0.8f);
				seg(p, q, theme.probe * 0.7f, 1.2f);
				out.dot(cam.project(q), 2.4f * px, theme.probe);
			}
		}

		// World axes in the corner, with the ring's normal and major axis among them.
		const float g = 13 * px;
		const Vec2f o{8 * px + g, cam.height - 6 * px - g};
		const auto arm = [&](Vec3 d, Rgba color, const char* name) {
			const Vec2f e = cam.projectDir(d);
			out.line(o, {o.x + e.x * g, o.y + e.y * g}, color, px);
			if (name)
				out.label({o.x + e.x * (g + 5 * px), o.y + e.y * (g + 5 * px)}, name, 7 * px, color);
		};
		arm({1, 0, 0}, theme.guide * 2.2f, "X");
		arm({0, 1, 0}, theme.guide * 2.2f, "Y");
		arm({0, 0, 1}, theme.guide * 2.2f, "Z");
		arm(base.basis.x, theme.ring * 0.6f, nullptr);
		arm(n, theme.ring, nullptr);

		// The center's velocity along each world axis as an arrow over that axis, squashed so drift at full speed
		// (1 unit/s) reaches a little past the axis.
		const Vec3 axes[] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
		const double speeds[] = {velocity.x, velocity.y, velocity.z};
		for (int k = 0; k < 3; ++k) {
			const float len = float(1.6 * std::tanh(std::abs(speeds[k])));
			if (len < 0.03f)
				continue;
			const Vec2f e = cam.projectDir(speeds[k] < 0 ? -axes[k] : axes[k]), tip{o.x + e.x * g * len, o.y + e.y * g * len};
			out.line(o, tip, theme.probe * 0.9f, 1.4f * px);
			const float el = std::hypot(e.x, e.y);
			if (el < 0.15f)
				continue;
			const Vec2f d{e.x / el, e.y / el}, side{-d.y, d.x};
			const float head = 3 * px;
			out.stroke({{tip.x - d.x * head + side.x * head * 0.6f, tip.y - d.y * head + side.y * head * 0.6f}, tip,
			            {tip.x - d.x * head - side.x * head * 0.6f, tip.y - d.y * head - side.y * head * 0.6f}},
			           theme.probe * 0.9f, 1.2f * px);
		}
	}

	// The relief: a disc of square cells in the plane through `c` normal to `n`, lifted by the field, cut into rows
	// along the screen's horizontal and painted far to near. Each row is filled down to a skirt on the far side of the
	// plane so it hides what it stands in front of, the grid's columns between it and the row behind included.
	// With a hue shift, heights are binned into bands so each band's lines go out in one batch.
	template <class Field>
	void paintFloor(Painter& out, const Camera& cam, const Field& field, Vec3 c, Vec3 n, double half, double lift) const {
		constexpr float SPACING[DENSITIES] = {7.5f, 5, 3.5f}, BRIGHT[BRIGHTNESSES] = {0.6f, 0.85f, 1.15f, 1.5f};
		constexpr float HUE_SPAN[HUES] = {0, 0.35f, 0.8f, 1.6f}; // radians at either extreme of the floor's range
		constexpr int BANDS[HUES] = {1, 4, 6, 8}, MAX_BANDS = 8;     // fewer for a subtler shift, since each costs a draw per row
		constexpr int MIN_ROWS[DENSITIES] = {8, 12, 16}, MAX_ROWS[DENSITIES] = {20, 32, 44}, SUB = 3;
		const float px = cam.height / 130;
		const int rows = std::clamp(int(cam.height / SPACING[density]), MIN_ROWS[density], MAX_ROWS[density]), cols = rows * SUB;
		const int bands = BANDS[hue];

		Vec3 a = cam.right - n * dot(cam.right, n);
		a = length(a) > 1e-3 ? normalize(a) : cross(n, std::abs(n.z) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0});
		Vec3 b = cross(n, a);
		if (dot(b, cam.forward) < 0)
			b = -b;
		const Vec3 skirt = n * (dot(cam.eye - c, n) >= 0 ? -1.2 * lift : 1.2 * lift);

		// Each row's samples run across its chord: the chord's ends, and between them the lattice points u = -half + j·step.
		// Grid columns and dots sit on every SUB-th lattice point, so the cells come out square.
		struct Row {
			std::vector<Vec2f> ridge, base;
			std::vector<float> value;
			std::vector<int> lattice; // each sample's j, or -1 for a chord end
		};
		std::vector<Row> grid(rows);
		const double step = 2 * half / cols;
		float lo = 1e9f, hi = -1e9f;
		for (int i = 0; i < rows; ++i) {
			Row& row = grid[i];
			const double v = half * ((2.0 * i + 1) / rows - 1), w = std::sqrt(std::max(0.0, half * half - v * v));
			const auto sample = [&](double u, int j) {
				const Vec3 p = c + a * u + b * v;
				const float f = field(p);
				row.ridge.push_back(cam.project(p + n * (lift * f)));
				row.base.push_back(cam.project(p + skirt));
				row.value.push_back(f);
				row.lattice.push_back(j);
				lo = std::min(lo, f), hi = std::max(hi, f);
			};
			sample(-w, -1);
			for (int j = int(std::floor((half - w) / step)) + 1; -half + j * step < w; ++j)
				sample(-half + j * step, j);
			sample(w, -1);
		}

		Rgba palette[MAX_BANDS];
		for (int k = 0; k < bands; ++k)
			palette[k] = bands > 1 ? hueTurned(theme.field, HUE_SPAN[hue] * (2.f * k / (bands - 1) - 1)) : theme.field;
		const float mid = 0.5f * (lo + hi), range = std::max(0.5f * (hi - lo), 1e-4f);
		const auto band = [&](float f) { return bands > 1 ? std::clamp(int(std::lround((f - mid) / range * 0.5f * (bands - 1) + 0.5f * (bands - 1))), 0, bands - 1) : 0; };
		const auto fade = [&](double i) { return BRIGHT[brightness] * float(1 - 0.55 * i / (rows - 1)); };

		std::vector<Vec2f> batch[MAX_BANDS], body;
		const auto flush = [&](float alpha, float width, bool asDots) {
			for (int k = 0; k < bands; ++k) {
				if (asDots)
					out.dots(batch[k], width, palette[k] * alpha);
				else
					out.segments(batch[k], palette[k] * alpha, width);
				batch[k].clear();
			}
		};
		const Row* behind = nullptr;
		for (int i = rows - 1; i >= 0; --i) {
			const Row& row = grid[i];
			if (floor == GRID && behind) {
				for (size_t s = 0, t = 0; s < row.lattice.size(); ++s) {
					const int j = row.lattice[s];
					if (j < 0 || j % SUB)
						continue;
					while (t < behind->lattice.size() && (behind->lattice[t] < 0 ? -1 : behind->lattice[t]) < j)
						++t;
					if (t < behind->lattice.size() && behind->lattice[t] == j) {
						std::vector<Vec2f>& into = batch[band(0.5f * (row.value[s] + behind->value[t]))];
						into.push_back(behind->ridge[t]);
						into.push_back(row.ridge[s]);
					}
				}
				flush(fade(i + 0.5), 0.9f * px, false);
			}

			body.assign(row.ridge.rbegin(), row.ridge.rend());
			body.insert(body.end(), row.base.begin(), row.base.end());
			out.fill(body, theme.glass);

			if (floor == DOTS) {
				for (size_t s = 0; s < row.lattice.size(); ++s)
					if (row.lattice[s] >= 0 && row.lattice[s] % SUB == 0)
						batch[band(row.value[s])].push_back(row.ridge[s]);
				flush(fade(i), 1.0f * px, true);
			} else if (bands == 1)
				out.stroke(row.ridge, theme.field * fade(i), 1.1f * px);
			else {
				for (size_t s = 0; s + 1 < row.ridge.size(); ++s) {
					std::vector<Vec2f>& into = batch[band(0.5f * (row.value[s] + row.value[s + 1]))];
					into.push_back(row.ridge[s]);
					into.push_back(row.ridge[s + 1]);
				}
				flush(fade(i), 1.1f * px, false);
			}
			behind = &row;
		}
	}

	// One cycle of both outputs as read straight from the field, N points each with the last back at phase 0.
	using Cycle = std::array<std::vector<float>, OrbitShape::OUTPUTS>;
	template <class Field>
	static Cycle cycle(const Field& field, const OrbitShape& shape, int n = 160) {
		Cycle waves;
		for (int k = 0; k < OrbitShape::OUTPUTS; ++k) {
			const Ring ring = shape.ringFor(k);
			for (int i = 0; i < n; ++i)
				waves[k].push_back(field(ring.at(shape.phaseFor(k, double(i) / (n - 1)))));
		}
		return waves;
	}

	// Both outputs unrolled left to right in the box (x, y, w, h), fitted to its height, with a cursor at the present phase.
	void strip(Painter& out, float x, float y, float w, float h, const Cycle& waves, double phase) const {
		const float px = out.height / 130;
		const int n = int(waves[0].size());
		float lo = 1e9f, hi = -1e9f;
		for (const auto& wave : waves)
			for (float v : wave)
				lo = std::min(lo, v), hi = std::max(hi, v);
		const float mid = 0.5f * (lo + hi), scale = 0.42f * h / std::max(0.5f * (hi - lo), 1e-3f);
		out.rect(x, y, w, h, theme.glass);
		std::vector<Vec2f> pts(n);
		for (int k = OrbitShape::OUTPUTS - 1; k >= 0; --k) {
			for (int i = 0; i < n; ++i)
				pts[i] = {x + w * i / (n - 1), y + 0.5f * h - (waves[k][i] - mid) * scale};
			out.stroke(pts, k == 0 ? theme.ring : theme.field, (k == 0 ? 1.3f : 0.9f) * px);
		}
		const float cx = x + w * float(phase - std::floor(phase));
		out.line({cx, y + 2 * px}, {cx, y + h - 2 * px}, theme.probe * 0.5f, 0.8f * px);
	}

	// A against B over one cycle in the box (x, y, w, h), each fitted to its axis or, if `equal`, both at one scale,
	// with the present phase as a dot trailing a fading tail.
	void xy(Painter& out, float x, float y, float w, float h, const Cycle& waves, double phase, bool equal) const {
		const float px = out.height / 130;
		const int n = int(waves[0].size());
		float mid[2], half[2];
		for (int k = 0; k < 2; ++k) {
			const auto [lo, hi] = std::minmax_element(waves[k].begin(), waves[k].end());
			mid[k] = 0.5f * (*lo + *hi);
			half[k] = std::max(0.5f * (*hi - *lo), 1e-3f);
		}
		float sx = 0.44f * w / half[0], sy = 0.44f * h / half[1];
		if (equal)
			sx = sy = std::min(sx, sy);
		const float cx = x + 0.5f * w, cy = y + 0.5f * h;
		const auto at = [&](double i) {
			const int j = std::min(int(i), n - 2);
			const float f = float(i - j);
			const float a = waves[0][j] + (waves[0][j + 1] - waves[0][j]) * f, b = waves[1][j] + (waves[1][j + 1] - waves[1][j]) * f;
			return Vec2f{cx + (a - mid[0]) * sx, cy - (b - mid[1]) * sy};
		};

		out.rect(x, y, w, h, theme.glass);
		const Rgba rule = theme.guide * 0.8f;
		out.line({x + 2 * px, cy}, {x + w - 2 * px, cy}, rule, 0.6f * px);
		out.line({cx, y + 2 * px}, {cx, y + h - 2 * px}, rule, 0.6f * px);
		out.label({x + w - 3 * px, cy - 3 * px}, "A", 6 * px, theme.guide * 2.2f, 1);
		out.label({cx + 3 * px, y + 6 * px}, "B", 6 * px, theme.guide * 2.2f, -1);

		std::vector<Vec2f> pts(n);
		for (int i = 0; i < n; ++i)
			pts[i] = at(i);
		out.stroke(pts, theme.ring, 1.2f * px, true);
		const double head = (phase - std::floor(phase)) * (n - 1);
		constexpr int TAIL = 12;
		for (int k = 0; k < TAIL; ++k) {
			const auto back = [&](int steps) { return std::fmod(head - trail * (n - 1) * steps / TAIL + (n - 1), double(n - 1)); };
			out.line(at(back(k + 1)), at(back(k)), theme.probe * (0.6f * (1 - float(k) / TAIL)), 2.f * px);
		}
		out.dot(at(head), 2.4f * px, theme.probe);
	}
};

} // namespace cxo
