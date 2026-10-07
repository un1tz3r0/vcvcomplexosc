#include "PanelArt.hpp"

namespace {

struct Palette {
	NVGcolor panel, card, sunken, ink, strong, shadow;
};
const Palette DARK{nvgRGB(0x1f, 0x22, 0x25), nvgRGB(0x2a, 0x2e, 0x33), nvgRGB(0x15, 0x17, 0x1a), nvgRGB(0x9a, 0xa1, 0xa8), nvgRGB(0xe6, 0xe9, 0xec), nvgRGBA(0, 0, 0, 110)};
const Palette LIGHT{nvgRGB(0xe4, 0xe6, 0xe9), nvgRGB(0xf6, 0xf7, 0xf8), nvgRGB(0xcf, 0xd3, 0xd8), nvgRGB(0x5b, 0x62, 0x6a), nvgRGB(0x1c, 0x20, 0x24), nvgRGBA(0, 0, 0, 45)};

struct Canvas : widget::Widget {
	const PanelArt* art;
	void draw(const DrawArgs& args) override { art->paint(args.vg); }
};

} // namespace

PanelArt::PanelArt(Vec size) {
	box.size = size;
	auto* canvas = new Canvas;
	canvas->art = this;
	canvas->box.size = size;
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
	const Palette& p = dark ? DARK : LIGHT;
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, box.size.x, box.size.y);
	nvgFillColor(vg, p.panel);
	nvgFill(vg);

	const float r = mm2px(1.6f);
	for (const Card& c : cards) {
		const math::Rect b(mm2px(c.mm.pos), mm2px(c.mm.size));
		if (!c.sunken) {
			nvgBeginPath(vg);
			nvgRect(vg, b.pos.x - 8, b.pos.y - 6, b.size.x + 16, b.size.y + 18);
			nvgFillPaint(vg, nvgBoxGradient(vg, b.pos.x, b.pos.y + 1.5f, b.size.x, b.size.y, r, 5, p.shadow, nvgRGBA(0, 0, 0, 0)));
			nvgFill(vg);
		}
		nvgBeginPath(vg);
		nvgRoundedRect(vg, b.pos.x, b.pos.y, b.size.x, b.size.y, r);
		nvgFillColor(vg, c.sunken ? p.sunken : p.card);
		nvgFill(vg);
		if (c.sunken) {
			nvgFillPaint(vg, nvgLinearGradient(vg, 0, b.pos.y, 0, b.pos.y + 6, p.shadow, nvgRGBA(0, 0, 0, 0)));
			nvgFill(vg);
		}
	}

	std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
	if (!font || font->handle < 0)
		return;
	nvgFontFaceId(vg, font->handle);
	for (const Text& t : texts) {
		nvgFontSize(vg, t.size);
		nvgTextLetterSpacing(vg, t.strong ? 1.2f : 0.6f);
		nvgTextAlign(vg, (t.align < 0 ? NVG_ALIGN_LEFT : t.align > 0 ? NVG_ALIGN_RIGHT : NVG_ALIGN_CENTER) | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, t.strong ? p.strong : p.ink);
		const Vec pos = mm2px(t.mm);
		nvgText(vg, pos.x, pos.y, t.text.c_str(), nullptr);
	}
}
