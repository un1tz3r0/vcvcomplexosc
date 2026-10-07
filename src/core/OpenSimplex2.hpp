#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include "Vec.hpp"

// OpenSimplex2 (the fast variant) in 2D and 3D, after K.jpg's public-domain reference implementation.
// Output lies roughly within [-1, 1]. noise3 uses the "ImproveXY" orientation: XY slices are isotropic,
// which leaves Z as the natural axis for evolving a planar slice over time.
namespace cxo::os2 {

using u64 = uint64_t;

constexpr u64 PRIME[3] = {0x5205402B9270C86Full, 0x598CD327003817B5ull, 0x5BCC226E9FA0BACBull};
constexpr u64 HASH_MUL = 0x53A3F72DEEC546F5ull;
constexpr u64 SEED_FLIP_3D = u64(-0x52D547B2E96ED629ll);
constexpr float RSQ2 = 0.5f, RSQ3 = 0.6f;
constexpr double NORM2 = 0.01001634121365712, NORM3 = 0.07969837668935331;

inline int floorInt(double x) {
	const int i = int(x);
	return x < i ? i - 1 : i;
}
inline int roundInt(double x) { return x < 0 ? int(x - 0.5) : int(x + 0.5); }
inline float pow4(float a) {
	a *= a;
	return a * a;
}
inline u64 latticePrime(int i, int axis) { return u64(int64_t(i)) * PRIME[axis]; }

// 24 directions at 7.5° + k·15°, cycled through 128 slots indexed by 7 hash bits.
inline const std::array<float, 256> GRAD2 = [] {
	std::array<float, 256> g{};
	for (int i = 0; i < 128; ++i) {
		const double a = (7.5 + 15.0 * (i % 24)) * PI / 180;
		g[2 * i] = float(std::cos(a) / NORM2);
		g[2 * i + 1] = float(std::sin(a) / NORM2);
	}
	return g;
}();

// 48 equal-length directions, (±s, ±s, ±1) and (±p, ±q, 0) in every axis arrangement,
// cycled through 256 slots indexed by 8 hash bits.
inline const std::array<float, 1024> GRAD3 = [] {
	const double s = 1 + std::sqrt(1.5), p = 3.0862664687972017, q = 1.1721513422464978;
	std::array<std::array<double, 3>, 48> dirs{};
	int n = 0;
	for (int axis = 0; axis < 3; ++axis) {
		const int i = (axis + 1) % 3, j = (axis + 2) % 3;
		for (int signs = 0; signs < 8; ++signs) {
			const double si = signs & 1 ? -1 : 1, sj = signs & 2 ? -1 : 1, sk = signs & 4 ? -1 : 1;
			auto& a = dirs[n++];
			a[i] = si * s, a[j] = sj * s, a[axis] = sk;
			auto& b = dirs[n++];
			b[i] = si * (sk > 0 ? p : q), b[j] = sj * (sk > 0 ? q : p), b[axis] = 0;
		}
	}
	std::array<float, 1024> g{};
	for (int i = 0; i < 256; ++i)
		for (int k = 0; k < 3; ++k)
			g[4 * i + k] = float(dirs[i % 48][k] / NORM3);
	return g;
}();

inline float grad2(u64 seed, u64 xp, u64 yp, float dx, float dy) {
	u64 h = (seed ^ xp ^ yp) * HASH_MUL;
	h ^= h >> 58;
	const unsigned gi = unsigned(h) & 0xFE;
	return GRAD2[gi] * dx + GRAD2[gi + 1] * dy;
}

inline float grad3(u64 seed, const u64 hp[3], const float d[3]) {
	u64 h = (seed ^ hp[0] ^ hp[1] ^ hp[2]) * HASH_MUL;
	h ^= h >> 58;
	const unsigned gi = unsigned(h) & 0x3FC;
	return GRAD3[gi] * d[0] + GRAD3[gi + 1] * d[1] + GRAD3[gi + 2] * d[2];
}

// Sums the three vertices of the triangle containing (x, y) on the A2 lattice.
inline float noise2(int64_t seed, double x, double y) {
	constexpr double SKEW = 0.366025403784439, UNSKEW = -0.21132486540518713;
	constexpr float U = float(UNSKEW), D = float(1 + 2 * UNSKEW);
	const double s = SKEW * (x + y), xs = x + s, ys = y + s;
	const int xb = floorInt(xs), yb = floorInt(ys);
	const float xi = float(xs - xb), yi = float(ys - yb);
	const u64 xp = latticePrime(xb, 0), yp = latticePrime(yb, 1);
	const float t = (xi + yi) * U, dx = xi + t, dy = yi + t;

	const auto vertex = [seed](u64 hx, u64 hy, float vx, float vy) {
		const float a = RSQ2 - vx * vx - vy * vy;
		return a > 0 ? pow4(a) * grad2(u64(seed), hx, hy, vx, vy) : 0.f;
	};
	return vertex(xp, yp, dx, dy)
		+ vertex(xp + PRIME[0], yp + PRIME[1], dx - D, dy - D)
		+ (dy > dx ? vertex(xp, yp + PRIME[1], dx - U, dy - U - 1)
		           : vertex(xp + PRIME[0], yp, dx - U - 1, dy - U));
}

// Sums the nearest and second-nearest points of two interleaved cubic lattices (together a BCC lattice).
inline float noise3(int64_t seed, double x, double y, double z) {
	constexpr double R3 = 0.577350269189626, ORTH = -0.21132486540518713;
	const double xy = x + y, s2 = xy * ORTH, zz = z * R3;
	const double r[3] = {x + s2 + zz, y + s2 + zz, xy * -R3 + zz};

	float d[3];
	int sgn[3];
	u64 hp[3];
	for (int i = 0; i < 3; ++i) {
		const int b = roundInt(r[i]);
		d[i] = float(r[i] - b);
		sgn[i] = d[i] >= 0 ? 1 : -1;
		hp[i] = latticePrime(b, i);
	}

	u64 sd = u64(seed);
	float value = 0;
	for (int copy = 0; copy < 2; ++copy) {
		const float a = RSQ3 - (d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
		if (a > 0)
			value += pow4(a) * grad3(sd, hp, d);

		// The second-nearest point is one step along the axis where the offset is largest.
		int m = 0;
		for (int i = 1; i < 3; ++i)
			if (std::abs(d[i]) > std::abs(d[m]))
				m = i;
		const float b = a + 2 * std::abs(d[m]) - 1;
		if (b > 0) {
			u64 hq[3] = {hp[0], hp[1], hp[2]};
			float dq[3] = {d[0], d[1], d[2]};
			hq[m] += u64(int64_t(sgn[m])) * PRIME[m];
			dq[m] -= float(sgn[m]);
			value += pow4(b) * grad3(sd, hq, dq);
		}

		// Shift to the other lattice, offset by half a cell toward the sample.
		for (int i = 0; i < 3; ++i) {
			if (sgn[i] > 0)
				hp[i] += PRIME[i];
			d[i] -= 0.5f * float(sgn[i]);
			sgn[i] = -sgn[i];
		}
		sd ^= SEED_FLIP_3D;
	}
	return value;
}

} // namespace cxo::os2
