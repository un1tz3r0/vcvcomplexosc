#pragma once
#include "plugin.hpp"
#include "core/Painter.hpp"

inline NVGcolor nvg(cxo::Rgba c, float alpha = 1) { return nvgRGBAf(c.r, c.g, c.b, c.a * alpha); }
