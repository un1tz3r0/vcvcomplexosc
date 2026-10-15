// Percentiles of each field setting's output over random points, to check that every setting spans about [-1, 1].
// Usage: calibrate [fractal|cells|exact]
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
#include "core/Field.hpp"
using namespace cxo;

template <class F>
static void report(const char* name, F f, int count = 100000) {
	std::mt19937 rng(3);
	std::uniform_real_distribution<double> u(-40, 40);
	std::vector<float> v(count);
	for (float& x : v)
		x = f(Vec3{u(rng), u(rng), u(rng)});
	std::sort(v.begin(), v.end());
	std::printf("%-34s p0.1 %+6.2f  p1 %+6.2f  p50 %+6.2f  p99 %+6.2f  p99.9 %+6.2f\n", name, v[count / 1000], v[count / 100], v[count / 2],
	            v[count - count / 100], v[count - count / 1000]);
}

// Brute-force nearest distances over the 125 cells around p, for checking the 27-cell search.
static void exact(const Cells& c, int64_t seed, Vec3 p, float& d1, float& d2, float& outside) {
	d1 = d2 = outside = 1e9f;
	for (int lat = 0; lat <= int(c.lattice == Cells::BCC); ++lat) {
		const double off = 0.5 * lat;
		const int bx = os2::floorInt(p.x - off), by = os2::floorInt(p.y - off), bz = os2::floorInt(p.z - off);
		for (int i = -2; i <= 2; ++i)
			for (int j = -2; j <= 2; ++j)
				for (int k = -2; k <= 2; ++k) {
					float s[4];
					Cells::site(seed, lat ? os2::SEED_FLIP_3D : 0, bx + i, by + j, bz + k, s);
					const double dx = bx + i + off + 0.5 + c.jitter * s[0] - p.x, dy = by + j + off + 0.5 + c.jitter * s[1] - p.y,
					             dz = bz + k + off + 0.5 + c.jitter * s[2] - p.z;
					const float d = float(std::sqrt(dx * dx + dy * dy + dz * dz)) * (c.lattice == Cells::BCC ? 1.26f : 1.f);
					d2 = std::min(d2, std::max(d1, d));
					d1 = std::min(d1, d);
					if (std::max({std::abs(i), std::abs(j), std::abs(k)}) == 2)
						outside = std::min(outside, d);
				}
	}
}

int main(int argc, char** argv) {
	const char* what = argc > 1 ? argv[1] : "fractal";
	char name[96];
	if (!std::strcmp(what, "fractal"))
		for (int fr = 0; fr < StockField::FRACTALS; ++fr)
			for (float rough : {0.25f, 0.5f, 1.f})
				for (float det : {0.f, 1.f, 3.f, 7.f}) {
					StockField f;
					f.fractal = StockField::Fractal(fr);
					f.detail = det;
					f.roughness = rough;
					std::snprintf(name, sizeof name, "%s r%.2f d%.0f", StockField::FRACTAL_NAMES[fr], rough, det);
					report(name, f);
				}
	if (!std::strcmp(what, "cells")) {
		for (int lat = 0; lat < Cells::LATTICES; ++lat)
			for (int m = 0; m < Cells::METRICS; ++m)
				for (int ft = 0; ft < Cells::FEATURES; ++ft)
					for (float s : {0.05f, 0.15f, 0.3f, 0.5f}) {
						if (ft < Cells::SMOOTH && s != 0.05f)
							continue;
						Cells c;
						c.feature = Cells::Feature(ft), c.metric = Cells::Metric(m), c.lattice = Cells::Lattice(lat), c.smoothness = s;
						std::snprintf(name, sizeof name, "%s %.5s %s s%.2f", lat ? "bcc" : "cub", Cells::METRIC_NAMES[m], Cells::FEATURE_NAMES[ft], s);
						report(name, [&](Vec3 p) { return c(0, p); }, 40000);
					}
		for (float j : {0.f, 0.5f}) {
			Cells c;
			c.feature = Cells::F1, c.jitter = j;
			std::snprintf(name, sizeof name, "cub F1 jitter %.1f", j);
			report(name, [&](Vec3 p) { return c(0, p); }, 40000);
		}
	}
	if (!std::strcmp(what, "exact"))
		for (int lat = 0; lat < Cells::LATTICES; ++lat) {
			Cells c;
			c.lattice = Cells::Lattice(lat);
			std::mt19937 rng(5);
			std::uniform_real_distribution<double> u(-40, 40);
			int wrongF2 = 0, far[4] = {};
			const float S[4] = {0.15f, 0.3f, 0.4f, 0.5f};
			const int N = 200000;
			for (int i = 0; i < N; ++i) {
				const Vec3 p{u(rng), u(rng), u(rng)};
				float d1, d2, outside;
				exact(c, 0, p, d1, d2, outside);
				c.feature = Cells::F2;
				wrongF2 += std::abs((c(0, p) + 1.917f) / 2.75f - d2) > 1e-4f;
				for (int k = 0; k < 4; ++k)
					far[k] += d1 + S[k] > outside; // an unsearched point is within the kernel's reach
			}
			std::printf("%s: F2 wrong %.4f%%; kernel past search at s 0.15/0.3/0.4/0.5: %.3f%% %.3f%% %.3f%% %.3f%%\n", lat ? "bcc" : "cubic",
			            100.0 * wrongF2 / N, 100.0 * far[0] / N, 100.0 * far[1] / N, 100.0 * far[2] / N, 100.0 * far[3] / N);
		}
}
