#pragma once
#include "widget.h"

// Flex container (#209): arranges children along a main axis (Row/Column) with
// fixed/auto/grow sizing, spacing, padding, and cross-axis alignment.
namespace spry {

class Box : public Widget {
public:
    Axis axis = Axis::Column;
    float spacing = 0.0f;
    Edges padding;
    Align cross = Align::Stretch;

    Size measure(Renderer& r, float availW, float availH) override;
    void arrange(Renderer& r, Rect rect) override;
};

} // namespace spry
