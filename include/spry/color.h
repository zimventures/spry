#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spry {

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

inline Color lerp(Color a, Color b, float t) {
    return Color{(uint8_t)lerpf(a.r, b.r, t), (uint8_t)lerpf(a.g, b.g, t),
                 (uint8_t)lerpf(a.b, b.b, t), (uint8_t)lerpf(a.a, b.a, t)};
}

// HSV <-> RGB, all components in [0,1]. hsv() wraps hue and returns opaque RGB.
inline Color hsv(float h, float s, float v) {
    h -= std::floor(h); // wrap to [0,1)
    s = s < 0 ? 0 : (s > 1 ? 1 : s);
    v = v < 0 ? 0 : (v > 1 ? 1 : v);
    float r = v, g = v, b = v;
    if (s > 0.0f) {
        float hh = h * 6.0f;
        int i = (int)hh;
        float f = hh - i;
        float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
        switch (i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        default: r = v, g = p, b = q; break;
        }
    }
    return Color{(uint8_t)(r * 255 + 0.5f), (uint8_t)(g * 255 + 0.5f), (uint8_t)(b * 255 + 0.5f), 255};
}

inline void toHsv(Color c, float& h, float& s, float& v) {
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    float mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
    v = mx;
    s = mx <= 0.0f ? 0.0f : d / mx;
    if (d <= 0.0f) {
        h = 0.0f;
        return;
    }
    if (mx == r)
        h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (mx == g)
        h = (b - r) / d + 2.0f;
    else
        h = (r - g) / d + 4.0f;
    h /= 6.0f;
}

} // namespace spry
