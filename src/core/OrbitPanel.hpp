#pragma once
#include <utility>
#include "Panel.hpp"

// Orbit's controls and their layout, shared by the module and tools/render.
namespace cxo::orbit {

constexpr int MODS = 12, OUTS = 4;

// The first MODS params each have a CV input and an attenuverter at the same offset.
enum ParamId {
	FREQ_PARAM, X_PARAM, Y_PARAM, Z_PARAM, MAJOR_PARAM, MINOR_PARAM,
	DRIFT_PARAM, ROTATE_PARAM, TILT_PARAM, AZIMUTH_PARAM, SPREAD_PARAM, ANGLE_PARAM,
	ATTEN_PARAM,
	FIELD_PARAM = ATTEN_PARAM + MODS, OCTAVES_PARAM, MODE_PARAM, RANGE_PARAM, AXIS_PARAM,
	PARAMS_LEN
};
enum InputId { CV_INPUT, VOCT_INPUT = CV_INPUT + MODS, SYNC_INPUT, INPUTS_LEN };
enum OutputId { WAVE_OUTPUT, X_OUTPUT = WAVE_OUTPUT + OUTS, Y_OUTPUT, Z_OUTPUT, OUTPUTS_LEN };

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
	{"R MAJOR", "Major radius", 0, 4, 1.2f, "", 1, "%.2f", SIZE},
	{"R MINOR", "Minor radius", 0, 4, 0.8f, "", 1, "%.2f", SIZE},
	{"DRIFT", "Drift along Z", -1, 1, 0, " units/s", 1, "%+.2f", MOTION},
	{"ROTATE", "Rotation rate", -1, 1, 0, " rev/s", 1, "%+.2f", MOTION},
	{"TILT", "Tilt", -90, 90, 0, "°", 1, "%+.0f°", TURN},
	{"AZIMUTH", "Azimuth", 0, 360, 0, "°", 1, "%.0f°", TURN},
	{"SPREAD", "Spread", 0, 1, 1, "%", 100, "%.0f%%", SPLIT},
	{"ANGLE", "Angle", 0, 360, 0, "°", 1, "%.0f°", SPLIT},
};

inline Panel panel() {
	Panel p{152.4f, 128.5f, {{4.5f, 10.5f}, {127.f, 47.f}}};
	p.text({76.2f, 5.2f}, "orbit", Panel::TITLE);
	p.text({76.2f, 125.9f}, "un1tz3r0", Panel::BRAND);

	// Two rows of knobs, the second half a step over so its wires pass between the first's. Each knob has its
	// attenuverter and CV input below it. Reading along the zigzag keeps related knobs, and their values, together.
	constexpr int ORDER[MODS] = {X_PARAM, Y_PARAM, Z_PARAM, MAJOR_PARAM, MINOR_PARAM, TILT_PARAM,
	                             AZIMUTH_PARAM, SPREAD_PARAM, ANGLE_PARAM, FREQ_PARAM, DRIFT_PARAM, ROTATE_PARAM};
	constexpr float ROWS[] = {61.5f, 89.f}, STEP = 10.2f;
	for (int s = 0; s < MODS; ++s) {
		const int id = ORDER[s];
		const Vec2f at{9.65f + STEP * s, ROWS[s % 2]};
		p.knobUnder(id, at, MOD[id].label, MOD[id].color);
		p.controls.push_back({Panel::TRIMPOT, ATTEN_PARAM + id, {at.x - 4.65f, at.y + 11.5f}, {}});
		p.controls.push_back({Panel::INPUT, CV_INPUT + id, {at.x + 3.7f, at.y + 11.5f}, {}});
	}

	// Global controls down the right: selectors beside the screen, then pitch and sync with labels in line with the rows'.
	const std::pair<int, const char*> selectors[] = {
		{FIELD_PARAM, "FIELD"}, {OCTAVES_PARAM, "OCTAVES"}, {MODE_PARAM, "MODE"}, {AXIS_PARAM, "AXIS"}, {RANGE_PARAM, "RANGE"}};
	for (int i = 0; i < 5; ++i)
		p.selectorBeside(selectors[i].first, {143.5f, 12.5f + 7.f * i}, selectors[i].second, SELECT);
	p.jack(Panel::INPUT, VOCT_INPUT, {143.5f, Panel::labelAbove({0, ROWS[0]}) + Panel::JACK_LABEL}, "V/OCT");
	p.jack(Panel::INPUT, SYNC_INPUT, {143.5f, Panel::labelAbove({0, ROWS[1]}) + Panel::JACK_LABEL}, "SYNC");

	const char* outs[OUTS + 3] = {"OUT 1", "OUT 2", "OUT 3", "OUT 4", "X", "Y", "Z"};
	float x = 40.45f;
	for (int k = 0; k < OUTS + 3; ++k, x += 11.5f) {
		x += k == OUTS ? 2.5f : 0.f;
		p.jack(Panel::OUTPUT, WAVE_OUTPUT + k, {x, 116.5f}, outs[k], Panel::BOXED);
	}
	p.boxes.push_back({{33.45f, 107.5f}, {x - 4.5f, 123.f}});
	return p;
}

} // namespace cxo::orbit
