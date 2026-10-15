// Percent of a core at 48 kHz for one Orbit voice (A and B, AUTO oversampling) per field setting.
// Build: g++ -O3 -march=nehalem -funsafe-math-optimizations -std=c++17 -I<repo>/src fieldcost.cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include "core/Field.hpp"
#include "core/Orbit.hpp"
using namespace cxo;

static volatile float sink;

static double percent(const StockField& field, double hz) {
	const OrbitShape shape{orbitRing({0.3, 0.2, 0.1}, 1, 0.2, 0, 0, Ring::NORMAL, 0), OrbitShape::PHASE, 0.5};
	double best = 1e18;
	for (int run = 0; run < 5; ++run) {
		OrbitVoice v;
		v.setAntialias(Antialias::AUTO);
		float out[2], sum = 0;
		constexpr int N = 30000;
		const auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < N; ++i) {
			v.process(shape, field, hz, 1 / 48000.0, true, true, out, 3);
			sum += out[0] + out[1];
		}
		sink = sum;
		best = std::min(best, std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / N);
	}
	return best / (1e9 / 48000) * 100;
}

int main() {
	const std::pair<const char*, std::function<void(StockField&)>> cases[] = {
		{"simplex3", [](StockField&) {}},
		{"simplex3 detail 3", [](StockField& f) { f.detail = 3; }},
		{"simplex3 warp+tilt", [](StockField& f) { f.warp = 0.5f, f.tilt = 1; }},
		{"cellular smooth", [](StockField& f) { f.kind = StockField::CELLULAR; }},
		{"cellular f1", [](StockField& f) { f.kind = StockField::CELLULAR, f.cells.feature = Cells::F1; }},
		{"cellular smooth bcc", [](StockField& f) { f.kind = StockField::CELLULAR, f.cells.lattice = Cells::BCC; }},
		{"cellular smooth detail 3", [](StockField& f) { f.kind = StockField::CELLULAR, f.detail = 3; }},
	};
	for (const auto& [name, set] : cases) {
		StockField f;
		set(f);
		std::printf("%-26s 110 Hz %5.1f%%   880 Hz %5.1f%%\n", name, percent(f, 110), percent(f, 880));
	}
}
