#pragma once
#include <algorithm>
#include <vector>
#include "Decimator.hpp"
#include "Phasor.hpp"
#include "Ring.hpp"

namespace cxo {

// A ring and the phases currently being read from it.
struct Trace {
	Ring ring;
	std::vector<double> phases;
};

// Radius of the smallest ball about `center` that holds every trace.
inline double reach(Vec3 center, const std::vector<Trace>& traces) {
	double r = 0;
	for (const Trace& t : traces)
		r = std::max(r, length(t.ring.center - center) + std::max(std::abs(t.ring.major), std::abs(t.ring.minor)));
	return r;
}

// Where each of an orbit oscillator's outputs reads its field, by spread mode:
// - Phase: every output reads the same ring, the k-th one `spread·k/count` of a cycle ahead, all offset by `angle`.
// - Pinch: copies of the ring scaled about its point at `angle`, so all meet there and fan apart toward the opposite side.
// - Stack: copies shifted along the normal, `spread` mean diameters from the lowest to the highest.
// - Radial: concentric copies scaled about the center.
// - Fan: copies turned about the diameter through `angle`, `spread` half turns from the first to the last,
//   so they meet at both its ends.
// Pinch and radial copies differ in scale by `spread` from the smallest to the largest. Stack and radial copies are
// all read `angle` ahead; pinch and fan copies are read in step, so their meeting points coincide in time too.
struct OrbitShape {
	enum Mode { PHASE, PINCH, STACK, RADIAL, FAN, MODES };
	static constexpr const char* MODE_NAMES[MODES] = {"PHASE", "PINCH", "STACK", "RADIAL", "FAN"};

	Ring ring;
	Mode mode = PHASE;
	double spread = 0.5, angle = 0; // angle in cycles
	int count = 4;

	// Output k's place in the family, from -0.5 to 0.5.
	double rank(int k) const { return count > 1 ? double(k) / (count - 1) - 0.5 : 0; }

	Ring ringFor(int k) const {
		switch (mode) {
			case PINCH: return ring.pinched(angle, 1 + spread * rank(k));
			case STACK: return ring.shifted(ring.basis.z * (spread * (ring.major + ring.minor) * rank(k)));
			case RADIAL: return ring.scaled(1 + spread * rank(k));
			case FAN: return ring.hinged(angle, PI * spread * rank(k));
			default: return ring;
		}
	}
	double phaseFor(int k, double phase) const {
		switch (mode) {
			case PHASE: return phase + angle + spread * k / count;
			case PINCH:
			case FAN: return phase;
			default: return phase + angle;
		}
	}
	Vec3 probe(int k, double phase) const { return ringFor(k).at(phaseFor(k, phase)); }
	bool shared() const { return mode == PHASE; } // whether every output reads the same ring

	std::vector<Trace> traces(double phase) const {
		if (shared()) {
			Trace t{ring, {}};
			for (int k = 0; k < count; ++k)
				t.phases.push_back(phaseFor(k, phase));
			return {t};
		}
		std::vector<Trace> ts;
		for (int k = 0; k < count; ++k)
			ts.push_back({ringFor(k), {phaseFor(k, phase)}});
		return ts;
	}
};

// A ring as its knobs set it, then turned `turn` cycles about `axis`. Tilt and azimuth are in radians.
inline Ring orbitRing(Vec3 center, double major, double minor, double tilt, double azimuth, Ring::Axis axis, double turn) {
	const Ring r{center, Ring::orient(tilt, azimuth), major, minor};
	return r.turned(r.axis(axis), TAU * turn);
}

// Mean and range of a field around a ring, from evenly spaced samples.
struct RingSurvey {
	float mean = 0, lo = -1, hi = 1;

	template <class Field>
	RingSurvey(const Ring& ring, const Field& field, int samples = 48) : lo(1e9f), hi(-1e9f) {
		double sum = 0;
		for (int i = 0; i < samples; ++i) {
			const float v = field(ring.at(double(i) / samples));
			sum += v;
			lo = std::min(lo, v);
			hi = std::max(hi, v);
		}
		mean = float(sum / samples);
	}
};

// The length of an ellipse, by Ramanujan's approximation.
inline double perimeter(const Ring& r) {
	const double a = std::abs(r.major), b = std::abs(r.minor);
	return PI * (3 * (a + b) - std::sqrt((3 * a + b) * (a + 3 * b)));
}

// How an orbit voice keeps its outputs from aliasing. Fade: octaves above the first fade out before their detail
// reaches Nyquist. Auto adds oversampling for the first octave, which can't fade: each output is read 1, 2 or 4 times
// a sample, as few as the ring's speed through the field allows, interpolated up to 4 and filtered back down.
struct Antialias {
	enum Level { OFF, FADE, AUTO, LEVELS };
	static constexpr const char* NAMES[LEVELS] = {"Off", "Octave fade", "Octave fade + auto oversampling"};
};

// One voice of an orbit oscillator. Outputs are optionally centered and normalized to about [-1, 1]
// using surveys of each ring taken every SURVEY_PERIOD samples, eased in so parameter changes don't click.
struct OrbitVoice {
	static constexpr int MAX_OUTPUTS = 8, SURVEY_PERIOD = 512, OVERSAMPLE = Decimator::MAX_FACTOR;
	static constexpr double EASE_SECONDS = 0.01;

	Phasor phasor;
	double drift = 0; // Z offset accumulated by the caller
	double turn = 0;  // rotation in cycles accumulated by the caller
	float offset[MAX_OUTPUTS] = {}, gain[MAX_OUTPUTS] = {}, targetOffset[MAX_OUTPUTS] = {}, targetGain[MAX_OUTPUTS] = {};
	int countdown = 0;
	bool primed = false;
	Antialias::Level antialias = Antialias::OFF;
	Decimator decimators[MAX_OUTPUTS];
	float last[MAX_OUTPUTS] = {}; // each output's latest oversampled read, which the next sample interpolates from

	void setAntialias(Antialias::Level level) {
		if (level == antialias)
			return;
		antialias = level;
		for (Decimator& d : decimators)
			d = Decimator(level == Antialias::AUTO ? OVERSAMPLE : 1);
	}

	// Writes shape.count outputs, skipping those whose bit in `wanted` is clear, then advances the phase.
	// Returns the phase the outputs were read at.
	template <class Field>
	double process(const OrbitShape& shape, const Field& field, double hz, double dt, bool center, bool normalize, float* out,
	               unsigned wanted = ~0u) {
		const int count = std::min(shape.count, MAX_OUTPUTS);
		Ring rings[MAX_OUTPUTS];
		Field fields[MAX_OUTPUTS];
		int factor[MAX_OUTPUTS];
		for (int k = 0; k < count; ++k) {
			rings[k] = k > 0 && shape.shared() ? rings[0] : shape.ringFor(k);
			factor[k] = 1;
			fields[k] = field;
			// The read covers perimeter·hz units a second, so detail finer than that over Nyquist would alias.
			const double speed = perimeter(rings[k]) * hz, nyquist = 0.5 / dt;
			if (antialias == Antialias::OFF || speed <= 0)
				continue;
			if (antialias == Antialias::AUTO)
				for (const double need = field.detail() * speed / nyquist; factor[k] < OVERSAMPLE && need > 0.5 * factor[k];)
					factor[k] *= 2;
			fields[k] = field.limited(factor[k] * nyquist / speed);
		}
		if (--countdown < 0) {
			countdown = SURVEY_PERIOD;
			survey(shape, rings, fields, center, normalize);
		}
		const float ease = float(std::min(1.0, dt / EASE_SECONDS));
		const double phase = phasor.phase;
		for (int k = 0; k < count; ++k) {
			offset[k] += (targetOffset[k] - offset[k]) * ease;
			gain[k] += (targetGain[k] - gain[k]) * ease;
			if (!(wanted >> k & 1))
				continue;
			const auto read = [&](double at) { return fields[k](rings[k].at(shape.phaseFor(k, phase + hz * dt * at))); };
			float value;
			if (antialias != Antialias::AUTO)
				value = read(0);
			else {
				// Reads land at the ends of equal steps through the sample, the last at its final quarter, and
				// the quarters between are interpolated from them and from the last read of the sample before.
				float sub[OVERSAMPLE];
				const int step = OVERSAMPLE / factor[k];
				for (int j = 0, end = step - 1; j < factor[k]; ++j, end += step) {
					const float from = last[k], to = last[k] = read(double(end) / OVERSAMPLE);
					for (int i = end - step + 1; i <= end; ++i)
						sub[i] = from + (to - from) * float(i - end + step) / step;
				}
				value = decimators[k].process(sub);
			}
			out[k] = gain[k] * (value - offset[k]);
		}
		phasor.step(hz, dt);
		return phase;
	}

	template <class Field>
	void survey(const OrbitShape& shape, const Ring* rings, const Field* fields, bool center, bool normalize) {
		for (int k = 0; k < std::min(shape.count, MAX_OUTPUTS); ++k) {
			if (k > 0 && shape.shared()) {
				targetOffset[k] = targetOffset[0], targetGain[k] = targetGain[0];
				continue;
			}
			const RingSurvey s(rings[k], fields[k]);
			targetOffset[k] = center ? s.mean : 0.f;
			targetGain[k] = normalize ? 1.f / std::max({s.hi - targetOffset[k], targetOffset[k] - s.lo, 0.05f}) : 1.f;
		}
		if (!primed) {
			std::copy(targetOffset, targetOffset + MAX_OUTPUTS, offset);
			std::copy(targetGain, targetGain + MAX_OUTPUTS, gain);
			primed = true;
		}
	}
};

} // namespace cxo
