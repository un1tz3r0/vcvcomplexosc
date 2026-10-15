// Times OrbitVoice::process per sample and per part, for one voice, as percent of a core at 48 kHz.
// Build: g++ -O3 -march=nehalem -funsafe-math-optimizations -std=c++17 -I<repo>/src voicecost.cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include "core/Field.hpp"
#include "core/Orbit.hpp"
using namespace cxo;

static volatile float sink; // keeps the outputs, so the work can't be optimized away

static double nsPerSample(const OrbitShape& shape, const StockField& field, double hz, Antialias::Level aa, unsigned wanted = 3) {
	double best = 1e18; // the fastest of several runs, the one least disturbed by everything else on the machine
	for (int run = 0; run < 7; ++run) {
		OrbitVoice v;
		v.setAntialias(aa);
		float out[2];
		constexpr int N = 60000;
		const auto t0 = std::chrono::steady_clock::now();
		float sum = 0;
		for (int i = 0; i < N; ++i) {
			v.process(shape, field, hz, 1 / 48000.0, true, true, out, wanted);
			sum += out[0] + out[1];
		}
		sink = sum;
		best = std::min(best, std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / N);
	}
	return best;
}

int main() {
	const OrbitShape shape{orbitRing({}, 1, 0.2, 0.3, 0.2, Ring::NORMAL, 0), OrbitShape::PINCH, 0.5};
	for (int oct : {1, 4})
		for (double hz : {110.0, 880.0})
			for (int aa = 0; aa < Antialias::LEVELS; ++aa) {
				StockField f;
				f.detail = oct - 1;
				const double ns = nsPerSample(shape, f, hz, Antialias::Level(aa));
				std::printf("oct %d %5.0f Hz %-32s %6.0f ns/sample  %5.2f%% of a core\n", oct, hz, Antialias::NAMES[aa], ns, ns * 48000 / 1e7);
			}
	StockField f;
	OrbitVoice v;
	const auto t0 = std::chrono::steady_clock::now();
	float sink = 0;
	for (int i = 0; i < 1000000; ++i)
		sink += f(Vec3{i * 1e-3, 0.3, 0.1});
	std::printf("one noise3 read: %.1f ns (%g)\n", std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / 1e6, sink);
}
