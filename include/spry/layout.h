#pragma once

namespace spry {

struct Size {
    float w = 0, h = 0;
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

// Margins / padding. Convenience ctors: Edges(all), Edges(horiz, vert), Edges(l,t,r,b).
struct Edges {
    float l = 0, t = 0, r = 0, b = 0;
    Edges() = default;
    Edges(float a) : l(a), t(a), r(a), b(a) {}
    Edges(float h, float v) : l(h), t(v), r(h), b(v) {}
    Edges(float l_, float t_, float r_, float b_) : l(l_), t(t_), r(r_), b(b_) {}
};

enum class Axis { Row, Column };
enum class Align { Start, Center, End, Stretch };

} // namespace spry
