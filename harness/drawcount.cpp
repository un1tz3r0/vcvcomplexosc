// Counts Painter calls (each a NanoVG path; strokes/dots/segments cost 3 glow passes) and times one Orbit display frame
// at the module's on-screen size, for each floor setting. Build: g++ -O2 -std=c++17 -I<repo>/src drawcount.cpp
#include <chrono>
#include <cstdio>
#include "core/OrbitScene.hpp"
using namespace cxo;

struct Counter : Painter {
	int fills = 0, strokes = 0, verts = 0;
	using Painter::Painter;
	void fill(const std::vector<Vec2f>& p, Rgba) override { ++fills, verts += int(p.size()); }
	void stroke(const std::vector<Vec2f>& p, Rgba, float, bool) override { ++strokes, verts += int(p.size()); }
	void segments(const std::vector<Vec2f>& p, Rgba, float) override { strokes += !p.empty(), verts += int(p.size()); }
	void dot(Vec2f, float, Rgba) override { ++strokes, ++verts; }
	void dots(const std::vector<Vec2f>& p, float, Rgba) override { strokes += !p.empty(), verts += int(p.size()); }
	void label(Vec2f, const std::string&, float, Rgba, int) override {}
};

int main() {
	const Panel panel = orbit::panel();
	for (int floor = 0; floor < OrbitView::FLOORS; ++floor)
		for (int density = 0; density < OrbitView::DENSITIES; ++density)
			for (int hue : {0, 1}) {
				OrbitScene scene;
				scene.taps = panel.taps;
				scene.view.floor = OrbitView::Floor(floor), scene.view.density = density, scene.view.hue = hue;
				OrbitState s;
				s.field.octaves = 2;
				Counter c(362, 108);
				const auto t0 = std::chrono::steady_clock::now();
				constexpr int N = 50;
				for (int i = 0; i < N; ++i) {
					c = Counter(362, 108);
					scene.paint(c, s);
				}
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / N;
				std::printf("%-5s %-6s hue %d: %3d fills %4d strokes %5d vertices, %.2f ms CPU\n", OrbitView::FLOOR_NAMES[floor],
				            OrbitView::DENSITY_NAMES[density], hue, c.fills, c.strokes, c.verts, ms);
			}
}
