#pragma once
#include <algorithm>
#include <cmath>

// Hard-to-soft sync of an oscillator's phase to an input of triggers or a ramp (see DESIGN.md).
namespace cxo {

// `x` wrapped into [-0.5, 0.5).
inline double wrapHalf(double x) { return x - std::floor(x + 0.5); }

struct Sync {
	enum Reading { AUTO, TRIGGERS, RAMP, READINGS };
	static constexpr const char* READING_NAMES[READINGS] = {"Auto", "Triggers", "Ramp"};
	static constexpr float LOW = 0.1f, HIGH = 1; // trigger thresholds in volts, as Rack's Schmitt triggers

	// How to move the oscillator's phase this sample: by `shift` cycles from fraction `at` of the way through it, from the
	// start if `at` is 0 and never if it is 1, reading on at `hz`.
	struct Step {
		double shift = 0, at = 1, hz;
	};

	float last = 0;
	bool high = false, ramp = false;
	double mean = 0, speed = 0; // a ramp's phase step per sample, signed and unsigned, averaged over about 20 ms
	double error = 0;           // the latest phase error, input minus oscillator, in cycles
	double quiet = 1e9;         // seconds since the error was last measured

	// `v` is the input in volts, `hardness` the knob from 0 (soft) to 1 (hard), and `phase` and `hz` the oscillator's
	// phase at the coming sample and its frequency.
	Step process(float v, float hardness, Reading reading, double phase, double hz, double dt) {
		Step s;
		s.hz = hz;
		quiet += dt;
		// A ramp reads as phase at 10 V per cycle. Jumps of more than a quarter cycle are edges, not motion, and a whole
		// cycle's jump, a saw's reset, doesn't register at all.
		const double in = v * 0.1 - std::floor(v * 0.1), d = wrapHalf(in - (last * 0.1 - std::floor(last * 0.1))), moved = std::abs(d) < 0.25 ? d : 0;
		const double k = 1 - std::exp(-dt / 0.02);
		mean += (moved - mean) * k;
		speed += (std::abs(moved) - speed) * k;
		// A ramp moves steadily one way; triggers and gates hold still between edges, and sines go both ways.
		if (std::abs(mean) > 0.9 * speed && speed > 1e-6)
			ramp = true;
		else if (std::abs(mean) < 0.6 * speed || speed < 5e-7)
			ramp = false;
		const float before = last;
		last = v;

		if (reading == RAMP || (reading == AUTO && ramp)) {
			// Hard follows the ramp outright; softer pulls toward it ever more gently, so FREQ matters more.
			error = wrapHalf(in - phase);
			quiet = 0;
			s.at = 0;
			if (hardness >= 1) {
				s.shift = error;
				s.hz = mean / dt;
			} else {
				const double rate = hardness > 0 ? 0.25 * std::exp2(18 * hardness) : 0; // pulls per second
				s.shift = error * (1 - std::exp(-rate * dt));
			}
			return s;
		}

		if (high ? v > LOW : v < HIGH)
			return s;
		high = !high;
		if (!high)
			return s;
		// The edge crossed HIGH this far into the last sample, so it takes effect as far into the coming one: a one-sample
		// latency that lets the jump land where it belongs.
		const double at = std::clamp(1 - double(v - HIGH) / std::max(v - before, 1e-6f), 0.0, 1.0);
		const double there = phase + hz * dt * at;
		error = std::round(there) - there;
		quiet = 0;
		// The oscillator resets only within a window of phase 0, from half a cycle (always) down to nothing.
		if (std::abs(error) <= 0.5 * hardness * hardness) {
			s.shift = error;
			s.at = at;
		}
		return s;
	}
};

} // namespace cxo
