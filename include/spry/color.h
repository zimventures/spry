#pragma once
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

} // namespace spry
