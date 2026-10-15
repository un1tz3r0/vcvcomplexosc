#pragma once
#include <cmath>
#include "Vec.hpp"

namespace cxo {

// Brings a signal oversampled by 2 or 4 back to the base rate through a Blackman-windowed sinc low-pass that is flat
// to about 18 kHz at 48 kHz and over 70 dB down from where it would fold back below 20 kHz. Factor 1 passes through.
struct Decimator {
	static constexpr int MAX_FACTOR = 4, TAPS_PER_FACTOR = 32, MAX_TAPS = MAX_FACTOR * TAPS_PER_FACTOR;

	int factor = 1, taps = 0, at = 0;
	float kernel[MAX_TAPS] = {}, history[2 * MAX_TAPS] = {}; // history holds the taps twice over, so a window of it is contiguous

	explicit Decimator(int factor = 1) : factor(factor), taps(factor > 1 ? TAPS_PER_FACTOR * factor : 0) {
		const double cutoff = 0.48 / factor; // in cycles per oversampled sample
		double sum = 0;
		for (int i = 0; i < taps; ++i) {
			const double t = i - 0.5 * (taps - 1), x = TAU * cutoff * t, w = TAU * i / (taps - 1);
			sum += kernel[i] = float((x == 0 ? 1 : std::sin(x) / x) * (0.42 - 0.5 * std::cos(w) + 0.08 * std::cos(2 * w)));
		}
		for (int i = 0; i < taps; ++i)
			kernel[i] = float(kernel[i] / sum);
	}

	// Takes `factor` samples, oldest first, and returns one.
	float process(const float* in) {
		if (taps == 0)
			return in[0];
		for (int i = 0; i < factor; ++i) {
			history[at] = history[at + taps] = in[i];
			at = (at + 1) % taps;
		}
		float y = 0;
		for (int i = 0; i < taps; ++i)
			y += kernel[i] * history[at + i];
		return y;
	}
};

} // namespace cxo
