#pragma once
#include "OpenSimplex2.hpp"
#include "Vec.hpp"

// A field is any callable taking a Vec3 and returning a float in about [-1, 1]. These are the stock ones.
namespace cxo {

// OpenSimplex2 with optional fractal octaves. Two-dimensional noise ignores z.
struct NoiseField {
	enum class Dim { Two, Three };
	Dim dim = Dim::Three;
	int64_t seed = 0;
	int octaves = 1;
	float lacunarity = 2, gain = 0.5f;

	float operator()(Vec3 p) const {
		float sum = 0, amp = 1, norm = 0;
		for (int i = 0; i < octaves; ++i) {
			sum += amp * (dim == Dim::Two ? os2::noise2(seed + i, p.x, p.y) : os2::noise3(seed + i, p.x, p.y, p.z));
			norm += amp;
			amp *= gain;
			p = p * lacunarity;
		}
		return sum / norm;
	}
};

// The gyroid, a triply periodic minimal surface, with one period every 2 units.
struct GyroidField {
	float operator()(Vec3 p) const {
		p = p * PI;
		return float(std::sin(p.x) * std::cos(p.y) + std::sin(p.y) * std::cos(p.z) + std::sin(p.z) * std::cos(p.x)) / 1.5f;
	}
};

// A linear ramp along `axis`: sampling it on a ring reads out the raw coordinate of the sampled point.
struct AxisField {
	Vec3 axis{1, 0, 0};
	float operator()(Vec3 p) const { return float(dot(axis, p)); }
};

} // namespace cxo
