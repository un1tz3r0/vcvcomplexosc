#pragma once
#include <cstdio>
#include <string>
#include <vector>
#include "Painter.hpp"

namespace cxo {

// A module's front panel in millimetres. The plugin builds its widgets and art from it and tools/render draws a
// mockup of it, so the two can't disagree. Knobs are wired to the screen, which shows each value where its wire arrives.
struct Panel {
	enum Kind { KNOB, SELECTOR, TRIMPOT, INPUT, OUTPUT };
	enum Style { LABEL, TITLE, BRAND, BOXED };
	struct Control {
		Kind kind;
		int id; // param, input or output id, by kind
		Vec2f at;
		Rgba color;
	};
	struct Text {
		Vec2f at;
		std::string text;
		Style style;
	};
	struct Rect {
		Vec2f min, max;
	};
	struct Wire {
		Vec2f from, to;
	};
	// A param's value shown on the screen's bottom edge, or its right edge if `side`, `at` that fraction along it.
	struct Tap {
		int param;
		float at;
		bool side;
		Rgba color;
	};

	static constexpr float BEZEL = 1.f, KNOB_SIZE = 10.5f, SELECTOR_SIZE = 6.5f, JACK_LABEL = 6.3f;
	static constexpr float FONT_SIZE[] = {1.9f, 4.4f, 2.f, 1.9f}; // by Style

	float width, height;
	Rect screen; // the glass, inside a bezel BEZEL wide
	std::vector<Control> controls;
	std::vector<Text> texts;
	std::vector<Wire> wires;
	std::vector<Rect> boxes; // dark boxes holding outputs
	std::vector<Tap> taps;

	Panel(float width, float height, Rect screen) : width(width), height(height), screen(screen) {}

	void text(Vec2f at, std::string s, Style style = LABEL) { texts.push_back({at, std::move(s), style}); }

	// A knob wired up to the screen's bottom edge, labelled where the wire meets it.
	void knobUnder(int id, Vec2f at, const char* label, Rgba color) {
		const float top = at.y - KNOB_SIZE / 2, y = labelAbove(at);
		controls.push_back({KNOB, id, at, color});
		text({at.x, y}, label);
		wires.push_back({{at.x, screen.max.y + BEZEL}, {at.x, y - 1.4f}});
		wires.push_back({{at.x, y + 1.4f}, {at.x, top}});
		taps.push_back({id, (at.x - screen.min.x) / (screen.max.x - screen.min.x), false, color});
	}

	// A small knob wired across to the screen's right edge, labelled over its wire.
	void selectorBeside(int id, Vec2f at, const char* label, Rgba color) {
		const float edge = screen.max.x + BEZEL, left = at.x - SELECTOR_SIZE / 2;
		controls.push_back({SELECTOR, id, at, color});
		text({0.5f * (edge + left), at.y - 1.9f}, label);
		wires.push_back({{edge, at.y}, {left, at.y}});
		taps.push_back({id, (at.y - screen.min.y) / (screen.max.y - screen.min.y), true, color});
	}

	void jack(Kind kind, int id, Vec2f at, const char* label, Style style = LABEL) {
		controls.push_back({kind, id, at, {}});
		text({at.x, at.y - JACK_LABEL}, label, style);
	}

	// The height of the label of a knob at `at`.
	static float labelAbove(Vec2f at) { return at.y - KNOB_SIZE / 2 - 2.8f; }
};

// The panel's colours for Rack's light and dark panel settings.
struct PanelColors {
	Rgba panel, ink, title, brand, wire, bezel, box, boxInk;
	Rgba text(Panel::Style s) const { return s == Panel::TITLE ? title : s == Panel::BRAND ? brand : s == Panel::BOXED ? boxInk : ink; }
};
inline constexpr PanelColors LIGHT_PANEL{hex(0xecebe8), hex(0x3a3a3c), hex(0x1f1f21), hex(0x9c9b98), hex(0xbdbbb6), hex(0x2f3032), hex(0x2d2e31), hex(0xeeeeec)};
inline constexpr PanelColors DARK_PANEL{hex(0x2b2c2e), hex(0xc9c8c4), hex(0xecebe8), hex(0x77787b), hex(0x4f5054), hex(0x141516), hex(0x1b1c1e), hex(0xd8d8d6)};

inline std::string css(Rgba c) {
	char s[8];
	std::snprintf(s, sizeof s, "#%02x%02x%02x", int(c.r * 255 + 0.5f), int(c.g * 255 + 0.5f), int(c.b * 255 + 0.5f));
	return s;
}

// A knob after vcvspeak's, a pastel cap in a dark ring lit from above, as SVG markup on a 100 × 100 viewBox.
// The pointer is the only part that turns, so the light stays put.
inline std::string knobArt(Rgba cap, bool pointer) {
	if (pointer)
		return "<line x1='50' y1='17' x2='50' y2='39' stroke='#26262a' stroke-width='7.5' stroke-linecap='round'/>";
	return "<defs><linearGradient id='ring' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='#47474c'/>"
	       "<stop offset='1' stop-color='#1a1a1d'/></linearGradient>"
	       "<radialGradient id='dome' cx='50' cy='43' r='40' gradientUnits='userSpaceOnUse'>"
	       "<stop offset='0' stop-color='#ffffff' stop-opacity='0.26'/><stop offset='0.62' stop-color='#ffffff' stop-opacity='0.1'/>"
	       "<stop offset='0.8' stop-color='#000000' stop-opacity='0'/><stop offset='1' stop-color='#000000' stop-opacity='0.16'/></radialGradient></defs>"
	       "<circle cx='50' cy='50' r='50' fill='url(#ring)'/><circle cx='50' cy='50' r='45' fill='#232326'/>"
	       "<circle cx='50' cy='50' r='37' fill='" + css(cap) + "'/><circle cx='50' cy='50' r='37' fill='url(#dome)'/>";
}

// The same as a standalone SVG document `mm` across, as Rack loads it.
inline std::string knobSvg(float mm, Rgba cap, bool pointer) {
	char head[160];
	std::snprintf(head, sizeof head, "<svg xmlns='http://www.w3.org/2000/svg' width='%gmm' height='%gmm' viewBox='0 0 100 100'>", mm, mm);
	return head + knobArt(cap, pointer) + "</svg>";
}

} // namespace cxo
