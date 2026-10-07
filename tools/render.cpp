// Headless orbit renderer: audio to a multichannel float WAV, a two-cycle scope and the 3D view to SVG.
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
#include "SvgPainter.hpp"

using namespace cxo;

struct Options {
	double freq = 110, seconds = 2, rate = 48000;
	std::string field = "simplex3"; // any of StockField::NAMES, in any case
	int64_t seed = 0;
	int octaves = 1;
	Vec3 center;
	double major = 1, minor = 1, spin = 0, tilt = 0, azimuth = 0;
	double drift = 0;               // center travel along Z, units per second
	std::string mode = "phase";     // phase or pinch, see OrbitShape
	int outputs = 1;
	double spread = 0.5, angle = 0; // angle in degrees
	bool removeDc = true, normalize = true;
	int frames = 1;
	double fps = 30, camAzimuth = -60, camElevation = 29, camOrbit = 0, viewRate = 0.25; // viewRate: probe cycles/s shown in frames
	float width = 584, height = 266; // twice the module's display
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
		{"orient", vec(&o.spin, &o.tilt, &o.azimuth)}, {"drift", val(o.drift)},
		{"mode", val(o.mode)}, {"outputs", val(o.outputs)}, {"spread", val(o.spread)}, {"angle", val(o.angle)},
		{"remove-dc", val(o.removeDc)}, {"normalize", val(o.normalize)},
		{"frames", val(o.frames)}, {"fps", val(o.fps)}, {"cam", vec(&o.camAzimuth, &o.camElevation)},
		{"cam-orbit", val(o.camOrbit)}, {"size", vec(&o.width, &o.height)}, {"view-rate", val(o.viewRate)}, {"out", val(o.out)},
	};
	for (int i = 1; i < argc; i += 2) {
		const std::string key = argv[i];
		const auto it = key.rfind("--", 0) == 0 ? set.find(key.substr(2)) : set.end();
		if (it == set.end() || i + 1 == argc)
			throw std::invalid_argument("bad option " + key);
		it->second(argv[i + 1]);
	}
	if (o.mode != "phase" && o.mode != "pinch")
		throw std::invalid_argument("unknown mode " + o.mode);
	if (o.outputs < 1 || o.outputs > OrbitVoice::MAX_OUTPUTS)
		throw std::invalid_argument("outputs must be 1 to " + std::to_string(OrbitVoice::MAX_OUTPUTS));
	return o;
}

static StockField makeField(const Options& o) {
	StockField f;
	std::string name = o.field;
	std::transform(name.begin(), name.end(), name.begin(), ::toupper);
	const auto it = std::find(std::begin(StockField::NAMES), std::end(StockField::NAMES), name);
	if (it == std::end(StockField::NAMES))
		throw std::invalid_argument("unknown field " + o.field);
	f.kind = StockField::Kind(it - std::begin(StockField::NAMES));
	f.seed = o.seed;
	f.octaves = o.octaves;
	return f;
}

static OrbitShape shapeAt(const Options& o, double t) {
	const double deg = PI / 180;
	const Ring ring{o.center + Vec3{0, 0, o.drift * t}, Ring::orient(o.spin * deg, o.tilt * deg, o.azimuth * deg), o.major, o.minor};
	return {ring, o.mode == "pinch", o.spread, o.angle / 360, o.outputs};
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
		for (size_t i = 0; i < length; ++i)
			voice.process(shapeAt(o, i / o.rate), field, o.freq, 1 / o.rate, o.removeDc, o.normalize, &frames[i * channels]);
		writeWav(o.out + ".wav", frames, channels, int(o.rate));

		for (int c = 0; c < channels; ++c) {
			double lo = 1e9, hi = -1e9, sum = 0, sq = 0;
			for (size_t i = c; i < frames.size(); i += channels)
				lo = std::min<double>(lo, frames[i]), hi = std::max<double>(hi, frames[i]), sum += frames[i], sq += double(frames[i]) * frames[i];
			std::printf("out %d: min %+.3f max %+.3f dc %+.3f rms %.3f\n", c, lo, hi, sum / length, std::sqrt(sq / length));
		}

		OrbitScene scene;
		scene.camElevation = o.camElevation * PI / 180;
		writeScope(o.out + "-scope.svg", frames, channels, int(std::min<double>(length, 2 * o.rate / o.freq)), scene.view.theme);
		for (int k = 0; k < o.frames; ++k) {
			const double t = k / o.fps;
			scene.camAzimuth = (o.camAzimuth + o.camOrbit * t) * PI / 180;
			const OrbitState state{shapeAt(o, t), o.tilt * PI / 180, o.azimuth * PI / 180, field, o.freq, o.viewRate * t, 1};
			SvgPainter svg(o.width, o.height, scene.view.theme.glass);
			scene.paint(svg, state);
			char name[32];
			std::snprintf(name, sizeof name, o.frames > 1 ? "-view-%04d.svg" : "-view.svg", k);
			std::ofstream(o.out + name) << svg.str();
		}
	} catch (const std::exception& e) {
		std::cerr << "render: " << e.what() << '\n';
		return 1;
	}
}
