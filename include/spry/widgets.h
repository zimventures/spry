#pragma once
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

} // namespace spry
