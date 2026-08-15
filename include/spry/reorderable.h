#pragma once

#include "box.h"
#include "theme.h"
#include "theme_tokens.h"
#include "widget.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace spry {

/// A vertical list of widget **rows** that can be reordered by dragging, with zebra
/// striping and a hover highlight.
///
/// `ListView` and `Table` cover lists whose rows are *drawn* (strings, cells). This
/// covers the other kind: rows built from child widgets — a label, a badge, a
/// spacer, a couple of buttons — which those can't lay out.
///
/// The list never reorders its own children. `onReorder` reports the move and the
/// caller rebuilds from its model, which stays the single source of truth. The move
/// is reported on **drop**, not while dragging: committing mid-drag would rebuild
/// the scene under the pointer and destroy the widget holding the press capture.
///
/// ```cpp
/// auto* list = parent->emplace<ReorderableList>();
/// list->onReorder = [&](int from, int to) { model.move(from, to); rebuild(); };
/// for (auto& item : model) {
///     auto* row = list->emplaceRow<Box>();
///     row->axis = Axis::Row;
///     row->emplace<Label>(item.name);        // inert: presses fall through to the list
///     row->emplace<Button>("Edit", ...);     // focusable: keeps its own clicks
/// }
/// ```
class ReorderableList : public Box {
public:
    bool striped = true;          ///< Tint alternate rows.
    bool highlightHovered = true; ///< Light the row under the pointer.
    /// Adopted rows have their inert descendants marked non-interactive (see
    /// `addRow`). Clear it to manage `Widget::interactive` yourself.
    bool markRowsInert = true;

    /// Fired on drop with the row's old and new index. Never fired for a drag that
    /// ends where it started.
    std::function<void(int fromIndex, int toIndex)> onReorder;

    ReorderableList() {
        axis = Axis::Column;
        spacing = 6.0f;
    }

    /// Adopt `row` as the next row.
    ///
    /// With `markRowsInert` set (the default), the row and its descendants are made
    /// non-interactive **except** those that are `focusable` or carry a `tooltip` —
    /// buttons and the like keep their own input. That is what lets a press on a
    /// row's label start a drag rather than dying on the label. A child that needs
    /// hover or presses for another reason can set `interactive = true` back
    /// afterwards.
    Widget* addRow(std::unique_ptr<Widget> row) {
        Widget* raw = add(std::move(row));
        if (markRowsInert && raw) markInert(*raw);
        return raw;
    }

    /// Construct a `T` row in place (forwarding `args`) and return it.
    template <class T, class... Args>
    T* emplaceRow(Args&&... args) {
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        addRow(std::move(owned));
        return raw;
    }

    /// Index of the row being dragged, or -1. Useful for a caller that wants to
    /// suppress its own hover effects mid-drag.
    int draggedRow() const { return dragging_ ? pressRow_ : -1; }

    void arrange(Renderer& r, Rect rc) override {
        Box::arrange(r, rc);

        // Remembered before the dragged row is displaced: the drop target is decided
        // against where the rows actually sit, which also keeps it correct when rows
        // differ in height.
        slots_.clear();
        for (auto& c : children_) {
            if (c->visible) slots_.push_back(c->rect);
        }

        if (dragging_ && pressRow_ >= 0 && pressRow_ < rowCount()) {
            // The whole row moves — background, labels and buttons — so it tracks the
            // pointer instead of leaving an abstract marker to do the explaining.
            shiftSubtree(*visibleRow(pressRow_), dragOffset_);
        }
    }

    void paint(Renderer& r, const Theme& th) override {
        // draw() paints this widget before its children, so these washes sit behind
        // the rows rather than over them.
        const Color text = th.color(tokens::Text, {226, 229, 242});
        const float radius = th.metric(tokens::Radius, 12.0f) * 0.5f;
        auto wash = [&](const Rect& rr, int alpha) {
            const Color c{text.r, text.g, text.b, static_cast<uint8_t>(alpha)};
            r.fillRoundedRect(rr.x + rr.w * 0.5f, rr.y + rr.h * 0.5f, rr.w, rr.h, radius, c, c);
        };

        for (int i = 0; i < rowCount(); ++i) {
            Widget* row = visibleRow(i);
            if (!row) continue;
            // Tinted with the *text* colour rather than a surface token: a list
            // usually sits on a surface itself, and surface-on-surface comes out
            // invisible. Text contrasts with its background by definition, in a light
            // theme as well as a dark one.
            if (dragging_ && i == pressRow_)
                wash(row->rect, 44);
            else if (highlightHovered && !dragging_ && row->hoveredWithin())
                wash(row->rect, 26);
            else if (striped && i % 2 == 1)
                wash(row->rect, 10);
        }

        if (dragging_ && target_ != pressRow_ && target_ >= 0 && target_ < (int)slots_.size()) {
            // Marks the slot the drop lands in. Drawn from the remembered slot rather
            // than the live rect, which has moved with the drag.
            const Color accent = th.color(tokens::Accent, {96, 126, 205});
            const Rect& slot = slots_[static_cast<std::size_t>(target_)];
            const float edge = target_ > pressRow_ ? slot.y + slot.h + spacing * 0.5f : slot.y - spacing * 0.5f;
            r.fillRoundedRect(slot.x + slot.w * 0.5f, edge, slot.w, 3.0f, 1.5f, accent, accent);
        }
    }

    bool onMouseDown(float, float y, int button, bool, bool) override {
        if (button != 0) return false; // left only; 0 is left, see Widget::onMouseDown
        pressRow_ = rowAt(y);
        pressY_ = y;
        target_ = pressRow_;
        dragging_ = false;
        dragOffset_ = 0.0f;
        return pressRow_ >= 0;
    }

    void onMouseDrag(float, float y) override {
        if (pressRow_ < 0) return;
        if (!dragging_) {
            // Armed past a few pixels so a click on a row doesn't jitter into a drag.
            if (std::fabs(y - pressY_) < 4.0f) return;
            dragging_ = true;
        }
        dragOffset_ = y - pressY_;
        target_ = rowAt(y, /*clampToEnds=*/true);
    }

    bool onMouseUp(float, float, int) override {
        const bool moved = dragging_ && target_ >= 0 && target_ != pressRow_;
        const int from = pressRow_, to = target_;
        dragging_ = false;
        dragOffset_ = 0.0f;
        pressRow_ = -1;
        target_ = -1;
        if (moved && onReorder) onReorder(from, to);
        return true;
    }

private:
    int rowCount() const { return static_cast<int>(slots_.size()); }

    Widget* visibleRow(int index) const {
        int i = 0;
        for (auto& c : children_) {
            if (!c->visible) continue;
            if (i++ == index) return c.get();
        }
        return nullptr;
    }

    /// Row under `y`, using the remembered (un-dragged) slots. -1 when outside the
    /// list, unless `clampToEnds` — a drag above the first row targets it, and one
    /// below the last targets that.
    int rowAt(float y, bool clampToEnds = false) const {
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            if (y >= slots_[i].y && y <= slots_[i].y + slots_[i].h) return static_cast<int>(i);
        }
        if (!clampToEnds || slots_.empty()) return -1;
        return y < slots_.front().y ? 0 : static_cast<int>(slots_.size()) - 1;
    }

    static void shiftSubtree(Widget& w, float dy) {
        w.rect.y += dy;
        for (auto& c : w.children()) shiftSubtree(*c, dy);
    }

    static void markInert(Widget& w) {
        // Anything that asked for input keeps it; everything else steps aside so the
        // list can own the press.
        if (!w.focusable && w.tooltip.empty()) w.interactive = false;
        for (auto& c : w.children()) markInert(*c);
    }

    std::vector<Rect> slots_; ///< Row rects as laid out, before any drag offset.
    int pressRow_ = -1;
    int target_ = -1;
    float pressY_ = 0.0f;
    float dragOffset_ = 0.0f;
    bool dragging_ = false;
};

} // namespace spry
