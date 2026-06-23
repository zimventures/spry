#pragma once
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "anim.h"
#include "theme.h"
#include "widget.h"

// A few basic leaf widgets so the layout engine has something to compose, now
// theme-aware (#211): they read colors/metrics by role from the active Theme.
// The full widget set (buttons, tables, trees, overlays) is #214.
namespace spry {

class Panel : public Widget {
public:
    float radius = -1.0f; // -1 => use theme metric "radius"
    void paint(Renderer& r, const Theme& th) override {
        float rad = radius >= 0 ? radius : th.metric("radius", 12.0f);
        r.fillRoundedRect(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, rect.w, rect.h, rad,
                          th.color("surface", {40, 43, 62}), th.color("surfaceAlt", {32, 34, 48}));
    }
};

class Label : public Widget {
public:
    std::string text;
    float scale = 1.6f;
    std::string role = "text";          // theme color role
    std::optional<Color> colorOverride; // per-widget override beats the theme

    explicit Label(std::string t, float s = 1.6f) : text(std::move(t)), scale(s) {}
    Size measure(Renderer& r, float, float) override {
        return Size{r.measureText(scale, text.c_str()).w + 2.0f, textLineH(scale)};
    }
    void paint(Renderer& r, const Theme& th) override {
        Color col = colorOverride.value_or(th.color(role, {220, 224, 235}));
        r.text(rect.x, rect.y + (rect.h - textLineH(scale)) * 0.5f, scale, col, text.c_str());
    }
    Role accessibleRole() const override { return Role::Label; }
    std::string accessibleLabel() const override { return text; }
};

// Hoverable card — brightens/lifts on hover via its own spring. All colours come
// from the theme, so it recolours live when the theme swaps.
class Card : public Widget {
public:
    std::string label;
    Spring lift;
    explicit Card(std::string l) : label(std::move(l)) {}
    void update(float dt) override {
        lift.target = hovered ? 1.0f : 0.0f;
        lift.step(dt);
        Widget::update(dt);
    }
    void paint(Renderer& r, const Theme& th) override {
        float k = lift.value < 0 ? 0 : (lift.value > 1 ? 1 : lift.value);
        Color base = th.color("surface", {46, 49, 68});
        Color acc = th.color("accent", {64, 84, 150});
        Color c = lerp(base, acc, k);
        float yo = -6.0f * k;
        float rad = th.metric("radius", 12.0f);
        r.fillRoundedRect(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f + yo, rect.w, rect.h, rad,
                          lerp(c, lerp(acc, Color{255, 255, 255, 255}, 0.2f), 0.25f * k), c);
        Color txt = lerp(th.color("text", {226, 229, 242}), th.color("accentText", {20, 20, 20}), k);
        r.text(rect.x + 14, rect.y + 14 + yo, 1.6f, txt, label.c_str());
    }
};

// A focusable button (#216): click, or Enter/Space when focused, to activate.
// Brightens on hover, darkens on press, shows a focus ring for keyboard nav.
class Button : public Widget {
public:
    std::string label;
    std::function<void()> onClickCb;
    float scale = 1.4f;
    Spring press;

    explicit Button(std::string l, std::function<void()> cb = {})
        : label(std::move(l)), onClickCb(std::move(cb)) {
        focusable = true;
    }
    Size measure(Renderer& r, float, float) override {
        return Size{r.measureText(scale, label.c_str()).w + 28.0f, textLineH(scale) + 14.0f};
    }
    void onClick() override {
        if (onClickCb) onClickCb();
    }
    Role accessibleRole() const override { return Role::Button; }
    std::string accessibleLabel() const override { return label; }
    void update(float dt) override {
        press.target = pressed ? 1.0f : 0.0f;
        press.step(dt);
        Widget::update(dt);
    }
    void paint(Renderer& r, const Theme& th) override {
        Color base = th.color("surface", {46, 49, 68});
        Color acc = th.color("accent", {96, 126, 205});
        Color c = hovered ? lerp(base, acc, 0.55f) : base;
        c = lerp(c, lerp(acc, Color{0, 0, 0, 255}, 0.35f), press.value * 0.5f);
        float rad = th.metric("radius", 10.0f);
        float yo = press.value * 1.5f;
        float cx = rect.x + rect.w * 0.5f, cy = rect.y + rect.h * 0.5f;
        if (focused) {
            Color ring{acc.r, acc.g, acc.b, 110};
            r.fillRoundedRect(cx, cy, rect.w + 8, rect.h + 8, rad + 4, ring, ring);
        }
        r.fillRoundedRect(cx, cy + yo, rect.w, rect.h, rad, c, lerp(c, Color{0, 0, 0, 255}, 0.18f));
        float tw = r.measureText(scale, label.c_str()).w;
        r.text(rect.x + (rect.w - tw) * 0.5f, rect.y + (rect.h - textLineH(scale)) * 0.5f + yo, scale,
               th.color("text", {226, 229, 242}), label.c_str());
    }
};

// A focusable checkbox (#216).
class Checkbox : public Widget {
public:
    std::string label;
    bool checked = false;
    std::function<void(bool)> onChange;
    float scale = 1.4f;

    explicit Checkbox(std::string l, bool c = false) : label(std::move(l)), checked(c) { focusable = true; }
    Size measure(Renderer& r, float, float) override {
        return Size{textLineH(scale) + 8.0f + r.measureText(scale, label.c_str()).w, textLineH(scale) + 6.0f};
    }
    void onClick() override {
        checked = !checked;
        if (onChange) onChange(checked);
    }
    Role accessibleRole() const override { return Role::Checkbox; }
    std::string accessibleLabel() const override { return label; }
    void paint(Renderer& r, const Theme& th) override {
        float box = textLineH(scale) * 0.8f;
        float bx = rect.x + box * 0.5f, by = rect.y + rect.h * 0.5f;
        Color acc = th.color("accent", {96, 126, 205});
        Color border = (hovered || focused) ? acc : th.color("textDim", {140, 144, 160});
        r.fillRoundedRect(bx, by, box, box, 5.0f, border, border);
        Color fill = checked ? acc : th.color("surface", {40, 43, 62});
        r.fillRoundedRect(bx, by, box - 3.0f, box - 3.0f, 4.0f, fill, lerp(fill, Color{0, 0, 0, 255}, 0.15f));
        if (checked)
            r.text(rect.x + box * 0.2f, by - box * 0.55f, scale * 0.95f, th.color("accentText", {235, 238, 248}),
                   "x");
        r.text(rect.x + box + 8.0f, rect.y + (rect.h - textLineH(scale)) * 0.5f, scale,
               th.color("text", {226, 229, 242}), label.c_str());
    }
};

} // namespace spry
