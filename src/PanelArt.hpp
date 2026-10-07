#pragma once
#include "plugin.hpp"

// Panel graphics painted at runtime from the same layout code that places the controls, so labels can never
// drift from their knobs. Follows Rack's dark-panel preference and is cached in a framebuffer.
struct PanelArt : widget::FramebufferWidget {
	struct Text {
		Vec mm;
		std::string text;
		float size;
		int align; // -1 left, 0 center, 1 right
		bool strong;
	};
	struct Card {
		math::Rect mm;
		bool sunken; // sunken cards hold outputs
	};

	std::vector<Text> texts;
	std::vector<Card> cards;
	bool dark = settings::preferDarkPanels;

	explicit PanelArt(Vec size);
	void step() override;
	void paint(NVGcontext* vg) const;

	void text(Vec mm, std::string s, float size = 6.f, int align = 0, bool strong = false) { texts.push_back({mm, std::move(s), size, align, strong}); }
	void card(Vec topLeft, Vec bottomRight, bool sunken = false) { cards.push_back({math::Rect(topLeft, bottomRight.minus(topLeft)), sunken}); }
};
