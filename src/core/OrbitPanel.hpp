#pragma once
#include <utility>
#include "Panel.hpp"

// Orbit's controls and their layout, shared by the module and tools/render.
namespace cxo::orbit {

constexpr int MODS = 11, OUTS = 2;

// The first MODS params each have a CV input and an attenuverter at the same offset.
enum ParamId {
	FREQ_PARAM, X_PARAM, Y_PARAM, Z_PARAM, RADIUS_PARAM, ECCENTRICITY_PARAM,
	DRIFT_PARAM, ROTATE_PARAM, PITCH_PARAM, ROLL_PARAM, SPREAD_PARAM,
	ATTEN_PARAM,
	FIELD_PARAM = ATTEN_PARAM + MODS, OCTAVES_PARAM, MODE_PARAM, RANGE_PARAM, AXIS_PARAM, SYNC_PARAM,
	PARAMS_LEN
};
enum InputId { CV_INPUT, VOCT_INPUT = CV_INPUT + MODS, SYNC_INPUT, INPUTS_LEN };
enum OutputId { WAVE_OUTPUT, X_OUTPUT = WAVE_OUTPUT + OUTS, Y_OUTPUT, Z_OUTPUT, OUTPUTS_LEN };
enum LightId { SYNC_LIGHT, LIGHTS_LEN = SYNC_LIGHT + 3 }; // red, green and blue

enum Range { VLFO, LFO, AUDIO, RANGES };
inline constexpr const char* RANGE_NAMES[RANGES] = {"VLFO", "LFO", "AUDIO"};
inline constexpr double RANGE_DIVISOR[RANGES] = {256 * 64, 256, 1}; // of C4, FREQ's center

// This plugin's knob colours, one per kind of control.
inline constexpr Rgba PLACE = hex(0xf1cd8c), SIZE = hex(0xbcd9a0), TURN = hex(0x98d4cf), SPLIT = hex(0xadb8f0), MOTION = hex(0xf4a99a),
                      SELECT = hex(0xd9d8d4);

struct Mod {
	const char* label; // on the panel
	const char* name;  // in tooltips
	float min, max, def;
	const char* unit;
	float scale;        // displayed value per unit
	const char* format; // on screen; none for the frequency, which is formatted as Hz
	Rgba color;
};

inline constexpr Mod MOD[MODS] = {
	{"FREQ", "Frequency", -4, 4, 0, " Hz", 1, nullptr, MOTION},
	{"X", "Center X", -5, 5, 0, "", 1, "%+.2f", PLACE},
	{"Y", "Center Y", -5, 5, 0, "", 1, "%+.2f", PLACE},
	{"Z", "Center Z", -5, 5, 0, "", 1, "%+.2f", PLACE},
	{"RADIUS", "Radius", 0, 4, 1, "", 1, "%.2f", SIZE},
	{"ECCENTRIC", "Eccentricity", -1, 1, 0.2f, "", 1, "%+.2f", SIZE},
	{"DRIFT", "Drift along Z", -1, 1, 0, " units/s", 1, "%+.2f", MOTION},
	{"ROTATE", "Rotation rate", -1, 1, 0, " rev/s", 1, "%+.2f", MOTION},
	{"PITCH", "Pitch", -90, 90, 0, "°", 1, "%+.0f°", TURN},
	{"ROLL", "Roll", -90, 90, 0, "°", 1, "%+.0f°", TURN},
	{"SPREAD", "Spread", -1, 1, 0.5f, "%", 100, "%+.0f%%", SPLIT},
};

inline Panel panel() {
	Panel p{152.4f, 128.5f, {{25.5f, 10.5f}, {147.9f, 52.2f}}};
	p.text({76.2f, 5.2f}, "orbit", Panel::TITLE);
	p.text({76.2f, 125.9f}, "un1tz3r0", Panel::BRAND);

	// Two rows of knobs, the second half a step over so its wires pass between the first's. Each knob has its
	// attenuverter and CV input below it. Reading along the zigzag keeps related knobs, and their values, together.
	constexpr int ORDER[MODS] = {X_PARAM,    Y_PARAM,      Z_PARAM,   RADIUS_PARAM, ECCENTRICITY_PARAM, PITCH_PARAM,
	                             ROLL_PARAM, SPREAD_PARAM, FREQ_PARAM, DRIFT_PARAM,  ROTATE_PARAM};
	constexpr float ROWS[] = {66.f, 90.5f}, STEP = 11.2f, BELOW = 10.f;
	for (int s = 0; s < MODS; ++s) {
		const int id = ORDER[s];
		const Vec2f at{31.f + STEP * s, ROWS[s % 2]};
		p.knobUnder(id, at, MOD[id].label, MOD[id].color);
		p.controls.push_back({Panel::TRIMPOT, ATTEN_PARAM + id, {at.x - 4.65f, at.y + BELOW}, {}});
		p.controls.push_back({Panel::INPUT, CV_INPUT + id, {at.x + 3.7f, at.y + BELOW}, {}});
	}

	// Global controls down the left: selectors and the range switch beside the screen, then pitch and sync.
	constexpr float LEFT = 8.f;
	const std::pair<int, const char*> selectors[] = {{FIELD_PARAM, "FIELD"}, {OCTAVES_PARAM, "OCTAVES"}, {MODE_PARAM, "MODE"}, {AXIS_PARAM, "AXIS"}};
	for (int i = 0; i < 4; ++i)
		p.beside(Panel::SELECTOR, selectors[i].first, {LEFT, 13.8f + 7.8f * i}, selectors[i].second, SELECT);
	p.beside(Panel::SWITCH, RANGE_PARAM, {LEFT, 13.8f + 7.8f * 4}, "RANGE", SELECT);
	p.jack(Panel::INPUT, VOCT_INPUT, {LEFT, Panel::labelAbove({0, ROWS[0]}) + Panel::JACK_LABEL}, "V/OCT");
	p.jack(Panel::INPUT, SYNC_INPUT, {LEFT, Panel::labelAbove({0, ROWS[1]}) - 4.f}, "SYNC");
	const Vec2f lock{LEFT, ROWS[1] + BELOW - 1.f};
	p.controls.push_back({Panel::SELECTOR, SYNC_PARAM, lock, SELECT});
	p.text({LEFT - 1.6f, Panel::labelAbove(lock, Panel::SELECTOR_SIZE)}, "LOCK");
	p.controls.push_back({Panel::LIGHT, SYNC_LIGHT, {LEFT + 4.2f, Panel::labelAbove(lock, Panel::SELECTOR_SIZE)}, {}});

	const char* outs[OUTS + 3] = {"A", "B", "X", "Y", "Z"};
	float x = 53.2f;
	for (int k = 0; k < OUTS + 3; ++k, x += 11.5f) {
		x += k == OUTS ? 2.5f : 0.f;
		p.jack(Panel::OUTPUT, WAVE_OUTPUT + k, {x, 116.5f}, outs[k], Panel::BOXED);
	}
	p.boxes.push_back({{53.2f - 7.f, 107.5f}, {x - 4.5f, 123.f}});
	return p;
}

} // namespace cxo::orbit
