// Headless orbit renderer: audio to a multichannel float WAV, a two-cycle scope, the display and a panel mockup to SVG.
// Usage: render [--option value]...   (see Options for names and defaults; angles in degrees)
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <algorithm>
#include <map>
#include <sstream>
#include "core/Field.hpp"
#include "core/Orbit.hpp"
#include "core/OrbitScene.hpp"
#include "PanelSvg.hpp"
#include "SvgPainter.hpp"

using namespace cxo;

struct Options {
	double freq = 110, seconds = 2, rate = 48000;
	std::string field = "simplex3"; // any of StockField::NAMES, in any case
	int64_t seed = 0;
	int octaves = 1;
	Vec3 center;
	double major = 1, minor = 1, tilt = 0, azimuth = 0;
	double drift = 0;               // center travel along Z, units per second
	double rotate = 0;              // turns per second about the axis
	std::string axis = "normal";    // any of Ring::AXIS_NAMES, in any case
	std::string mode = "phase";     // any of OrbitShape::MODE_NAMES, in any case
	int outputs = 1;
	double spread = 0.5, angle = 0; // angle in degrees
	bool removeDc = true, normalize = true;
	int antialias = Antialias::AUTO;
	int frames = 1;
	double fps = 30, camAzimuth = -60, camElevation = 29, camOrbit = 0, viewRate = 0.25; // viewRate: probe cycles/s shown in frames
	float width = 723, height = 216; // twice the module's display
	std::string panel, rack;         // panel: light or dark for a mockup; rack: Rack's source or SDK, for its component art
	float panelScale = 6;            // mockup pixels per millimetre
	int accent = 0;                  // of Theme::ACCENTS
	std::string out = "orbit";
};

static Options parse(int argc, char** argv) {
	Options o;
	const auto val = [](auto& ref) { return [&ref](const std::string& s) { std::istringstream(s) >> ref; }; };
	const auto vec = [](auto*... refs) {
		return [=](const std::string& s) {
			std::istringstream in(s);
			((in >> *refs, in.ignore(1)), ...);
		};
	};
	const std::map<std::string, std::function<void(const std::string&)>> set{
		{"freq", val(o.freq)}, {"seconds", val(o.seconds)}, {"rate", val(o.rate)},
		{"field", val(o.field)}, {"seed", val(o.seed)}, {"octaves", val(o.octaves)},
		{"center", vec(&o.center.x, &o.center.y, &o.center.z)}, {"radii", vec(&o.major, &o.minor)},
		{"orient", vec(&o.tilt, &o.azimuth)}, {"drift", val(o.drift)}, {"rotate", val(o.rotate)}, {"axis", val(o.axis)},
		{"mode", val(o.mode)}, {"outputs", val(o.outputs)}, {"spread", val(o.spread)}, {"angle", val(o.angle)},
		{"remove-dc", val(o.removeDc)}, {"normalize", val(o.normalize)}, {"antialias", val(o.antialias)},
		{"frames", val(o.frames)}, {"fps", val(o.fps)}, {"cam", vec(&o.camAzimuth, &o.camElevation)},
		{"cam-orbit", val(o.camOrbit)}, {"size", vec(&o.width, &o.height)}, {"view-rate", val(o.viewRate)}, {"out", val(o.out)},
		{"panel", val(o.panel)}, {"rack", val(o.rack)}, {"panel-scale", val(o.panelScale)}, {"accent", val(o.accent)},
	};
	for (int i = 1; i < argc; i += 2) {
		const std::string key = argv[i];
		const auto it = key.rfind("--", 0) == 0 ? set.find(key.substr(2)) : set.end();
		if (it == set.end() || i + 1 == argc)
			throw std::invalid_argument("bad option " + key);
		it->second(argv[i + 1]);
	}
	if (!o.panel.empty() && o.panel != "light" && o.panel != "dark")
		throw std::invalid_argument("panel must be light or dark");
	if (o.antialias < 0 || o.antialias >= Antialias::LEVELS)
		throw std::invalid_argument("antialias must be 0 to " + std::to_string(Antialias::LEVELS - 1));
	if (o.outputs < 1 || o.outputs > OrbitVoice::MAX_OUTPUTS)
		throw std::invalid_argument("outputs must be 1 to " + std::to_string(OrbitVoice::MAX_OUTPUTS));
	return o;
}

// The index of `name` among `names`, ignoring case.
template <size_t N>
static int lookup(const char* const (&names)[N], std::string name, const char* what) {
	std::transform(name.begin(), name.end(), name.begin(), ::toupper);
	const auto it = std::find(std::begin(names), std::end(names), name);
	if (it == std::end(names))
		throw std::invalid_argument(std::string("unknown ") + what + " " + name);
	return int(it - std::begin(names));
}

static StockField makeField(const Options& o) {
	StockField f;
	f.kind = StockField::Kind(lookup(StockField::NAMES, o.field, "field"));
	f.seed = o.seed;
	f.octaves = o.octaves;
	return f;
}

static OrbitShape shapeAt(const Options& o, double t) {
	const double deg = PI / 180;
	const Ring ring = orbitRing(o.center + Vec3{0, 0, o.drift * t}, o.major, o.minor, o.tilt * deg, o.azimuth * deg,
	                            Ring::Axis(lookup(Ring::AXIS_NAMES, o.axis, "axis")), o.rotate * t);
	return {ring, OrbitShape::Mode(lookup(OrbitShape::MODE_NAMES, o.mode, "mode")), o.spread, o.angle / 360, o.outputs};
}

static OrbitState stateAt(const Options& o, const StockField& field, double t) {
	OrbitState s{shapeAt(o, t), o.tilt * PI / 180, o.azimuth * PI / 180, Ring::Axis(lookup(Ring::AXIS_NAMES, o.axis, "axis")),
	             field, o.freq, o.viewRate * t, 1, o.freq >= 20};
	s.values = {float(o.freq),  float(o.center.x), float(o.center.y), float(o.center.z), float(o.major), float(o.minor),
	            float(o.drift), float(o.rotate),   float(o.tilt),     float(o.azimuth),  float(o.spread * 100), float(o.angle)};
	return s;
}

// Where each of Orbit's knobs points for a state, from 0 to 1.
static float turn(const OrbitState& s, int param) {
	using namespace orbit;
	if (param < MODS) {
		const Mod& m = MOD[param];
		const double raw = param == FREQ_PARAM ? std::log2(s.values[param] / (s.audio ? C4 : C4 / 256)) : s.values[param] / m.scale;
		return std::clamp(float((raw - m.min) / (m.max - m.min)), 0.f, 1.f);
	}
	switch (param) {
		case FIELD_PARAM: return s.field.kind / 2.f;
		case OCTAVES_PARAM: return (s.field.octaves - 1) / 3.f;
		case MODE_PARAM: return s.shape.mode / float(OrbitShape::MODES - 1);
		case AXIS_PARAM: return s.axis / float(Ring::AXES - 1);
		case RANGE_PARAM: return s.audio;
		default: return 0.5f;
	}
}

static void writeWav(const std::string& path, const std::vector<float>& frames, int channels, int rate) {
	std::ofstream f(path, std::ios::binary);
	const auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
	const auto u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
	const uint32_t bytes = uint32_t(frames.size() * sizeof(float));
	f.write("RIFF", 4), u32(36 + bytes), f.write("WAVEfmt ", 8), u32(16);
	u16(3), u16(uint16_t(channels)), u32(uint32_t(rate)), u32(uint32_t(rate * channels * 4)), u16(uint16_t(channels * 4)), u16(32);
	f.write("data", 4), u32(bytes), f.write(reinterpret_cast<const char*>(frames.data()), bytes);
}

static void writeScope(const std::string& path, const std::vector<float>& frames, int channels, int length, const Theme& theme) {
	if (length < 2)
		return;
	SvgPainter svg(640, 240, theme.glass);
	const Rgba colors[] = {theme.ring, theme.field, theme.probe, theme.guide * 3};
	svg.line({0, 120}, {640, 120}, theme.guide, 1);
	for (int x : {160, 320, 480})
		svg.line({float(x), 10}, {float(x), 230}, theme.guide * 0.5f, 1);
	std::vector<Vec2f> pts;
	for (int c = 0; c < channels; ++c) {
		pts.clear();
		for (int i = 0; i < length; ++i)
			pts.push_back({640.f * i / (length - 1), 120 - 100 * frames[size_t(i) * channels + c]});
		svg.stroke(pts, colors[c % 4], 1.5f, false);
	}
	std::ofstream(path) << svg.str();
}

int main(int argc, char** argv) {
	try {
		const Options o = parse(argc, argv);
		const StockField field = makeField(o);
		const int channels = o.outputs;
		const size_t length = size_t(o.seconds * o.rate);

		std::vector<float> frames(length * channels);
		OrbitVoice voice;
		voice.setAntialias(Antialias::Level(o.antialias));
		for (size_t i = 0; i < length; ++i)
			voice.process(shapeAt(o, i / o.rate), field, o.freq, 1 / o.rate, o.removeDc, o.normalize, &frames[i * channels]);
		writeWav(o.out + ".wav", frames, channels, int(o.rate));

		for (int c = 0; c < channels; ++c) {
			double lo = 1e9, hi = -1e9, sum = 0, sq = 0;
			for (size_t i = c; i < frames.size(); i += channels)
				lo = std::min<double>(lo, frames[i]), hi = std::max<double>(hi, frames[i]), sum += frames[i], sq += double(frames[i]) * frames[i];
			std::printf("out %d: min %+.3f max %+.3f dc %+.3f rms %.3f\n", c, lo, hi, sum / length, std::sqrt(sq / length));
		}

		const Panel panel = orbit::panel();
		OrbitScene scene;
		scene.taps = panel.taps;
		scene.view.theme = Theme::accent(o.accent);
		scene.camElevation = o.camElevation * PI / 180;
		writeScope(o.out + "-scope.svg", frames, channels, int(std::min<double>(length, 2 * o.rate / o.freq)), scene.view.theme);
		for (int k = 0; k < o.frames; ++k) {
			const double t = k / o.fps;
			scene.camAzimuth = (o.camAzimuth + o.camOrbit * t) * PI / 180;
			const OrbitState state = stateAt(o, field, t);
			SvgPainter svg(o.width, o.height, scene.view.theme.glass);
			scene.paint(svg, state);
			char name[32];
			std::snprintf(name, sizeof name, o.frames > 1 ? "-view-%04d.svg" : "-view.svg", k);
			std::ofstream(o.out + name) << svg.str();
		}

		if (!o.panel.empty()) {
			// The screen at the size Rack gives it, 75 pixels to the inch, scaled onto the glass.
			const Panel::Rect& g = panel.screen;
			const float w = g.max.x - g.min.x, h = g.max.y - g.min.y, px = 75 / 25.4f;
			const OrbitState state = stateAt(o, field, 0);
			SvgPainter svg(w * px, h * px, scene.view.theme.glass);
			svg.radius = 2.5f;
			scene.paint(svg, state);
			std::ostringstream place;
			place << "x='" << g.min.x << "' y='" << g.min.y << "' width='" << w << "' height='" << h << "'";
			std::ofstream(o.out + "-panel.svg") << panelSvg(panel, o.panel == "dark", o.panelScale, svg.str(place.str()), o.rack,
			                                                [&](int param) { return turn(state, param); });
		}
	} catch (const std::exception& e) {
		std::cerr << "render: " << e.what() << '\n';
		return 1;
	}
}
