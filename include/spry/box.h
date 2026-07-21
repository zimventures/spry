#pragma once
#include <algorithm>

#include "widget.h"

/// @file box.h
/// Flex + flow layout containers.

namespace spry {

/// @addtogroup widgets
/// @{

/// Flex container (#209): arranges children along a main @ref axis (Row/Column)
/// with fixed/auto/grow sizing, spacing, padding, and cross-axis alignment.
class Box : public Widget {
public:
    Axis axis = Axis::Column;   ///< Main axis children are laid out along.
    float spacing = 0.0f;       ///< Gap between children, in logical pixels.
    Edges padding;              ///< Inset around the children.
    Align cross = Align::Stretch;  ///< Cross-axis alignment of children.

    Size measure(Renderer& r, float availW, float availH) override;
    void arrange(Renderer& r, Rect rect) override;
};

/// Flow container (#220): lays children left-to-right and wraps to a new line when
/// the next one would overflow the available width — for toolbars in a resizable
/// pane that can't always fit on one line. Reflows as the width changes. Children
/// keep their measured (or `prefW`/`prefH`) size; no grow/stretch.
class WrapBox : public Widget {
public:
    float hspace = 6.0f;  ///< Horizontal gap between items on a line.
    float vspace = 6.0f;  ///< Vertical gap between wrapped lines.
    Edges padding;        ///< Inset around the children.

    Size measure(Renderer& r, float availW, float availH) override {
        float inner = std::max(0.0f, (availW > 0 ? availW : 1e9f) - padding.l - padding.r);
        float x = 0, lineH = 0, totalH = 0;
        bool atStart = true;
        for (auto& c : children_) {
            if (!c->visible) continue;
            Size cs = c->measure(r, inner, availH);
            float cw = c->prefW >= 0 ? c->prefW : cs.w;
            float chh = c->prefH >= 0 ? c->prefH : cs.h;
            if (!atStart && x + hspace + cw > inner) {
                totalH += lineH + vspace;
                x = 0;
                lineH = 0;
                atStart = true;
            }
            if (!atStart) x += hspace;
            x += cw;
            lineH = std::max(lineH, chh);
            atStart = false;
        }
        totalH += lineH;
        float w = availW > 0 ? availW : x + padding.l + padding.r;
        return Size{w, totalH + padding.t + padding.b};
    }

    void arrange(Renderer& r, Rect rc) override {
        rect = rc;
        float startX = rc.x + padding.l;
        float inner = std::max(0.0f, rc.w - padding.l - padding.r);
        float x = startX, y = rc.y + padding.t, lineH = 0;
        bool atStart = true;
        for (auto& c : children_) {
            if (!c->visible) continue;
            Size cs = c->measure(r, inner, rc.h);
            float cw = c->prefW >= 0 ? c->prefW : cs.w;
            float chh = c->prefH >= 0 ? c->prefH : cs.h;
            if (!atStart && (x + hspace + cw) > startX + inner) {
                x = startX;
                y += lineH + vspace;
                lineH = 0;
                atStart = true;
            }
            if (!atStart) x += hspace;
            c->arrange(r, Rect{x, y, cw, chh});
            x += cw;
            lineH = std::max(lineH, chh);
            atStart = false;
        }
    }
};

/// @}

} // namespace spry
