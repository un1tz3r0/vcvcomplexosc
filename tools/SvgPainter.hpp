#pragma once
#include <cstdio>
#include <sstream>
#include "core/Painter.hpp"

namespace cxo {

// Paints into an SVG document; additive glow uses mix-blend-mode: plus-lighter.
class SvgPainter : public Painter {
	std::ostringstream body;

	static std::string rgb(Rgba c) {
		char s[32];
		std::snprintf(s, sizeof s, "rgb(%d,%d,%d)", int(c.r * 255 + 0.5f), int(c.g * 255 + 0.5f), int(c.b * 255 + 0.5f));
		return s;
	}
	void points(const std::vector<Vec2f>& pts) {
		for (const Vec2f& p : pts)
			body << p.x << ',' << p.y << ' ';
	}

public:
	Rgba background;

	SvgPainter(float width, float height, Rgba background) : Painter(width, height), background(background) {
		body.precision(4);
	}

	void fill(const std::vector<Vec2f>& pts, Rgba c) override {
		body << "<polygon fill='" << rgb(c) << "' fill-opacity='" << c.a << "' points='";
		points(pts);
		body << "'/>\n";
	}
	void stroke(const std::vector<Vec2f>& pts, Rgba c, float w, bool closed) override {
		for (const GlowPass& g : GLOW) {
			body << '<' << (closed ? "polygon" : "polyline") << " fill='none' stroke='" << rgb(c) << "' stroke-opacity='" << c.a * g.alpha
			     << "' stroke-width='" << w * g.width << "' stroke-linejoin='round' stroke-linecap='round' style='mix-blend-mode:plus-lighter' points='";
			points(pts);
			body << "'/>\n";
		}
	}
	void dot(Vec2f p, float r, Rgba c) override {
		for (const GlowPass& g : GLOW)
			body << "<circle cx='" << p.x << "' cy='" << p.y << "' r='" << r * g.width << "' fill='" << rgb(c) << "' fill-opacity='" << c.a * g.alpha
			     << "' style='mix-blend-mode:plus-lighter'/>\n";
	}
	void label(Vec2f p, const std::string& text, float size, Rgba c, int align) override {
		body << "<text x='" << p.x << "' y='" << p.y << "' font-size='" << size << "' fill='" << rgb(c) << "' fill-opacity='" << c.a
		     << "' font-family='Share Tech Mono, monospace' text-anchor='" << (align < 0 ? "start" : align > 0 ? "end" : "middle")
		     << "' dominant-baseline='central' style='mix-blend-mode:plus-lighter'>" << text << "</text>\n";
	}

	std::string str() const {
		std::ostringstream doc;
		doc << "<svg xmlns='http://www.w3.org/2000/svg' width='" << width << "' height='" << height << "' viewBox='0 0 " << width << ' ' << height
		    << "'>\n<clipPath id='frame'><rect width='100%' height='100%'/></clipPath>\n<g clip-path='url(#frame)'>\n<rect width='100%' height='100%' fill='"
		    << rgb(background) << "'/>\n" << body.str() << "</g>\n</svg>\n";
		return doc.str();
	}
};

} // namespace cxo
