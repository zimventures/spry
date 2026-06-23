#pragma once
#include <algorithm>
#include <memory>

#include "anim.h"
#include "renderer.h"
#include "theme.h"
#include "widget.h"

// Owns the root widget + the active theme, and drives a frame: layout -> hover
// hit-test -> update -> draw. Theme swaps are animated by interpolating tokens.
namespace spry {

class Context {
public:
    Context() {
        to_ = Theme::builtinDark();
        from_ = to_;
        displayed_ = to_;
    }

    void setRoot(std::unique_ptr<Widget> root) {
        root_ = std::move(root);
        hovered_ = nullptr;
    }
    Widget* root() const { return root_.get(); }

    // Swap with no transition (e.g. initial theme).
    void setThemeImmediate(Theme th) {
        to_ = std::move(th);
        from_ = to_;
        displayed_ = to_;
        trans_ = 1.0f;
    }
    // Swap with an animated crossfade.
    void setTheme(Theme th) {
        from_ = displayed_;
        to_ = std::move(th);
        trans_ = 0.0f;
    }
    const Theme& displayedTheme() const { return displayed_; }

    void frame(Renderer& r, float dt, float mouseX, float mouseY) {
        if (!root_) return;
        trans_ = std::min(1.0f, trans_ + dt * 4.0f);
        displayed_ = trans_ >= 1.0f ? to_ : lerp(from_, to_, easeOutCubic(trans_));

        int w = 0, h = 0;
        r.outputSize(w, h);
        root_->arrange(r, Rect{0, 0, (float)w, (float)h});

        Widget* hv = root_->hitTest(mouseX, mouseY);
        if (hv != hovered_) {
            if (hovered_) hovered_->hovered = false;
            hovered_ = hv;
            if (hovered_) hovered_->hovered = true;
        }

        root_->update(dt);
        root_->draw(r, displayed_);
    }

private:
    std::unique_ptr<Widget> root_;
    Widget* hovered_ = nullptr;
    Theme from_, to_, displayed_;
    float trans_ = 1.0f;
};

} // namespace spry
