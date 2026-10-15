// Dumps a field read along many random lines, 64 samples per unit, as raw float32 for a spectrum (see spectrum.py).
// Usage: spectrum kind [feature] [smoothness] > samples.f32   kind: simplex3, gyroid, cellular, ...
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include "core/Field.hpp"
using namespace cxo;

int main(int argc, char** argv) {
	StockField f;
	for (int k = 0; k < StockField::KINDS; ++k)
		if (!strcasecmp(argv[1], StockField::NAMES[k]))
			f.kind = StockField::Kind(k);
	if (argc > 2)
		f.cells.feature = Cells::Feature(std::stoi(argv[2]));
	if (argc > 3)
		f.cells.smoothness = std::stof(argv[3]);
	if (argc > 4)
		f.shaper = StockField::Shaper(std::stoi(argv[4])), f.drive = 2;
	std::mt19937 rng(9);
	std::normal_distribution<double> g;
	std::uniform_real_distribution<double> u(-50, 50);
	constexpr int LINES = 64, N = 1 << 14;
	static float buf[N];
	for (int l = 0; l < LINES; ++l) {
		Vec3 d{g(rng), g(rng), g(rng)};
		d = d * (1 / length(d));
		const Vec3 o{u(rng), u(rng), u(rng)};
		for (int i = 0; i < N; ++i)
			buf[i] = f(o + d * (i / 64.0));
		fwrite(buf, sizeof(float), N, stdout);
	}
}
