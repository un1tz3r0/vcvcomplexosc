#pragma once
#include <cmath>

namespace cxo {

inline double voltsToHz(double volts) { return 261.6255653005986 * std::exp2(volts); }

// Phase accumulator in cycles, kept in [0, 1) at double precision so slow LFOs never drift.
struct Phasor {
	double phase = 0;

	double step(double hz, double dt) {
		phase += hz * dt;
		phase -= std::floor(phase);
		return phase;
	}
};

} // namespace cxo
