// Headless orbit renderer: audio to a multichannel float WAV, a two-cycle scope and the 3D view to SVG.
// Usage: render [--option value]...   (see Options for names and defaults; angles in degrees)
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include "core/Field.hpp"
#include "core/OrbitView.hpp"
#include "core/Phasor.hpp"
#include "SvgPainter.hpp"

using namespace cxo;

struct Options {
	double freq = 110, seconds = 2, rate = 48000;
	std::string field = "simplex3"; // simplex2 | simplex3 | gyroid
	int64_t seed = 0;
	int octaves = 1;
	Vec3 center;
	double major = 1, minor = 1, spin = 0, tilt = 0, azimuth = 0;
	double drift = 0;               // center travel along Z, units per second
	std::string mode = "phase";     // phase: outputs spread around one ring; pinch: rings meeting at pinchPhase
	int outputs = 1;
	double spread = 0.5, pinchPhase = 0;
	int frames = 1;
	double fps = 30, camAzimuth = -60, camElevation = 35, camOrbit = 0, viewRate = 0.25; // viewRate: probe cycles/s shown in frames
	std::string out = "orbit";
};

static Options parse(int argc, char** argv) {
	Options o;
	const auto val = [](auto& ref) { return [&ref](const std::string& s) { std::istringstream(s) >> ref; }; };
	const auto vec = [](std::initializer_list<double*> refs) {
		return [refs = std::vector<double*>(refs)](const std::string& s) {
			std::istringstream in(s);
			for (double* r : refs) {
				in >> *r;
				in.ignore(1);
			}
		};
	};
	const std::map<std::string, std::function<void(const std::string&)>> set{
		{"freq", val(o.freq)}, {"seconds", val(o.seconds)}, {"rate", val(o.rate)},
		{"field", val(o.field)}, {"seed", val(o.seed)}, {"octaves", val(o.octaves)},
		{"center", vec({&o.center.x, &o.center.y, &o.center.z})}, {"radii", vec({&o.major, &o.minor})},
		{"orient", vec({&o.spin, &o.tilt, &o.azimuth})}, {"drift", val(o.drift)},
		{"mode", val(o.mode)}, {"outputs", val(o.outputs)}, {"spread", val(o.spread)}, {"pinch-phase", val(o.pinchPhase)},
		{"frames", val(o.frames)}, {"fps", val(o.fps)}, {"cam", vec({&o.camAzimuth, &o.camElevation})},
		{"cam-orbit", val(o.camOrbit)}, {"view-rate", val(o.viewRate)}, {"out", val(o.out)},
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
	if (o.outputs < 1)
		throw std::invalid_argument("need at least one output");
	return o;
}

static std::function<float(Vec3)> makeField(const Options& o) {
	if (o.field == "gyroid")
		return GyroidField{};
	if (o.field != "simplex2" && o.field != "simplex3")
		throw std::invalid_argument("unknown field " + o.field);
	NoiseField f;
	f.dim = o.field == "simplex2" ? NoiseField::Dim::Two : NoiseField::Dim::Three;
	f.seed = o.seed;
	f.octaves = o.octaves;
	return f;
}

static std::vector<Trace> traces(const Options& o, double t, double phase) {
	const double deg = PI / 180;
	const Ring ring{o.center + Vec3{0, 0, o.drift * t}, Ring::orient(o.spin * deg, o.tilt * deg, o.azimuth * deg), o.major, o.minor};
	if (o.mode == "phase") {
		Trace trace{ring, {}};
		for (int k = 0; k < o.outputs; ++k)
			trace.phases.push_back(phase + double(k) / o.outputs);
		return {trace};
	}
	std::vector<Trace> family;
	for (int k = 0; k < o.outputs; ++k) {
		const double scale = o.outputs > 1 ? 1 + o.spread * (double(k) / (o.outputs - 1) - 0.5) : 1;
		family.push_back({ring.pinched(o.pinchPhase, scale), {phase}});
	}
	return family;
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
		const auto field = makeField(o);
		int channels = 0;
		for (const Trace& trace : traces(o, 0, 0))
			channels += int(trace.phases.size());
		const size_t length = size_t(o.seconds * o.rate);

		std::vector<float> frames;
		frames.reserve(length * channels);
		Phasor phasor;
		for (size_t i = 0; i < length; ++i) {
			const double t = i / o.rate, phase = phasor.phase;
			for (const Trace& trace : traces(o, t, phase))
				for (double p : trace.phases)
					frames.push_back(field(trace.ring.at(p)));
			phasor.step(o.freq, 1 / o.rate);
		}
		writeWav(o.out + ".wav", frames, channels, int(o.rate));

		for (int c = 0; c < channels; ++c) {
			double lo = 1e9, hi = -1e9, sum = 0, sq = 0;
			for (size_t i = c; i < frames.size(); i += channels)
				lo = std::min<double>(lo, frames[i]), hi = std::max<double>(hi, frames[i]), sum += frames[i], sq += double(frames[i]) * frames[i];
			std::printf("out %d: min %+.3f max %+.3f dc %+.3f rms %.3f\n", c, lo, hi, sum / length, std::sqrt(sq / length));
		}

		const OrbitView view;
		writeScope(o.out + "-scope.svg", frames, channels, int(std::min<double>(length, 2 * o.rate / o.freq)), view.theme);
		for (int k = 0; k < o.frames; ++k) {
			const double t = k / o.fps;
			const std::vector<Trace> shown = traces(o, t, o.viewRate * t);
			const double deg = PI / 180;
			const Mat3 plane = Ring::orient(0, o.tilt * deg, o.azimuth * deg); // ride with the ring's plane so the relief stays legible
			const Camera cam = Camera::orbit(shown.front().ring.center, 4.2 * reach(shown), (o.camAzimuth + o.camOrbit * t) * deg,
			                                 o.camElevation * deg, 480, 360, plane);
			SvgPainter svg(cam.width, cam.height, view.theme.glass);
			view(svg, cam, field, shown);
			char name[32];
			std::snprintf(name, sizeof name, o.frames > 1 ? "-view-%04d.svg" : "-view.svg", k);
			std::ofstream(o.out + name) << svg.str();
		}
	} catch (const std::exception& e) {
		std::cerr << "render: " << e.what() << '\n';
		return 1;
	}
}
