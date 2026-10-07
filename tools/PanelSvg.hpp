#pragma once
#include <functional>
#include <sstream>
#include "core/Panel.hpp"

namespace cxo {

// A mockup of a panel as an SVG document in millimetres, `scale` pixels to the millimetre. The panel's own art and
// knobs are drawn as the plugin draws them; Rack's jacks, trimpots and screws are linked from `rackDir` (Rack's
// source or SDK), or stood in for by plain discs without it. `screen` is an element placed on the glass, and
// `turn(param)` is where each knob points, from 0 to 1.
inline std::string panelSvg(const Panel& p, bool dark, float scale, const std::string& screen, const std::string& rackDir,
                            const std::function<float(int)>& turn) {
	const PanelColors& c = dark ? DARK_PANEL : LIGHT_PANEL;
	std::ostringstream s;
	s.precision(5);
	s << "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' width='" << p.width * scale << "' height='"
	  << p.height * scale << "' viewBox='0 0 " << p.width << ' ' << p.height << "'>\n"
	  << "<rect width='100%' height='100%' fill='" << css(c.panel) << "'/>\n";

	const Vec2f b0{p.screen.min.x - Panel::BEZEL, p.screen.min.y - Panel::BEZEL}, b1{p.screen.max.x + Panel::BEZEL, p.screen.max.y + Panel::BEZEL};
	s << "<rect x='" << b0.x << "' y='" << b0.y << "' width='" << b1.x - b0.x << "' height='" << b1.y - b0.y << "' rx='1.6' fill='" << css(c.bezel) << "'/>\n"
	  << screen;
	for (const Panel::Rect& r : p.boxes)
		s << "<rect x='" << r.min.x << "' y='" << r.min.y << "' width='" << r.max.x - r.min.x << "' height='" << r.max.y - r.min.y << "' rx='1.6' fill='"
		  << css(c.box) << "'/>\n";
	for (const Panel::Wire& w : p.wires)
		s << "<line x1='" << w.from.x << "' y1='" << w.from.y << "' x2='" << w.to.x << "' y2='" << w.to.y << "' stroke='" << css(c.wire)
		  << "' stroke-width='0.25'/>\n";
	for (const Panel::Text& t : p.texts)
		s << "<text x='" << t.at.x << "' y='" << t.at.y << "' font-family='Nunito' font-weight='700' font-size='" << Panel::FONT_SIZE[t.style]
		  << "' fill='" << css(c.text(t.style)) << "' text-anchor='middle' dominant-baseline='central'>" << t.text << "</text>\n";

	const auto art = [&](const char* name, Vec2f at, float size) {
		if (rackDir.empty())
			s << "<circle cx='" << at.x << "' cy='" << at.y << "' r='" << size / 2 << "' fill='#9a9a9a'/>\n";
		else
			s << "<image href='file://" << rackDir << "/res/ComponentLibrary/" << name << ".svg' x='" << at.x - size / 2 << "' y='" << at.y - size / 2
			  << "' width='" << size << "' height='" << size << "'/>\n";
	};
	const auto knob = [&](const Panel::Control& k, float size) {
		const float degrees = (turn(k.id) - 0.5f) * 0.83f * 360;
		s << "<circle cx='" << k.at.x << "' cy='" << k.at.y + 0.1f * size << "' r='" << size / 2 << "' fill-opacity='0.15'/>\n"
		  << "<svg x='" << k.at.x - size / 2 << "' y='" << k.at.y - size / 2 << "' width='" << size << "' height='" << size << "' viewBox='0 0 100 100'>"
		  << knobArt(k.color, false) << "<g transform='rotate(" << degrees << " 50 50)'>" << knobArt(k.color, true) << "</g></svg>\n";
	};
	for (const Panel::Control& k : p.controls)
		switch (k.kind) {
			case Panel::KNOB: knob(k, Panel::KNOB_SIZE); break;
			case Panel::SELECTOR: knob(k, Panel::SELECTOR_SIZE); break;
			case Panel::TRIMPOT: art("Trimpot_bg", k.at, 6.05f), art("Trimpot", k.at, 6.05f); break;
			default: art(dark ? "PJ301M-dark" : "PJ301M", k.at, 8.03f);
		}
	for (float x : {7.62f, p.width - 7.62f})
		for (float y : {2.54f, p.height - 2.54f})
			art(dark ? "ScrewBlack" : "ScrewSilver", {x, y}, 5.08f);
	s << "</svg>\n";
	return s.str();
}

} // namespace cxo
