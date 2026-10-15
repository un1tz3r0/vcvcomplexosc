// One configuration of OrbitVoice::process for profiling under callgrind (build with -O2 -g).
#include <cstdio>
#include <cstdlib>
#include "core/Field.hpp"
#include "core/Orbit.hpp"
using namespace cxo;
int main(int argc, char** argv) {
	const double hz = argc > 1 ? std::atof(argv[1]) : 880;
	const int aa = argc > 2 ? std::atoi(argv[2]) : 2, oct = argc > 3 ? std::atoi(argv[3]) : 1;
	const OrbitShape shape{orbitRing({}, 1, 0.2, 0.3, 0.2, Ring::NORMAL, 0), OrbitShape::PINCH, 0.5};
	StockField f;
	f.octaves = oct;
	OrbitVoice v;
	v.setAntialias(Antialias::Level(aa));
	float out[2], sink = 0;
	for (int i = 0; i < 20000; ++i)
		v.process(shape, f, hz, 1 / 48000.0, true, true, out), sink += out[0];
	std::printf("%g\n", sink);
}
