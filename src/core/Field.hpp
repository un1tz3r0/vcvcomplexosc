#pragma once
#include <algorithm>
#include <cmath>
#include "Cellular.hpp"
#include "OpenSimplex2.hpp"
#include "Vec.hpp"

// A field is any callable taking a Vec3 and returning a float in about [-1, 1].
namespace cxo {

// The gyroid, a triply periodic minimal surface, with one period every 2 units.
inline float gyroid(Vec3 p) {
	p = p * PI;
	return float(std::sin(p.x) * std::cos(p.y) + std::sin(p.y) * std::cos(p.z) + std::sin(p.z) * std::cos(p.x)) / 1.5f;
}

// The built-in fields: a base field summed over fractal octaves, read through an optional distortion of space and
// shaped by an optional waveshaper. Two-dimensional simplex ignores z.
struct StockField {
	enum Kind { SIMPLEX3, SIMPLEX2, GYROID, CELLULAR, KINDS };
	static constexpr const char* NAMES[KINDS] = {"SIMPLEX3", "SIMPLEX2", "GYROID", "CELLULAR"};
	// How octaves combine. fBM sums them. Ridged folds each about zero and lets each ridge sharpen the next octave.
	// Hetero terrain and hybrid multifractal scale each octave by the value so far, so valleys stay smooth and peaks get rough.
	enum Fractal { FBM, RIDGED, HETERO, HYBRID, FRACTALS };
	static constexpr const char* FRACTAL_NAMES[FRACTALS] = {"FBM", "RIDGED", "HETERO", "HYBRID"};
	// Distortions of space before the field is read, by `distortion` from 0 (none) to 1.
	enum Distort { NONE, TWIST, SWIRL, FOLD, RIPPLE, DISTORTS };
	static constexpr const char* DISTORT_NAMES[DISTORTS] = {"NONE", "TWIST", "SWIRL", "FOLD", "RIPPLE"};
	// Shapes for the value read, driven by `drive` from 1 (gentle) up.
	enum Shaper { OFF, WAVEFOLD, SINE, CLIP, RECTIFY, SHAPERS };
	static constexpr const char* SHAPER_NAMES[SHAPERS] = {"OFF", "FOLD", "SINE", "CLIP", "RECTIFY"};
	static constexpr int MAX_OCTAVES = 8;

	Kind kind = SIMPLEX3;
	int64_t seed = 0;
	Fractal fractal = FBM;
	float detail = 0;        // octaves above the first, from 0 to MAX_OCTAVES - 1; a fraction fades the top one in
	float roughness = 0.5f;  // each octave's amplitude relative to the one below
	float lacunarity = 2;    // each octave's frequency relative to the one below
	Cells cells;
	// A second, independently seeded noise: its first three components displace space by `warp`, and its fourth scales
	// the roughness by up to 4 times either way at `tilt` 1, for patches of smoother and grittier texture. It is read at
	// `warpScale` times the frequency and slid `warpDrift` along its own Z, which evolves it apart from the main field.
	float warp = 0, tilt = 0, warpScale = 1, warpDrift = 0;
	Distort distort = NONE;
	float distortion = 0;
	Shaper shaper = OFF;
	float drive = 1;
	// Octaves above the first fade out as their detail nears `limit` cycles per unit of path, and are skipped past it,
	// so a path read too fast for them doesn't alias.
	double limit = INFINITY;

	int octaves() const { return std::clamp(int(std::ceil(detail)), 0, MAX_OCTAVES - 1) + 1; }

	// Cycles per unit of path up to which the field's spectrum stays above about -60 dB, measured along lines. Distortion,
	// warp and shaping all add detail, roughly in proportion to how hard they work.
	double bandwidth() const {
		double d = kind == CELLULAR ? cells.bandwidth() : kind == GYROID ? 1.4 : 4.4;
		d *= (1 + warp * warpScale) * (1 + 2 * distortion * (distort != NONE));
		static constexpr double SHAPING[SHAPERS] = {0, 0.25, 0.7, 0.3, 1.5}; // measured at drive 2
		return d * (1 + SHAPING[shaper] * drive);
	}

	StockField limited(double cyclesPerUnit) const {
		StockField f = *this;
		f.limit = cyclesPerUnit;
		return f;
	}

	float octave(int i, Vec3 p) const {
		switch (kind) {
			case SIMPLEX2: return os2::noise2(seed + i, p.x, p.y);
			case GYROID: return gyroid(p + Vec3{1.7, 2.3, 0.9} * i); // offset so octaves don't stack in phase
			case CELLULAR: return cells(seed + i, p);
			default: return os2::noise3(seed + i, p.x, p.y, p.z);
		}
	}

	Vec3 distorted(Vec3 p) const {
		const double a = distortion;
		switch (distort) {
			case TWIST: return Mat3::rotZ(a * PI * p.z) * p; // turns each slice about Z in proportion to its height
			case SWIRL: return Mat3::rotZ(a * TAU * std::exp(-(p.x * p.x + p.y * p.y))) * p; // a vortex about the Z axis
			case FOLD: {
				// Mirrors space into unit cells, mixed in by the amount: kaleidoscopic repeats.
				const auto tri = [](double x) { return std::abs(x - 2 * std::floor(0.5 * x + 0.5)); };
				return p * (1 - a) + Vec3{tri(p.x), tri(p.y), tri(p.z)} * a;
			}
			case RIPPLE: return p + Vec3{std::sin(TAU * p.y), std::sin(TAU * p.z), std::sin(TAU * p.x)} * (0.25 * a);
			default: return p;
		}
	}

	float shaped(float v) const {
		const float g = std::max(drive, 1.f);
		switch (shaper) {
			case WAVEFOLD: return float(std::sin(0.5 * PI * g * v)); // folds back over itself once v·drive passes 1
			case SINE: return float(std::sin(PI * g * v));
			case CLIP: return std::tanh(g * v) / std::tanh(g);
			case RECTIFY: {
				// Full-wave rectified with rounded corners, which keeps its spectrum finite.
				const float x = g * v;
				return 2 * std::tanh(1.5f * (std::sqrt(x * x + 0.04f) - 0.2f)) - 1;
			}
			default: return v;
		}
	}

	float operator()(Vec3 p) const {
		float rough = roughness;
		if (warp > 0 || tilt > 0) {
			const Vec3 q = p * warpScale + Vec3{0, 0, warpDrift};
			const auto read = [&](int k) { return os2::noise3(seed + 1001 + k, q.x, q.y, q.z); };
			if (tilt > 0)
				rough = std::min(1.f, rough * std::exp2(2 * tilt * read(3)));
			if (warp > 0)
				p = p + Vec3{read(0), read(1), read(2)} * warp;
		}
		p = distorted(p);
		const int n = octaves();
		double reach = limit / bandwidth(); // in octave scale, where octave i is lacunarity^i
		// Alongside the sum, each type tracks what it needs to bring its typical range to about [-1, 1] whatever the detail
		// and roughness: fBM its RMS amplitude, ridged its total amplitude, and the multifractals their expected value.
		float amp = 1, value = 0, weight = 1, norm = 0, power = 0, expected = 0, expectedWeight = 1;
		for (int i = 0; i < n; ++i, amp *= rough, reach /= lacunarity, p = p * lacunarity) {
			// The top octave fades in with the fraction of detail, and any octave fades out as it nears the limit.
			const float fade = i == n - 1 && i > 0 ? detail - (n - 2) : 1.f;
			const float w = i > 0 ? fade * float(std::clamp(2 - 2 / reach, 0.0, 1.0)) : 1.f;
			norm += fade * amp;
			power += fade * fade * amp * amp;
			if (fractal == HETERO)
				expected = i == 0 ? 1 : expected * (1 + w * amp);
			else if (fractal == HYBRID) {
				const float signal = HYBRID_OFFSET * amp, e = i == 0 ? 1 : std::min(expectedWeight, 1.f);
				expected += w * e * signal;
				expectedWeight = e * signal;
			}
			if (w <= 0)
				continue;
			const float o = octave(i, p);
			switch (fractal) {
				case RIDGED: {
					const float r = 1 - std::abs(o), signal = r * r * weight;
					weight = std::clamp(2 * signal, 0.f, 1.f);
					value += w * amp * signal;
					break;
				}
				case HETERO: value += i == 0 ? o + 1 : w * amp * (o + 1) * value; break;
				case HYBRID: {
					const float signal = amp * (o + HYBRID_OFFSET), e = i == 0 ? 1 : std::min(weight, 1.f);
					value += w * e * signal;
					weight = e * signal;
					break;
				}
				default: value += w * amp * o;
			}
		}
		// The constants are fitted to simplex noise over roughness 0.25 to 1 and detail 0 to 7.
		switch (fractal) {
			case RIDGED: {
				// Each ridge thins the next, so the more of the amplitude is in upper octaves, the less of its range it fills.
				const float upper = 1 - 1 / norm;
				value = 2 * (1 + 0.4f * upper * upper) * value / norm - 0.95f;
				break;
			}
			case HETERO: {
				// Valleys bottom out at -1 while peaks grow with each octave, so only the peaks are compressed.
				const float u = value / expected - 1;
				value = u / (1 + 0.08f * std::log2(expected) * std::max(u, 0.f));
				break;
			}
			case HYBRID: value = (value - expected) / std::sqrt(power); break;
			default: value /= std::sqrt(power);
		}
		return shaper == OFF ? value : shaped(value);
	}

	static constexpr float HYBRID_OFFSET = 0.7f;
};

} // namespace cxo
