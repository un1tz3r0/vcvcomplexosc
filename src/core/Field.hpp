#pragma once
#include "OpenSimplex2.hpp"
#include "Vec.hpp"

// A field is any callable taking a Vec3 and returning a float in about [-1, 1].
namespace cxo {

// The gyroid, a triply periodic minimal surface, with one period every 2 units.
inline float gyroid(Vec3 p) {
	p = p * PI;
	return float(std::sin(p.x) * std::cos(p.y) + std::sin(p.y) * std::cos(p.z) + std::sin(p.z) * std::cos(p.x)) / 1.5f;
}

// The built-in fields, summed over fractal octaves. Two-dimensional simplex ignores z.
struct StockField {
	enum Kind { SIMPLEX3, SIMPLEX2, GYROID, KINDS };
	static constexpr const char* NAMES[KINDS] = {"SIMPLEX3", "SIMPLEX2", "GYROID"};

	Kind kind = SIMPLEX3;
	int64_t seed = 0;
	int octaves = 1;
	float lacunarity = 2, gain = 0.5f;

	float octave(int i, Vec3 p) const {
		switch (kind) {
			case SIMPLEX2: return os2::noise2(seed + i, p.x, p.y);
			case GYROID: return gyroid(p + Vec3{1.7, 2.3, 0.9} * i); // offset so octaves don't stack in phase
			default: return os2::noise3(seed + i, p.x, p.y, p.z);
		}
	}

	float operator()(Vec3 p) const {
		float sum = 0, amp = 1, norm = 0;
		for (int i = 0; i < octaves; ++i) {
			sum += amp * octave(i, p);
			norm += amp;
			amp *= gain;
			p = p * lacunarity;
		}
		return sum / norm;
	}
};

} // namespace cxo
