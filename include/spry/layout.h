#pragma once

/// @file layout.h
/// Small value types shared by every module: sizes, rectangles, edge insets, and
/// the layout enums.

namespace spry {

/// @addtogroup widgets
/// @{

/// A width/height pair, in logical pixels.
struct Size {
    float w = 0;  ///< Width.
    float h = 0;  ///< Height.
};

/// An axis-aligned rectangle (position + size), in logical pixels.
struct Rect {
    float x = 0;  ///< Left edge.
    float y = 0;  ///< Top edge.
    float w = 0;  ///< Width.
    float h = 0;  ///< Height.
    /// True if the point (`px`,`py`) lies within this rectangle (edges inclusive).
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

/// Edge insets (margins / padding). Convenience ctors: `Edges(all)`,
/// `Edges(horiz, vert)`, `Edges(l, t, r, b)`.
struct Edges {
    float l = 0;  ///< Left inset.
    float t = 0;  ///< Top inset.
    float r = 0;  ///< Right inset.
    float b = 0;  ///< Bottom inset.
    Edges() = default;
    Edges(float a) : l(a), t(a), r(a), b(a) {}            ///< Same inset on all four sides.
    Edges(float h, float v) : l(h), t(v), r(h), b(v) {}   ///< Horizontal + vertical insets.
    Edges(float l_, float t_, float r_, float b_) : l(l_), t(t_), r(r_), b(b_) {}  ///< Per-side.
};

/// Main axis of a `Box`: lay children out in a row or a column.
enum class Axis { Row, Column };

/// Cross-axis alignment of children within a `Box`.
enum class Align { Start, Center, End, Stretch };

/// @}

} // namespace spry
