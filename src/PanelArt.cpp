#include <map>
#include "PanelArt.hpp"
#include "Nvg.hpp"

namespace {

struct Canvas : widget::Widget {
	const PanelArt* art;
	void draw(const DrawArgs& args) override { art->paint(args.vg); }
};

Vec mm(cxo::Vec2f v) { return mm2px(Vec(v.x, v.y)); }

std::shared_ptr<window::Svg> svgFrom(const std::string& text) {
	static std::map<std::string, std::shared_ptr<window::Svg>> cache;
	std::shared_ptr<window::Svg>& svg = cache[text];
	if (!svg) {
		svg = std::make_shared<window::Svg>();
		svg->loadString(text);
	}
	return svg;
}

// The panels' coloured knob, drawn by cxo::knobSvg.
struct CapKnob : RoundKnob {
	void paint(float size, cxo::Rgba cap) {
		setSvg(svgFrom(cxo::knobSvg(size, cap, true)));
		bg->setSvg(svgFrom(cxo::knobSvg(size, cap, false)));
	}
};

} // namespace

PanelArt::PanelArt(cxo::Panel panel) : panel(std::move(panel)) {
	box.size = mm({this->panel.width, this->panel.height});
	auto* canvas = new Canvas;
	canvas->art = this;
	canvas->box.size = box.size;
	addChild(canvas);
}

void PanelArt::step() {
	if (dark != settings::preferDarkPanels) {
		dark = settings::preferDarkPanels;
		setDirty();
	}
	FramebufferWidget::step();
}

void PanelArt::paint(NVGcontext* vg) const {
	const cxo::PanelColors& c = dark ? cxo::DARK_PANEL : cxo::LIGHT_PANEL;
	const auto rect = [&](cxo::Vec2f from, cxo::Vec2f to, float radius, cxo::Rgba fill) {
		const Vec a = mm(from), b = mm(to);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, a.x, a.y, b.x - a.x, b.y - a.y, mm2px(radius));
		nvgFillColor(vg, nvg(fill));
		nvgFill(vg);
	};
	rect({0, 0}, {panel.width, panel.height}, 0, c.panel);
	const float bezel = cxo::Panel::BEZEL;
	rect({panel.screen.min.x - bezel, panel.screen.min.y - bezel}, {panel.screen.max.x + bezel, panel.screen.max.y + bezel}, 1.6f, c.bezel);
	for (const cxo::Panel::Rect& r : panel.boxes)
		rect(r.min, r.max, 1.6f, c.box);

	nvgStrokeColor(vg, nvg(c.wire));
	nvgStrokeWidth(vg, mm2px(0.25f));
	for (const cxo::Panel::Wire& w : panel.wires) {
		const Vec a = mm(w.from), b = mm(w.to);
		nvgBeginPath(vg);
		nvgMoveTo(vg, a.x, a.y);
		nvgLineTo(vg, b.x, b.y);
		nvgStroke(vg);
	}

	std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
	if (!font || font->handle < 0)
		return;
	nvgFontFaceId(vg, font->handle);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	for (const cxo::Panel::Text& t : panel.texts) {
		const Vec at = mm(t.at);
		nvgFontSize(vg, mm2px(cxo::Panel::FONT_SIZE[t.style]));
		nvgFillColor(vg, nvg(c.text(t.style)));
		nvgText(vg, at.x, at.y, t.text.c_str(), nullptr);
	}
}

void addPanel(ModuleWidget* widget, const cxo::Panel& panel) {
	Module* module = widget->module;
	widget->addChild(new PanelArt(panel));
	for (float x : {RACK_GRID_WIDTH, widget->box.size.x - 2 * RACK_GRID_WIDTH})
		for (float y : {0.f, RACK_GRID_HEIGHT - RACK_GRID_WIDTH})
			widget->addChild(createWidget<ThemedScrew>(Vec(x, y)));

	for (const cxo::Panel::Control& c : panel.controls) {
		const Vec at = mm(c.at);
		switch (c.kind) {
			case cxo::Panel::KNOB:
			case cxo::Panel::SELECTOR: {
				auto* knob = createParam<CapKnob>(Vec(), module, c.id);
				knob->paint(c.kind == cxo::Panel::KNOB ? cxo::Panel::KNOB_SIZE : cxo::Panel::SELECTOR_SIZE, c.color);
				knob->box.pos = at.minus(knob->box.size.div(2));
				widget->addParam(knob);
				break;
			}
			case cxo::Panel::TRIMPOT: widget->addParam(createParamCentered<Trimpot>(at, module, c.id)); break;
			case cxo::Panel::INPUT: widget->addInput(createInputCentered<ThemedPJ301MPort>(at, module, c.id)); break;
			case cxo::Panel::OUTPUT: widget->addOutput(createOutputCentered<ThemedPJ301MPort>(at, module, c.id)); break;
		}
	}
}
