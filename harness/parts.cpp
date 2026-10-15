// Times the pieces of one OrbitVoice sample in isolation (best of 7 runs), to find where a sample's time goes.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include "core/Field.hpp"
#include "core/Orbit.hpp"
using namespace cxo;
template <class F>
static double ns(F f) {
	double best = 1e18;
	for (int r = 0; r < 7; ++r) {
		const auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < 200000; ++i)
			f(i);
		best = std::min(best, std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / 200000);
	}
	return best;
}
volatile double sinkd;
volatile float sinkf;
int main() {
	const OrbitShape shape{orbitRing({}, 1, 0.2, 0.3, 0.2, Ring::NORMAL, 0), OrbitShape::PINCH, 0.5};
	StockField f;
	std::printf("ring.at           %6.1f ns\n", ns([&](int i) { sinkd = shape.ring.at(i * 1e-4).x; }));
	std::printf("ringFor(1)        %6.1f ns\n", ns([&](int i) { sinkd = shape.ringFor(1).center.x + i; }));
	std::printf("perimeter         %6.1f ns\n", ns([&](int i) { sinkd = perimeter(shape.ring) + i; }));
	std::printf("noise3            %6.1f ns\n", ns([&](int i) { sinkf = os2::noise3(0, i * 1e-3, 0.3, 0.1); }));
	std::printf("field 1 oct       %6.1f ns\n", ns([&](int i) { sinkf = f(Vec3{i * 1e-3, 0.3, 0.1}); }));
	const StockField lim = f.limited(3.0);
	std::printf("field 1 oct lim   %6.1f ns\n", ns([&](int i) { sinkf = lim(Vec3{i * 1e-3, 0.3, 0.1}); }));
	Decimator d(4);
	float in[4] = {0.1f, 0.2f, 0.3f, 0.4f};
	std::printf("decimator x4      %6.1f ns\n", ns([&](int i) { in[0] = float(i); sinkf = d.process(in); }));
	OrbitVoice v;
	float out[2];
	std::printf("voice off         %6.1f ns\n", ns([&](int) { v.process(shape, f, 110, 1 / 48000.0, true, true, out); }));
}
