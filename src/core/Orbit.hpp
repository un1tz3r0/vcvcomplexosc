#pragma once
#include <algorithm>
#include <vector>
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

// One voice of an orbit oscillator. Outputs are optionally centered and normalized to about [-1, 1]
// using surveys of each ring taken every SURVEY_PERIOD samples, eased in so parameter changes don't click.
struct OrbitVoice {
	static constexpr int MAX_OUTPUTS = 8, SURVEY_PERIOD = 256;
	static constexpr double EASE_SECONDS = 0.01;

	Phasor phasor;
	double drift = 0; // Z offset accumulated by the caller
	double turn = 0;  // rotation in cycles accumulated by the caller
	float offset[MAX_OUTPUTS] = {}, gain[MAX_OUTPUTS] = {}, targetOffset[MAX_OUTPUTS] = {}, targetGain[MAX_OUTPUTS] = {};
	int countdown = 0;
	bool primed = false;

	// Writes shape.count outputs, skipping those whose bit in `wanted` is clear, then advances the phase.
	// Returns the phase the outputs were read at.
	template <class Field>
	double process(const OrbitShape& shape, const Field& field, double hz, double dt, bool center, bool normalize, float* out,
	               unsigned wanted = ~0u) {
		if (--countdown < 0) {
			countdown = SURVEY_PERIOD;
			survey(shape, field, center, normalize);
		}
		const float ease = float(std::min(1.0, dt / EASE_SECONDS));
		const double phase = phasor.phase;
		for (int k = 0; k < shape.count; ++k) {
			offset[k] += (targetOffset[k] - offset[k]) * ease;
			gain[k] += (targetGain[k] - gain[k]) * ease;
			if (wanted >> k & 1)
				out[k] = gain[k] * (field(shape.probe(k, phase)) - offset[k]);
		}
		phasor.step(hz, dt);
		return phase;
	}

	template <class Field>
	void survey(const OrbitShape& shape, const Field& field, bool center, bool normalize) {
		for (int k = 0; k < shape.count; ++k) {
			if (k > 0 && shape.shared()) {
				targetOffset[k] = targetOffset[0], targetGain[k] = targetGain[0];
				continue;
			}
			const RingSurvey s(shape.ringFor(k), field);
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
