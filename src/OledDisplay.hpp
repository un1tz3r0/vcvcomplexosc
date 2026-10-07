#pragma once
#include "plugin.hpp"
#include "core/Painter.hpp"

// Black glass whose scene is painted on Rack's light layer, so it stays lit when the room is dimmed.
struct OledDisplay : widget::Widget {
	cxo::Theme theme;
	float cornerRadius = 2.5f;

	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
	virtual void paintScene(cxo::Painter& painter) = 0;
};
