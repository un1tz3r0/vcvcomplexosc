#pragma once
#include "plugin.hpp"
#include "core/Panel.hpp"

// A panel's background, screen bezel, wires, output boxes and labels, painted from its description so they can never
// drift from the controls. Follows Rack's dark-panel setting and is cached in a framebuffer.
struct PanelArt : widget::FramebufferWidget {
	cxo::Panel panel;
	bool dark = settings::preferDarkPanels;

	explicit PanelArt(cxo::Panel panel);
	void step() override;
	void paint(NVGcontext* vg) const;
};

// Gives a module widget its panel: the art, the screws and every control, each bound to its module's param or port.
void addPanel(ModuleWidget* widget, const cxo::Panel& panel);
