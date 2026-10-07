#pragma once
#include <string>
#include <vector>
#include "Vec.hpp"

namespace cxo {

struct Rgba {
	float r = 1, g = 1, b = 1, a = 1;
	Rgba operator*(float k) const { return {r, g, b, a * k}; }
};

// The OLED palette: one cool phosphor for fields, one warm for rings, near-white for probes.
struct Theme {
	Rgba glass{0.010f, 0.013f, 0.016f, 1.f};
	Rgba field{0.32f, 0.86f, 1.00f, 0.85f};
	Rgba ring{1.00f, 0.66f, 0.22f, 1.00f};
	Rgba probe{1.00f, 0.96f, 0.88f, 1.00f};
	Rgba guide{0.76f, 0.82f, 0.88f, 0.30f};
};

// Emissive strokes are painted as a few concentric passes, widest and faintest first, blended additively.
struct GlowPass {
	float width, alpha;
};
constexpr GlowPass GLOW[] = {{5.f, 0.06f}, {2.5f, 0.16f}, {1.f, 1.f}};

// The drawing vocabulary of the OLED views, implemented by NanoVG in the plugin and by SVG in the tools.
struct Painter {
	float width, height;

	Painter(float width, float height) : width(width), height(height) {}
	virtual ~Painter() = default;

	virtual void fill(const std::vector<Vec2f>& pts, Rgba color) = 0; // opaque occluder
	virtual void stroke(const std::vector<Vec2f>& pts, Rgba color, float lineWidth, bool closed = false) = 0;
	virtual void dot(Vec2f center, float radius, Rgba color) = 0;
	// `align` is -1, 0 or 1 to anchor the text's left edge, center or right edge at `pos`; it is centered vertically.
	virtual void label(Vec2f pos, const std::string& text, float size, Rgba color, int align = 0) = 0;

	void line(Vec2f a, Vec2f b, Rgba color, float lineWidth) { stroke({a, b}, color, lineWidth); }
	void rect(float x, float y, float w, float h, Rgba color) { fill({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, color); }
};

} // namespace cxo
