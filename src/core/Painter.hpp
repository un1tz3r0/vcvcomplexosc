#pragma once
#include <iterator>
#include <string>
#include <vector>
#include "Vec.hpp"

namespace cxo {

struct Rgba {
	float r = 1, g = 1, b = 1, a = 1;
	Rgba operator*(float k) const { return {r, g, b, a * k}; }
};

constexpr Rgba hex(unsigned rgb, float a = 1) { return {(rgb >> 16 & 255) / 255.f, (rgb >> 8 & 255) / 255.f, (rgb & 255) / 255.f, a}; }

// The OLED palette: an accent phosphor for fields, a contrasting one for rings, near-white for probes.
struct Theme {
	Rgba glass{0.010f, 0.013f, 0.016f, 1.f};
	Rgba field{0.32f, 0.86f, 1.00f, 0.85f};
	Rgba ring{1.00f, 0.66f, 0.22f, 1.00f};
	Rgba probe{1.00f, 0.96f, 0.88f, 1.00f};
	Rgba guide{0.76f, 0.82f, 0.88f, 0.30f};

	static constexpr const char* ACCENTS[] = {"Cyan", "Green", "Amber", "Violet", "White"};
	static Theme accent(int i) {
		constexpr Rgba fields[] = {{0.32f, 0.86f, 1.00f, 0.85f}, {0.40f, 1.00f, 0.55f, 0.85f}, {1.00f, 0.70f, 0.26f, 0.85f},
		                           {0.72f, 0.58f, 1.00f, 0.85f}, {0.86f, 0.90f, 0.96f, 0.80f}};
		constexpr Rgba rings[] = {{1.00f, 0.66f, 0.22f, 1.f}, {1.00f, 0.72f, 0.28f, 1.f}, {0.50f, 0.88f, 1.00f, 1.f},
		                          {1.00f, 0.70f, 0.40f, 1.f}, {1.00f, 0.66f, 0.22f, 1.f}};
		Theme t;
		i = i < 0 || i >= int(std::size(ACCENTS)) ? 0 : i;
		t.field = fields[i];
		t.ring = rings[i];
		return t;
	}
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
