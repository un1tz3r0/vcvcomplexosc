// Hard sync with exact reset times, for checking how well OrbitVoice band-limits its jumps: writes A as raw float32.
// Usage: synccheck oscHz syncHz blend(0|1) antialias > out.f32
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "core/Field.hpp"
#include "core/Orbit.hpp"
using namespace cxo;

int main(int argc, char** argv) {
	const double hz = atof(argv[1]), sync = atof(argv[2]), dt = 1 / 48000.0;
	const bool blend = atoi(argv[3]);
	OrbitVoice v;
	v.setAntialias(Antialias::Level(atoi(argv[4])));
	StockField f;
	const OrbitShape shape{orbitRing({0.3, 0.2, 0.1}, 1, 0.2, 0, 0, Ring::NORMAL, 0), OrbitShape::PHASE, 0.5};
	for (long n = 0; n < 48000 * 4; ++n) {
		// The reset falls `at` of the way through this sample if an edge time lies in [n, n+1).
		const double e0 = std::ceil(n * dt * sync), te = e0 / sync / dt; // next edge, in samples
		double shift = 0, at = 1;
		if (te < n + 1) {
			at = te - n;
			const double there = v.phasor.phase + hz * dt * at;
			shift = -(there - std::floor(there));
			if (!blend)
				at = 0, v.phasor.phase += shift - std::floor(v.phasor.phase + shift), shift = 0, at = 1;
		}
		float out[2];
		v.process(shape, f, hz, dt, false, false, out, 1, shift, at);
		fwrite(out, 4, 1, stdout);
	}
}
