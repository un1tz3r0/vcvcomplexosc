#include "OledDisplay.hpp"

namespace {

NVGcolor nvg(cxo::Rgba c, float alpha = 1) { return nvgRGBAf(c.r, c.g, c.b, c.a * alpha); }

struct NvgPainter : cxo::Painter {
	NVGcontext* vg;
	std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));

	NvgPainter(NVGcontext* vg, float width, float height) : Painter(width, height), vg(vg) {}

	void path(const std::vector<cxo::Vec2f>& pts, bool closed) {
		nvgBeginPath(vg);
		nvgMoveTo(vg, pts[0].x, pts[0].y);
		for (size_t i = 1; i < pts.size(); ++i)
			nvgLineTo(vg, pts[i].x, pts[i].y);
		if (closed)
			nvgClosePath(vg);
	}

	void fill(const std::vector<cxo::Vec2f>& pts, cxo::Rgba c) override {
		if (pts.size() < 3)
			return;
		nvgGlobalCompositeOperation(vg, NVG_SOURCE_OVER);
		path(pts, true);
		nvgFillColor(vg, nvg(c));
		nvgFill(vg);
	}
	void stroke(const std::vector<cxo::Vec2f>& pts, cxo::Rgba c, float width, bool closed) override {
		if (pts.size() < 2)
			return;
		nvgGlobalCompositeBlendFunc(vg, NVG_SRC_ALPHA, NVG_ONE);
		path(pts, closed);
		for (const cxo::GlowPass& g : cxo::GLOW) {
			nvgStrokeWidth(vg, width * g.width);
			nvgStrokeColor(vg, nvg(c, g.alpha));
			nvgStroke(vg);
		}
	}
	void dot(cxo::Vec2f p, float radius, cxo::Rgba c) override {
		nvgGlobalCompositeBlendFunc(vg, NVG_SRC_ALPHA, NVG_ONE);
		for (const cxo::GlowPass& g : cxo::GLOW) {
			nvgBeginPath(vg);
			nvgCircle(vg, p.x, p.y, radius * g.width);
			nvgFillColor(vg, nvg(c, g.alpha));
			nvgFill(vg);
		}
	}
	void label(cxo::Vec2f p, const std::string& text, float size, cxo::Rgba c) override {
		if (!font || font->handle < 0)
			return;
		nvgGlobalCompositeBlendFunc(vg, NVG_SRC_ALPHA, NVG_ONE);
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, size);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, nvg(c));
		nvgText(vg, p.x, p.y, text.c_str(), nullptr);
	}
};

} // namespace

void OledDisplay::draw(const DrawArgs& args) {
	nvgBeginPath(args.vg);
	nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, cornerRadius);
	nvgFillColor(args.vg, nvg(theme.glass));
	nvgFill(args.vg);
	nvgFillPaint(args.vg, nvgLinearGradient(args.vg, 0, 0, 0, box.size.y * 0.4f, nvgRGBAf(1, 1, 1, 0.04f), nvgRGBAf(1, 1, 1, 0)));
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 1);
	nvgStrokeColor(args.vg, nvgRGBAf(0, 0, 0, 0.7f));
	nvgStroke(args.vg);
	Widget::draw(args);
}

void OledDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		nvgSave(args.vg);
		nvgIntersectScissor(args.vg, 0, 0, box.size.x, box.size.y);
		NvgPainter painter(args.vg, box.size.x, box.size.y);
		paintScene(painter);
		nvgRestore(args.vg);
	}
	Widget::drawLayer(args, layer);
}
