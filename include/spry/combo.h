#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "context.h"
#include "overlay.h"
#include "widget.h"

/// @file combo.h
/// A dropdown selector widget.

namespace spry {

/// @addtogroup widgets
/// @{

/// A dropdown selector (#220): a closed box showing the current option, which opens
/// a `Menu` of choices on click (or Enter/Space). Arrow keys cycle the selection in
/// place without opening. Built on the `Menu` overlay, which it spawns via
/// `Context::current()` during the click.
class Combo : public Widget {
public:
    std::vector<std::string> options;      ///< The choices.
    int selected = 0;                      ///< Index into @ref options; -1 = none.
    float scale = 1.4f;                    ///< Text scale.
    std::string placeholder = "Select..."; ///< Shown when nothing is selected.
    /// Fired when the selection changes (via the menu or arrow keys), with the index and value.
    std::function<void(int index, const std::string& value)> onChange;

    Combo() { focusable = true; }
    /// Construct with options and an initial selection.
    explicit Combo(std::vector<std::string> opts, int sel = 0) : options(std::move(opts)), selected(sel) {
        focusable = true;
    }

    /// Select by value; no-op (returns `false`) if absent. Does not fire @ref onChange.
    bool setValue(const std::string& v) {
        for (int i = 0; i < (int)options.size(); ++i)
            if (options[i] == v) {
                selected = i;
                return true;
            }
        return false;
    }
    /// The currently-selected value (empty string if none).
    const std::string& value() const {
        static const std::string kEmpty;
        return (selected >= 0 && selected < (int)options.size()) ? options[selected] : kEmpty;
    }

    Size measure(Renderer& r, float, float) override {
        float widest = r.measureText(scale, placeholder.c_str()).w;
        for (const auto& o : options)
            widest = std::max(widest, r.measureText(scale, o.c_str()).w);
        // text + chevron column + horizontal padding.
        return Size{widest + kPad * 2.0f + kChevron, textLineH(scale) + 14.0f};
    }

    void onClick() override { openMenu(); }

    bool onKey(Key key, bool, bool, bool) override {
        if (options.empty()) return false;
        if (key == Key::Down || key == Key::Up) {
            int n = (int)options.size();
            int next = selected < 0 ? 0 : (selected + (key == Key::Down ? 1 : -1));
            next = next < 0 ? 0 : (next >= n ? n - 1 : next);
            commit(next);
            return true;
        }
        return false;
    }

    Role accessibleRole() const override { return Role::Button; }
    std::string accessibleLabel() const override {
        const std::string& v = value();
        return v.empty() ? placeholder : v; // never expose an unlabeled control
    }

    void paint(Renderer& r, const Theme& th) override {
        float rad = th.metric(tokens::Radius, 8.0f);
        float cx = rect.x + rect.w * 0.5f, cy = rect.y + rect.h * 0.5f;
        Color acc = th.color(tokens::Accent, {96, 126, 205});
        Color surface = th.color(tokens::Surface, {40, 43, 62});
        if (focused) {
            Color ring{acc.r, acc.g, acc.b, 150};
            r.fillRoundedRect(cx, cy, rect.w, rect.h, rad, ring, ring);
            r.fillRoundedRect(cx, cy, rect.w - 3, rect.h - 3, rad - 1, surface, surface);
        } else {
            Color border = hovered ? Color{acc.r, acc.g, acc.b, 90} : th.color(tokens::TextDim, {140, 144, 160});
            r.fillRoundedRect(cx, cy, rect.w, rect.h, rad, border, border);
            r.fillRoundedRect(cx, cy, rect.w - 2, rect.h - 2, rad - 1, surface, surface);
        }

        float ty = rect.y + (rect.h - textLineH(scale)) * 0.5f;
        bool hasSel = selected >= 0 && selected < (int)options.size();
        const std::string& label = hasSel ? options[selected] : placeholder;
        Color textCol = hasSel ? th.color(tokens::Text, {226, 229, 242}) : th.color(tokens::TextDim, {140, 144, 160});
        r.pushClip(Rect{rect.x + 2, rect.y + 2, rect.w - kChevron - kPad, rect.h - 4});
        r.text(rect.x + kPad, ty, scale, textCol, label.c_str());
        r.popClip();

        // Down chevron, drawn as a thin filled triangle on the right.
        float chCx = rect.x + rect.w - kChevron * 0.5f - kPad * 0.5f;
        float chCy = cy + 1.0f, s = 4.0f;
        Color chev = th.color(tokens::TextDim, {150, 154, 170});
        std::vector<Vertex> v{{chCx - s, chCy - s * 0.6f, chev}, {chCx + s, chCy - s * 0.6f, chev},
                              {chCx, chCy + s * 0.6f, chev}};
        std::vector<int> idx{0, 1, 2};
        r.fillMesh(v, idx);
    }

private:
    static constexpr float kPad = 10.0f;
    static constexpr float kChevron = 18.0f;

    void commit(int idx) {
        if (idx == selected) return;
        selected = idx;
        if (onChange) onChange(selected, value());
    }

    void openMenu() {
        Context* ctx = Context::current();
        if (!ctx || options.empty()) return;
        auto menu = std::make_unique<Menu>();
        menu->anchorX = rect.x;
        menu->anchorY = rect.y + rect.h + 4.0f;
        menu->minWidth = rect.w;
        for (int i = 0; i < (int)options.size(); ++i)
            menu->addItem(options[i], [this, i] { commit(i); });
        ctx->addOverlay(std::move(menu));
    }
};

/// @}

} // namespace spry
