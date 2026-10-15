#pragma once
#include <algorithm>
#include <cmath>
#include "OpenSimplex2.hpp"

// Cellular (Worley) noise: distances from a point to the feature points of a jittered lattice, one per cell.
namespace cxo {

struct Cells {
	enum Feature { F1, F2, EDGE, SMOOTH, VALUE, FEATURES };
	enum Metric { EUCLIDEAN, MANHATTAN, CHEBYSHEV, METRICS };
	enum Lattice { CUBIC, BCC, LATTICES };
	static constexpr const char* FEATURE_NAMES[FEATURES] = {"F1", "F2", "F2-F1", "SMOOTH F1", "CELL VALUE"};
	static constexpr const char* METRIC_NAMES[METRICS] = {"Euclidean", "Manhattan", "Chebyshev"};
	static constexpr const char* LATTICE_NAMES[LATTICES] = {"Cubic", "Body-centered cubic"};
	static constexpr float MAX_SMOOTHNESS = 0.3f; // past this, points outside the search start to count

	Feature feature = SMOOTH;
	Metric metric = EUCLIDEAN;
	Lattice lattice = CUBIC;
	float jitter = 1;         // 0: feature points at cell centers, 1: anywhere in their cells
	float smoothness = 0.25f; // of smooth F1 and cell value, in cell widths, up to MAX_SMOOTHNESS

	// The feature point of cell (cx, cy, cz) as an offset from its center in [-0.5, 0.5) per unit jitter, and its value
	// in [-1, 1). A BCC lattice adds a second cubic lattice, offset by half a cell and hashed with `flip`.
	static void site(int64_t seed, uint64_t flip, int cx, int cy, int cz, float out[4]) {
		uint64_t h = (uint64_t(seed) ^ flip ^ os2::latticePrime(cx, 0) ^ os2::latticePrime(cy, 1) ^ os2::latticePrime(cz, 2)) * os2::HASH_MUL;
		h ^= h >> 29;
		for (int i = 0; i < 4; ++i)
			out[i] = float((h >> (16 * i)) & 0xFFFF) * (1.f / 65536) - 0.5f;
		out[3] *= 2;
	}

	// Distances are scaled so every metric and lattice spreads F1 over about the same range as Euclidean on cubic.
	float scale() const {
		static constexpr float METRIC_SCALE[METRICS] = {1, 0.68f, 1.23f};
		return METRIC_SCALE[metric] * (lattice == BCC ? 1.26f : 1.f); // BCC has twice the points: 2^(1/3)
	}

	// Cycles per unit up to which the spectrum stays above -60 dB, measured along lines. F1 and F2 have creases, F2-F1
	// sharp ones, and blended cell values approach steps as the smoothness goes to zero.
	double bandwidth() const {
		const double s = std::clamp(smoothness, 0.01f, MAX_SMOOTHNESS);
		static constexpr double CREASED[] = {15, 22, 32};
		const double b = feature < SMOOTH ? CREASED[feature] : feature == SMOOTH ? 11 - 15 * s : std::min(32.0, 1.5 / s + 0.5);
		return b * scale();
	}

	// The feature at `p`, scaled to about [-1, 1]. Searches the 27 cells around p (twice that on a BCC lattice), which holds
	// every feature point within one cell width, so F1 is always exact, and F2 and the smooth features are unless the
	// nearest points are unusually far.
	float operator()(int64_t seed, Vec3 p) const {
		const float scale = this->scale();
		constexpr int N = 54;
		float dist[N], value[N];
		int n = 0;
		const auto search = [&](double offset, uint64_t flip) {
			const int bx = os2::floorInt(p.x - offset), by = os2::floorInt(p.y - offset), bz = os2::floorInt(p.z - offset);
			for (int i = -1; i <= 1; ++i)
				for (int j = -1; j <= 1; ++j)
					for (int k = -1; k <= 1; ++k) {
						const int cx = bx + i, cy = by + j, cz = bz + k;
						float s[4];
						site(seed, flip, cx, cy, cz, s);
						const float dx = float(cx + offset + 0.5 - p.x) + jitter * s[0], dy = float(cy + offset + 0.5 - p.y) + jitter * s[1],
						            dz = float(cz + offset + 0.5 - p.z) + jitter * s[2];
						const float ax = std::abs(dx), ay = std::abs(dy), az = std::abs(dz);
						const float d = metric == MANHATTAN ? ax + ay + az : metric == CHEBYSHEV ? std::max({ax, ay, az}) : std::sqrt(dx * dx + dy * dy + dz * dz);
						dist[n] = scale * d;
						value[n++] = s[3];
					}
		};
		search(0, 0);
		if (lattice == BCC)
			search(0.5, os2::SEED_FLIP_3D);

		float d1 = 1e9f, d2 = 1e9f;
		for (int i = 0; i < n; ++i) {
			d2 = std::min(d2, std::max(d1, dist[i]));
			d1 = std::min(d1, dist[i]);
		}
		switch (feature) {
			case F1: return 2.36f * d1 - 1.215f;
			case F2: return 2.75f * d2 - 1.917f;
			case EDGE: return 0.905f - 2.73f * (d2 - d1);
			default: break;
		}
		// Smooth F1 is a soft minimum over a kernel that reaches `s` past the nearest point. A compact kernel, unlike the
		// usual exponential, ignores points outside the search, so the field never steps as the search moves. Its weight of
		// 1/3 makes the minimum smooth where two points tie. Cell values blend by the same kernel about that minimum.
		const float s = std::clamp(smoothness, 0.01f, MAX_SMOOTHNESS);
		const auto kernel = [s](float d) {
			const float t = std::max(1 - d / s, 0.f);
			return t * t * t;
		};
		float sum = 0;
		for (int i = 0; i < n; ++i)
			sum += kernel(dist[i] - d1);
		const float soft = d1 - s / 3 * std::log(sum);
		if (feature == SMOOTH)
			return 2.36f * soft - 1.215f;
		float weights = 0, blend = 0;
		for (int i = 0; i < n; ++i) {
			const float w = kernel(dist[i] - soft);
			weights += w;
			blend += w * value[i];
		}
		return blend / weights;
	}
};

} // namespace cxo
