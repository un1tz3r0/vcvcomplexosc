#include <chrono>
#include <cstdio>
#include "core/Field.hpp"
using namespace cxo;
template <class F> double bench(F f) {
	const auto t0 = std::chrono::steady_clock::now();
	float s = 0;
	for (int i = 0; i < 2000000; ++i) s += f(i);
	static volatile float sink; sink = s;
	return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / 2e6;
}
int main() {
	StockField f;
	for (int r = 0; r < 2; ++r)
	std::printf("noise3 %.1f ns  field %.1f ns\n", bench([](int i) { return os2::noise3(0, i * 1e-3, 0.3, 0.1); }), bench([&](int i) { return f(Vec3{i * 1e-3, 0.3, 0.1}); }));
}
