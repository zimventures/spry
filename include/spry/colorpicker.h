#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "color.h"
#include "context.h"
#include "overlay.h"
#include "widget.h"

// Colour picker (#220): a saturation/value square plus a hue strip, opened from a
// swatch button. Built for the Settings Colour type (which stores #RRGGBB) but the
// widgets work in spry::Color; hex conversion stays in the caller. Drag either
// region to edit; the field fires onChange live.
/// @file colorpicker.h
/// The color-picker widgets: the SV/hue pad, its popup, and the swatch field.

namespace spry {

/// @addtogroup widgets
/// @{

/// The interactive picker pad: a saturation/value square (at the current hue) over
/// a hue strip. Drag either region to edit; fires @ref onChange live.
class ColorPickerPad : public Widget {
public:
    std::function<void(Color)> onChange;  ///< Fired live as the color is dragged.

    ColorPickerPad() = default;
    /// Construct showing color `c`.
    explicit ColorPickerPad(Color c) { setColor(c); }

    /// Set the displayed color.
    void setColor(Color c) { toHsv(c, h_, s_, v_); }
    /// The currently-picked color.
    Color color() const { return hsv(h_, s_, v_); }

    Size measure(Renderer&, float, float) override {
        return Size{prefW >= 0 ? prefW : 220.0f, prefH >= 0 ? prefH : 174.0f};
    }

    bool onMouseDown(float x, float y, int, bool, bool) override {
        Regions g = regions();
        if (g.sv.contains(x, y)) {
            mode_ = Mode::SV;
            applySV(x, y);
            return true;
        }
        if (g.hue.contains(x, y)) {
            mode_ = Mode::Hue;
            applyHue(x);
            return true;
        }
        return false;
    }
    void onMouseDrag(float x, float y) override {
        if (mode_ == Mode::SV)
            applySV(x, y);
        else if (mode_ == Mode::Hue)
            applyHue(x);
    }
    bool onMouseUp(float, float, int) override {
        mode_ = Mode::None;
        return false;
    }

    void paint(Renderer& r, const Theme& th) override {
        Regions g = regions();
        Color white{255, 255, 255}, black{0, 0, 0}, hueC = hsv(h_, 1.0f, 1.0f);

        // SV square: bilinear quad (white -> hue across, both -> black down) is an
        // exact saturation/value field.
        std::vector<Vertex> sv{{g.sv.x, g.sv.y, white},
                               {g.sv.x + g.sv.w, g.sv.y, hueC},
                               {g.sv.x + g.sv.w, g.sv.y + g.sv.h, black},
                               {g.sv.x, g.sv.y + g.sv.h, black}};
        std::vector<int> quad{0, 1, 2, 0, 2, 3};
        r.fillMesh(sv, quad);
        // SV cursor (ring of current colour).
        float cxp = g.sv.x + s_ * g.sv.w, cyp = g.sv.y + (1.0f - v_) * g.sv.h;
        r.fillRoundedRect(cxp, cyp, 11, 11, 5.5f, white, white);
        r.fillRoundedRect(cxp, cyp, 8, 8, 4.0f, color(), color());

        // Hue strip: 6 spectrum segments.
        for (int k = 0; k < 6; ++k) {
            float x0 = g.hue.x + g.hue.w * (k / 6.0f), x1 = g.hue.x + g.hue.w * ((k + 1) / 6.0f);
            Color c0 = hsv(k / 6.0f, 1, 1), c1 = hsv((k + 1) / 6.0f, 1, 1);
            std::vector<Vertex> seg{
                {x0, g.hue.y, c0}, {x1, g.hue.y, c1}, {x1, g.hue.y + g.hue.h, c1}, {x0, g.hue.y + g.hue.h, c0}};
            r.fillMesh(seg, quad);
        }
        // Hue cursor (vertical marker).
        float hx = g.hue.x + h_ * g.hue.w;
        r.fillRect(hx - 2.0f, g.hue.y - 2.0f, 4.0f, g.hue.h + 4.0f, white);
        r.fillRect(hx - 1.0f, g.hue.y - 1.0f, 2.0f, g.hue.h + 2.0f, th.color(tokens::TextDim, {60, 62, 74}));
    }

private:
    enum class Mode { None, SV, Hue };
    struct Regions {
        Rect sv, hue;
    };
    static constexpr float kPad = 10.0f, kGap = 10.0f, kHueH = 14.0f;

    Regions regions() const {
        float w = rect.w - 2 * kPad;
        float sqH = rect.h - 2 * kPad - kGap - kHueH;
        Rect sv{rect.x + kPad, rect.y + kPad, w, sqH};
        Rect hue{rect.x + kPad, sv.y + sqH + kGap, w, kHueH};
        return {sv, hue};
    }
    static float clamp01(float t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
    void applySV(float x, float y) {
        Regions g = regions();
        if (g.sv.w <= 0.0f || g.sv.h <= 0.0f) return; // degenerate rect -> no NaNs
        s_ = clamp01((x - g.sv.x) / g.sv.w);
        v_ = 1.0f - clamp01((y - g.sv.y) / g.sv.h);
        fire();
    }
    void applyHue(float x) {
        Regions g = regions();
        if (g.hue.w <= 0.0f) return;
        h_ = clamp01((x - g.hue.x) / g.hue.w);
        fire();
    }
    void fire() {
        if (onChange) onChange(color());
    }

    float h_ = 0.0f, s_ = 0.0f, v_ = 1.0f;
    Mode mode_ = Mode::None;
};

/// The popup that wraps a @ref ColorPickerPad, anchored near a point; dismisses on
/// outside-click / Escape like a `Menu`.
class ColorPicker : public Overlay {
public:
    float anchorX = 0.0f;  ///< Anchor X, in Spry coordinates.
    float anchorY = 0.0f;  ///< Anchor Y, in Spry coordinates.

    /// Open a picker starting at `initial`, reporting live edits to `onChange`.
    ColorPicker(Color initial, std::function<void(Color)> onChange) {
        dimBackground = false;
        dismissOnOutsideClick = true;
        auto pad = std::make_unique<ColorPickerPad>(initial);
        pad->onChange = std::move(onChange);
        setContent(std::move(pad));
    }

protected:
    /// Place the content near the anchor, clamped to the window.
    Rect placeContent(Rect full, Size cs) override {
        float rise = (1.0f - presence()) * 8.0f;
        return clampToWindow(full, Rect{anchorX, anchorY + rise, cs.w, cs.h});
    }
    /// Paint the popup's surface + drop shadow.
    void paintSurface(Renderer& r, const Theme& th) override {
        Rect c = contentRect_;
        float rad = th.metric(tokens::Radius, 10.0f);
        r.fillRoundedRect(c.x + c.w * 0.5f, c.y + c.h * 0.5f + 3, c.w + 6, c.h + 6, rad + 2, Color{0, 0, 0, 60},
                          Color{0, 0, 0, 60});
        r.fillRoundedRect(c.x + c.w * 0.5f, c.y + c.h * 0.5f, c.w, c.h, rad, th.color(tokens::Surface, {46, 49, 68}),
                          th.color(tokens::SurfaceAlt, {38, 40, 56}));
    }
};

/// A swatch button showing the current color; opens a @ref ColorPicker on click.
/// This is the widget callers place.
class ColorField : public Widget {
public:
    Color value{128, 128, 128};  ///< The current color.
    float scale = 1.4f;          ///< Text scale for the hex label.
    bool showHex = true;         ///< Whether to draw the `#RRGGBB` label beside the swatch.
    std::function<void(Color)> onChange;  ///< Fired live as the picker is dragged.

    ColorField() { focusable = true; }
    /// Construct showing color `c`.
    explicit ColorField(Color c) : ColorField() { value = c; }

    /// The current color as a `#RRGGBB` string.
    std::string hex() const {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", (unsigned)value.r, (unsigned)value.g, (unsigned)value.b);
        return buf;
    }

    Size measure(Renderer& r, float, float) override {
        float h = textLineH(scale) + 14.0f;
        float w = h + (showHex ? 8.0f + r.measureText(scale, "#FFFFFF").w + 6.0f : 8.0f);
        return Size{prefW >= 0 ? prefW : w, h};
    }

    void onClick() override {
        Context* ctx = Context::current();
        if (!ctx) return;
        auto picker = std::make_unique<ColorPicker>(value, [this](Color c) {
            value = c;
            if (onChange) onChange(c);
        });
        picker->anchorX = rect.x;
        picker->anchorY = rect.y + rect.h + 4.0f;
        ctx->addOverlay(std::move(picker));
    }

    Role accessibleRole() const override { return Role::Button; }
    std::string accessibleLabel() const override { return hex(); }

    void paint(Renderer& r, const Theme& th) override {
        float rad = th.metric(tokens::Radius, 8.0f);
        Color acc = th.color(tokens::Accent, {96, 126, 205});
        float sw = rect.h - 6.0f; // swatch box
        float scx = rect.x + 3.0f + sw * 0.5f, scy = rect.y + rect.h * 0.5f;
        Color border = focused ? acc : (hovered ? Color{acc.r, acc.g, acc.b, 120} : th.color(tokens::TextDim, {140, 144, 160}));
        r.fillRoundedRect(scx, scy, sw + 2, sw + 2, rad, border, border);
        r.fillRoundedRect(scx, scy, sw - 1, sw - 1, rad - 1, value, value);
        if (showHex)
            r.text(rect.x + sw + 11.0f, rect.y + (rect.h - textLineH(scale)) * 0.5f, scale,
                   th.color(tokens::Text, {226, 229, 242}), hex().c_str());
    }
};

/// @}

} // namespace spry
