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

// Radius of the smallest ball about the first ring's center that holds every trace.
inline double reach(const std::vector<Trace>& traces) {
	double r = 0;
	for (const Trace& t : traces)
		r = std::max(r, length(t.ring.center - traces.front().ring.center) + std::max(t.ring.major, t.ring.minor));
	return r;
}

// Where each of an orbit oscillator's outputs reads its field.
// Phase mode: every output reads the same ring, the k-th one `spread·k/count` of a cycle ahead.
// Pinch mode: output k reads a copy of the ring scaled about its point at `angle`, so all copies meet there
// and fan apart toward the opposite side; `spread` is the scale difference between the outermost copies.
struct OrbitShape {
	Ring ring;
	bool pinch = false;
	double spread = 0.5, angle = 0; // angle in cycles
	int count = 4;

	double scale(int k) const { return count > 1 ? 1 + spread * (double(k) / (count - 1) - 0.5) : 1; }
	Ring ringFor(int k) const { return pinch ? ring.pinched(angle, scale(k)) : ring; }
	double phaseFor(int k, double phase) const { return pinch ? phase : phase + angle + spread * k / count; }
	Vec3 probe(int k, double phase) const { return ringFor(k).at(phaseFor(k, phase)); }

	std::vector<Trace> traces(double phase) const {
		if (!pinch) {
			Trace t{ring, {}};
			for (int k = 0; k < count; ++k)
				t.phases.push_back(phaseFor(k, phase));
			return {t};
		}
		std::vector<Trace> ts;
		for (int k = 0; k < count; ++k)
			ts.push_back({ringFor(k), {phase}});
		return ts;
	}
};

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
			if (k > 0 && !shape.pinch) { // every output reads the same ring
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
