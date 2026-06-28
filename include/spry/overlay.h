#pragma once
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "anim.h"
#include "box.h"
#include "widget.h"
#include "widgets.h"

// Overlays (#214): transient layers drawn above the root — modals, menus,
// tooltips, toasts. Context owns a stack of them, draws them on top of the root,
// routes input to the topmost interactive one, and prunes them once closed. An
// overlay fills the window so it can position content anywhere and (when modal)
// catch outside clicks. Each owns a presence spring that fades + offsets its
// surface in and out; subclasses place and paint that surface.
namespace spry {

class Overlay : public Widget {
public:
    enum class Phase { Opening, Open, Closing, Closed };

    bool interactive = true;          // false => pointer-transparent (tooltips/toasts)
    bool dimBackground = false;       // draw a scrim behind the content (modal)
    bool dismissOnOutsideClick = true;
    bool dismissOnEscape = true;
    float autoClose = 0.0f;           // seconds visible before self-closing (0 = never)
    std::function<void()> onClosed;   // fired once, when fully closed
    int stackIndex = 0;               // slot among stacked() overlays, assigned by Context

    Overlay() = default;

    // Stacked overlays (toasts) tile in a corner instead of overlapping; Context
    // assigns each a stackIndex per frame and the subclass offsets by it.
    virtual bool stacked() const { return false; }

    // Start the close animation; Context removes the overlay once closed().
    void requestClose() {
        if (phase_ == Phase::Closing || phase_ == Phase::Closed) return;
        phase_ = Phase::Closing;
    }
    bool closed() const { return phase_ == Phase::Closed; }
    float presence() const { return appear_.value < 0 ? 0 : (appear_.value > 1 ? 1 : appear_.value); }

    Widget* setContent(std::unique_ptr<Widget> c) {
        content_ = c.get();
        return add(std::move(c));
    }
    Widget* content() const { return content_; }
    const Rect& contentRect() const { return contentRect_; }

    // Fill the window; the subclass positions content within it.
    void arrange(Renderer& r, Rect rc) override {
        rect = rc;
        if (!content_) return;
        Size cs = content_->measure(r, rc.w, rc.h);
        contentRect_ = placeContent(rc, cs);
        content_->arrange(r, contentRect_);
    }

    void update(float dt) override {
        appear_.target = (phase_ == Phase::Closing || phase_ == Phase::Closed) ? 0.0f : 1.0f;
        appear_.step(dt);
        if (phase_ == Phase::Opening && appear_.value > 0.99f) phase_ = Phase::Open;
        if (autoClose > 0.0f && phase_ == Phase::Open) {
            autoClose -= dt;
            if (autoClose <= 0.0f) requestClose();
        }
        if (phase_ == Phase::Closing && appear_.value < 0.02f) {
            phase_ = Phase::Closed;
            if (onClosed) onClosed();
        }
        Widget::update(dt);
    }

    void draw(Renderer& r, const Theme& th) override {
        if (!visible) return;
        float a = presence();
        if (dimBackground) {
            Color scrim = th.color("scrim", {8, 9, 14, 170});
            scrim.a = (uint8_t)(scrim.a * a);
            r.fillRect(rect.x, rect.y, rect.w, rect.h, scrim);
        }
        r.pushOpacity(a); // fade the surface + content together
        paintSurface(r, th);
        for (auto& c : children_) c->draw(r, th);
        r.popOpacity();
    }

protected:
    virtual Rect placeContent(Rect full, Size content) = 0;
    virtual void paintSurface(Renderer& /*r*/, const Theme& /*th*/) {}

    // Clamp a content rect so it stays within the window.
    static Rect clampToWindow(Rect full, Rect c) {
        if (c.x + c.w > full.x + full.w) c.x = full.x + full.w - c.w;
        if (c.y + c.h > full.y + full.h) c.y = full.y + full.h - c.h;
        if (c.x < full.x) c.x = full.x;
        if (c.y < full.y) c.y = full.y;
        return c;
    }

    Spring appear_{};
    Phase phase_ = Phase::Opening;
    Widget* content_ = nullptr;
    Rect contentRect_{};
};

// A centred modal: scrim behind, panel rises + fades in. The caller supplies the
// content (typically a padded Box with a title, body, and buttons).
class Modal : public Overlay {
public:
    Modal() {
        dimBackground = true;
        dismissOnOutsideClick = true;
    }

protected:
    Rect placeContent(Rect full, Size cs) override {
        float rise = (1.0f - presence()) * 24.0f; // slide up as it appears
        return Rect{full.x + (full.w - cs.w) * 0.5f, full.y + (full.h - cs.h) * 0.5f + rise, cs.w, cs.h};
    }
    void paintSurface(Renderer& r, const Theme& th) override {
        Rect c = contentRect_;
        float rad = th.metric("radius", 12.0f);
        r.fillRoundedRect(c.x + c.w * 0.5f, c.y + c.h * 0.5f, c.w, c.h, rad,
                          th.color("surface", {40, 43, 62}), th.color("surfaceAlt", {32, 34, 48}));
    }
};

// A row inside a Menu: hover-highlights, click runs its action and closes the menu.
class MenuItem : public Widget {
public:
    std::string label;
    std::function<void()> chosen; // action + close, wired by Menu
    // Optional leading icon, drawn centered at (cx, cy) in the given colour. The
    // consumer supplies the glyph so the toolkit stays icon-agnostic.
    std::function<void(Renderer&, float cx, float cy, Color)> icon;
    float scale = 1.4f;

    explicit MenuItem(std::string l) : label(std::move(l)) { focusable = true; }
    Size measure(Renderer& r, float, float) override {
        float iconW = icon ? 24.0f : 0.0f;
        return Size{r.measureText(scale, label.c_str()).w + 28.0f + iconW, textLineH(scale) + 12.0f};
    }
    void onClick() override {
        if (chosen) chosen();
    }
    void paint(Renderer& r, const Theme& th) override {
        if (hovered || focused) {
            Color acc = th.color("accent", {96, 126, 205});
            r.fillRoundedRect(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, rect.w - 6, rect.h - 2, 6.0f,
                              Color{acc.r, acc.g, acc.b, 60}, Color{acc.r, acc.g, acc.b, 60});
        }
        Color textC = th.color("text", {226, 229, 242});
        float labelX = rect.x + 12.0f;
        if (icon) {
            icon(r, rect.x + 16.0f, rect.y + rect.h * 0.5f, textC);
            labelX = rect.x + 34.0f;
        }
        r.text(labelX, rect.y + (rect.h - textLineH(scale)) * 0.5f, scale, textC, label.c_str());
    }
};

// A dropdown / context menu anchored at a point. Items are a vertical list; a
// click outside (or Escape) dismisses it.
class Menu : public Overlay {
public:
    float anchorX = 0.0f, anchorY = 0.0f;
    float minWidth = 0.0f; // floor for the menu width (e.g. match a combo's box)

    Menu() {
        dimBackground = false;
        dismissOnOutsideClick = true;
        auto list = std::make_unique<Box>();
        list_ = list.get();
        list_->axis = Axis::Column;
        list_->padding = Edges(6);
        list_->spacing = 2;
        setContent(std::move(list));
    }
    void addItem(std::string label, std::function<void()> action,
                 std::function<void(Renderer&, float, float, Color)> icon = nullptr) {
        auto* it = list_->emplace<MenuItem>(std::move(label));
        it->icon = std::move(icon);
        it->chosen = [this, action = std::move(action)] {
            if (action) action();
            requestClose();
        };
    }

protected:
    Rect placeContent(Rect full, Size cs) override {
        float rise = (1.0f - presence()) * 8.0f;
        float w = cs.w < minWidth ? minWidth : cs.w;
        if (w > full.w) w = full.w; // never wider than the window (clampToWindow only repositions)
        return clampToWindow(full, Rect{anchorX, anchorY + rise, w, cs.h});
    }
    void paintSurface(Renderer& r, const Theme& th) override {
        Rect c = contentRect_;
        float rad = th.metric("radius", 10.0f);
        // Soft shadow then the panel.
        r.fillRoundedRect(c.x + c.w * 0.5f, c.y + c.h * 0.5f + 3, c.w + 6, c.h + 6, rad + 2,
                          Color{0, 0, 0, 60}, Color{0, 0, 0, 60});
        r.fillRoundedRect(c.x + c.w * 0.5f, c.y + c.h * 0.5f, c.w, c.h, rad,
                          th.color("surface", {46, 49, 68}), th.color("surfaceAlt", {38, 40, 56}));
    }

private:
    Box* list_ = nullptr;
};

// A small label anchored near a point, fading in. Non-interactive; auto-closes.
class Tooltip : public Overlay {
public:
    float anchorX = 0.0f, anchorY = 0.0f;

    explicit Tooltip(std::string text, float lifetime = 2.5f) {
        interactive = false;
        dismissOnOutsideClick = false;
        dismissOnEscape = false;
        autoClose = lifetime;
        auto box = std::make_unique<Box>();
        box->padding = Edges(10, 6);
        auto* lbl = box->emplace<Label>(std::move(text), 1.3f);
        lbl->role = "text";
        setContent(std::move(box));
    }

protected:
    Rect placeContent(Rect full, Size cs) override {
        // Prefer above the anchor; the fade does the rest.
        float fade = (1.0f - presence()) * 4.0f;
        return clampToWindow(full, Rect{anchorX, anchorY - cs.h - 6.0f - fade, cs.w, cs.h});
    }
    void paintSurface(Renderer& r, const Theme& th) override {
        Rect c = contentRect_;
        r.fillRoundedRect(c.x + c.w * 0.5f, c.y + c.h * 0.5f, c.w, c.h, 6.0f,
                          th.color("surfaceAlt", {30, 32, 46}), th.color("surface", {24, 26, 38}));
    }
};

// A transient notification: slides up from the bottom-centre, lingers, fades out.
class Toast : public Overlay {
public:
    explicit Toast(std::string text, float lifetime = 3.0f) {
        interactive = false;
        dismissOnOutsideClick = false;
        dismissOnEscape = false;
        autoClose = lifetime;
        auto box = std::make_unique<Box>();
        box->padding = Edges(16, 11);
        box->emplace<Label>(std::move(text), 1.4f);
        setContent(std::move(box));
    }

    bool stacked() const override { return true; }
    void update(float dt) override {
        slot_.target = (float)stackIndex * kSlotH; // ease toward this toast's slot
        slot_.step(dt);
        Overlay::update(dt);
    }

protected:
    Rect placeContent(Rect full, Size cs) override {
        float slide = (1.0f - presence()) * 30.0f;        // rise from below as it appears
        float y = full.y + full.h - cs.h - 40.0f - slot_.value + slide; // stack upward
        return Rect{full.x + (full.w - cs.w) * 0.5f, y, cs.w, cs.h};
    }
    void paintSurface(Renderer& r, const Theme& th) override {
        Rect c = contentRect_;
        Color acc = th.color("accent", {96, 126, 205});
        r.fillRoundedRect(c.x + c.w * 0.5f, c.y + c.h * 0.5f + 3, c.w + 4, c.h + 4, 10.0f, Color{0, 0, 0, 70},
                          Color{0, 0, 0, 70});
        r.fillRoundedRect(c.x + c.w * 0.5f, c.y + c.h * 0.5f, c.w, c.h, 10.0f, lerp(th.color("surface", {46, 49, 68}), acc, 0.25f),
                          th.color("surface", {40, 43, 62}));
    }

private:
    static constexpr float kSlotH = 52.0f; // vertical spacing between stacked toasts
    Spring slot_;                          // eases toward stackIndex * kSlotH
};

} // namespace spry
