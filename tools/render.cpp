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
	std::string fractal = "fbm", distort = "none", shaper = "off"; // any of StockField's names, in any case
	float detail = 0, roughness = 0.5f, lacunarity = 2, warp = 0, tilt = 0, warpScale = 1, warpDrift = 0, distortion = 0, drive = 1;
	std::string cells = "smooth", metric = "euclidean", lattice = "cubic"; // smooth, value, f1, f2 or edge; then Cells' names
	float jitter = 1, smoothness = 0.25f;
	Vec3 center;
	double radius = 1, eccentricity = 0.2, pitch = 0, roll = 0;
	double drift = 0;               // center travel along Z, units per second
	double rotate = 0;              // turns per second about the axis
	std::string axis = "normal";    // any of Ring::AXIS_NAMES, in any case
	std::string mode = "phase";     // any of OrbitShape::MODE_NAMES, in any case
	double spread = 0.5;
	std::string side = "both";      // side panel: off, waves, xy or both
	bool equalXY = false;
	std::string floor = "grid";     // rows, grid or dots
	int density = 1, brightness = 2, hue = 1; // as indices into OrbitView's names
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
		{"field", val(o.field)}, {"seed", val(o.seed)},
		{"fractal", val(o.fractal)}, {"detail", val(o.detail)}, {"roughness", val(o.roughness)}, {"lacunarity", val(o.lacunarity)},
		{"warp", val(o.warp)}, {"tilt", val(o.tilt)}, {"warp-scale", val(o.warpScale)}, {"warp-drift", val(o.warpDrift)},
		{"distort", val(o.distort)}, {"distortion", val(o.distortion)}, {"shaper", val(o.shaper)}, {"drive", val(o.drive)},
		{"cells", val(o.cells)}, {"metric", val(o.metric)}, {"lattice", val(o.lattice)}, {"jitter", val(o.jitter)}, {"smoothness", val(o.smoothness)},
		{"center", vec(&o.center.x, &o.center.y, &o.center.z)}, {"radius", val(o.radius)},
		{"eccentricity", val(o.eccentricity)}, {"orient", vec(&o.pitch, &o.roll)}, {"drift", val(o.drift)}, {"rotate", val(o.rotate)}, {"axis", val(o.axis)},
		{"mode", val(o.mode)}, {"spread", val(o.spread)}, {"side", val(o.side)}, {"equal-xy", val(o.equalXY)},
		{"floor", val(o.floor)}, {"density", val(o.density)}, {"brightness", val(o.brightness)}, {"hue", val(o.hue)},
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
	if (o.density < 0 || o.density >= OrbitView::DENSITIES || o.brightness < 0 || o.brightness >= OrbitView::BRIGHTNESSES || o.hue < 0 ||
	    o.hue >= OrbitView::HUES)
		throw std::invalid_argument("density, brightness or hue out of range");
	if (o.antialias < 0 || o.antialias >= Antialias::LEVELS)
		throw std::invalid_argument("antialias must be 0 to " + std::to_string(Antialias::LEVELS - 1));
	return o;
}

// The index of `name` among `names`, ignoring case.
template <size_t N>
static int lookup(const char* const (&names)[N], std::string name, const char* what) {
	const auto upper = [](std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), ::toupper);
		return s;
	};
	const auto it = std::find_if(std::begin(names), std::end(names), [&](const char* n) { return upper(n) == upper(name); });
	if (it == std::end(names))
		throw std::invalid_argument(std::string("unknown ") + what + " " + name);
	return int(it - std::begin(names));
}

static StockField makeField(const Options& o) {
	StockField f;
	f.kind = StockField::Kind(lookup(StockField::NAMES, o.field, "field"));
	f.seed = o.seed;
	f.fractal = StockField::Fractal(lookup(StockField::FRACTAL_NAMES, o.fractal, "fractal"));
	f.detail = std::clamp(o.detail, 0.f, float(StockField::MAX_OCTAVES - 1));
	f.roughness = o.roughness, f.lacunarity = o.lacunarity;
	f.warp = o.warp, f.tilt = o.tilt, f.warpScale = o.warpScale;
	f.distort = StockField::Distort(lookup(StockField::DISTORT_NAMES, o.distort, "distortion"));
	f.distortion = o.distortion;
	f.shaper = StockField::Shaper(lookup(StockField::SHAPER_NAMES, o.shaper, "shaper"));
	f.drive = o.drive;
	static constexpr const char* FEATURES[Cells::FEATURES] = {"f1", "f2", "edge", "smooth", "value"};
	f.cells.feature = Cells::Feature(lookup(FEATURES, o.cells, "cell feature"));
	f.cells.metric = Cells::Metric(lookup(Cells::METRIC_NAMES, o.metric, "metric"));
	f.cells.lattice = Cells::Lattice(o.lattice == "bcc" ? Cells::BCC : lookup(Cells::LATTICE_NAMES, o.lattice, "lattice"));
	f.cells.jitter = o.jitter, f.cells.smoothness = o.smoothness;
	return f;
}

static StockField fieldAt(const Options& o, StockField f, double t) {
	f.warpDrift = float(o.warpDrift * t);
	return f;
}

static OrbitShape shapeAt(const Options& o, double t) {
	const double deg = PI / 180;
	const Ring ring = orbitRing(o.center + Vec3{0, 0, o.drift * t}, o.radius, o.eccentricity, o.pitch * deg, o.roll * deg,
	                            Ring::Axis(lookup(Ring::AXIS_NAMES, o.axis, "axis")), o.rotate * t);
	return {ring, OrbitShape::Mode(lookup(OrbitShape::MODE_NAMES, o.mode, "mode")), o.spread};
}

static OrbitState stateAt(const Options& o, const StockField& field, double t) {
	OrbitState s{shapeAt(o, t), o.pitch * PI / 180, o.roll * PI / 180, Ring::Axis(lookup(Ring::AXIS_NAMES, o.axis, "axis")),
	             field, o.freq, o.viewRate * t, {0, 0, o.drift}, 1, o.freq >= 20};
	s.values = {float(o.freq),  float(o.center.x), float(o.center.y), float(o.center.z), float(o.radius), float(o.eccentricity),
	            float(o.drift), float(o.rotate),   float(o.pitch),    float(o.roll),     float(o.spread * 100)};
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
		case FIELD_PARAM: return s.field.kind / float(StockField::KINDS - 1);
		case OCTAVES_PARAM: return (s.field.octaves() - 1) / 3.f;
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
		const int channels = OrbitShape::OUTPUTS;
		const size_t length = size_t(o.seconds * o.rate);

		std::vector<float> frames(length * channels);
		OrbitVoice voice;
		voice.setAntialias(Antialias::Level(o.antialias));
		for (size_t i = 0; i < length; ++i)
			voice.process(shapeAt(o, i / o.rate), fieldAt(o, field, i / o.rate), o.freq, 1 / o.rate, o.removeDc, o.normalize, &frames[i * channels]);
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
		std::string side = o.side;
		std::transform(side.begin(), side.end(), side.begin(), ::tolower);
		const std::map<std::string, OrbitScene::Side> sides{{"off", OrbitScene::OFF}, {"waves", OrbitScene::WAVES}, {"xy", OrbitScene::XY}, {"both", OrbitScene::BOTH}};
		if (!sides.count(side))
			throw std::invalid_argument("side must be off, waves, xy or both");
		scene.side = sides.at(side);
		scene.equalXY = o.equalXY;
		scene.view.floor = OrbitView::Floor(lookup(OrbitView::FLOOR_NAMES, o.floor, "floor"));
		scene.view.density = o.density;
		scene.view.brightness = o.brightness;
		scene.view.hue = o.hue;
		scene.camElevation = o.camElevation * PI / 180;
		writeScope(o.out + "-scope.svg", frames, channels, int(std::min<double>(length, 2 * o.rate / o.freq)), scene.view.theme);
		for (int k = 0; k < o.frames; ++k) {
			const double t = k / o.fps;
			scene.camAzimuth = (o.camAzimuth + o.camOrbit * t) * PI / 180;
			const OrbitState state = stateAt(o, fieldAt(o, field, t), t);
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
