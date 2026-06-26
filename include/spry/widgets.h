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

// Multi-line, word-wrapped text (#220). Wraps to the width it's arranged into;
// its measured height grows with the wrapped line count. Honors explicit '\n' as
// hard line breaks. (Word-based wrapping — a single word longer than the width
// isn't broken, so clip the container if that's possible.)
class Paragraph : public Widget {
public:
    std::string text;
    float scale = 1.4f;
    std::string role = "text";
    std::optional<Color> colorOverride;
    bool preformatted = false; // true => verbatim lines (no word-wrap / whitespace collapse)

    explicit Paragraph(std::string t, float s = 1.4f) : text(std::move(t)), scale(s) {}

    Size measure(Renderer& r, float availW, float) override {
        float w = availW > 0 ? availW : (prefW >= 0 ? prefW : 200.0f);
        int lines = wrap(r, w, nullptr);
        return Size{w, (float)lines * textLineH(scale)};
    }
    void paint(Renderer& r, const Theme& th) override {
        Color c = colorOverride.value_or(th.color(role, {220, 224, 235}));
        float y = rect.y;
        wrap(r, rect.w, [&](const std::string& line) {
            r.text(rect.x, y, scale, c, line.c_str());
            y += textLineH(scale);
        });
    }
    Role accessibleRole() const override { return Role::Label; }
    std::string accessibleLabel() const override { return text; }

private:
    // Greedy word-wrap to `width`; calls emit(line) per line when set. Returns the
    // line count (used by measure with no emit).
    int wrap(Renderer& r, float width, const std::function<void(const std::string&)>& emit) const {
        // Verbatim: split only on '\n', preserving all other whitespace (license
        // text and other preformatted content rely on it). Long lines aren't
        // wrapped — clip the container if needed.
        if (preformatted) {
            int count = 0;
            std::string line;
            for (char ch : text) {
                if (ch == '\n') {
                    if (emit) emit(line);
                    line.clear();
                    ++count;
                } else {
                    line += ch;
                }
            }
            if (emit) emit(line);
            return count + 1;
        }
        int count = 0;
        std::string line, word;
        auto flush = [&] {
            if (emit) emit(line);
            line.clear();
            ++count;
        };
        auto pushWord = [&] {
            if (word.empty()) return;
            std::string cand = line.empty() ? word : line + " " + word;
            if (r.measureText(scale, cand.c_str()).w > width && !line.empty()) {
                flush();
                line = word;
            } else {
                line = cand;
            }
            word.clear();
        };
        for (char ch : text) {
            if (ch == '\n') {
                pushWord();
                flush();
            } else if (ch == ' ' || ch == '\t') {
                pushWord();
            } else {
                word += ch;
            }
        }
        pushWord();
        flush();
        return count;
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

// A focusable button (#216): click, or Enter/Space when focused, to activate.
// Brightens on hover, darkens on press, shows a focus ring for keyboard nav.
class Button : public Widget {
public:
    std::string label;
    std::function<void()> onClickCb;
    float scale = 1.4f;
    bool selected = false; // persistent active state (e.g. the current sidebar tab)
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
        Color acc = th.color("accent", {96, 126, 205});
        // The selected state reads as a brighter (accent-tinted) base; hover then
        // brightens further so it's still responsive when already selected.
        Color base = selected ? lerp(th.color("surface", {46, 49, 68}), acc, 0.30f) : th.color("surface", {46, 49, 68});
        Color c = hovered ? lerp(base, acc, 0.40f) : base;
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

// A focusable on/off switch (#214). Click or Enter/Space to toggle; the knob
// slides between ends via its own spring, and the track colour crossfades.
class Toggle : public Widget {
public:
    std::string label;
    bool on = false;
    std::function<void(bool)> onChange;
    float scale = 1.4f;
    Spring knob; // 0 (off) .. 1 (on)

    explicit Toggle(std::string l = {}, bool v = false) : label(std::move(l)), on(v) {
        focusable = true;
        knob.value = knob.target = v ? 1.0f : 0.0f;
    }
    Size measure(Renderer& r, float, float) override {
        float track = textLineH(scale) * 1.7f;
        float lw = label.empty() ? 0.0f : 10.0f + r.measureText(scale, label.c_str()).w;
        return Size{track + lw, textLineH(scale) + 6.0f};
    }
    void onClick() override {
        on = !on;
        if (onChange) onChange(on);
    }
    Role accessibleRole() const override { return Role::Checkbox; }
    std::string accessibleLabel() const override { return label; }
    void update(float dt) override {
        knob.target = on ? 1.0f : 0.0f;
        knob.step(dt);
        Widget::update(dt);
    }
    void paint(Renderer& r, const Theme& th) override {
        float h = textLineH(scale) * 0.9f;
        float w = h * 1.9f;
        float k = knob.value < 0 ? 0 : (knob.value > 1 ? 1 : knob.value);
        float cx = rect.x + w * 0.5f, cy = rect.y + rect.h * 0.5f;
        Color off = th.color("surface", {40, 43, 62});
        Color acc = th.color("accent", {96, 126, 205});
        Color track = lerp(off, acc, k);
        if (focused) {
            Color ring{acc.r, acc.g, acc.b, 110};
            r.fillRoundedRect(cx, cy, w + 6, h + 6, (h + 6) * 0.5f, ring, ring);
        }
        r.fillRoundedRect(cx, cy, w, h, h * 0.5f, track, lerp(track, Color{0, 0, 0, 255}, 0.18f));
        float knobR = h - 6.0f;
        float kx = rect.x + 3.0f + knobR * 0.5f + k * (w - knobR - 6.0f);
        Color knobCol = (hovered || focused) ? Color{255, 255, 255, 255} : Color{232, 235, 244, 255};
        r.fillRoundedRect(kx, cy, knobR, knobR, knobR * 0.5f, knobCol, lerp(knobCol, Color{200, 204, 214, 255}, 0.6f));
        if (!label.empty())
            r.text(rect.x + w + 10.0f, rect.y + (rect.h - textLineH(scale)) * 0.5f, scale,
                   th.color("text", {226, 229, 242}), label.c_str());
    }
};

// A focusable horizontal slider (#214). Click/drag the track to set the value;
// arrow keys nudge, Home/End jump to the ends. The thumb eases via a spring.
class Slider : public Widget {
public:
    float minV = 0.0f, maxV = 1.0f, value = 0.0f;
    float step = 0.0f; // 0 => continuous; otherwise snap to multiples of step
    std::function<void(float)> onChange;
    Spring thumb; // 0..1 fraction

    Slider() { focusable = true; }
    Slider(float lo, float hi, float v) : minV(lo), maxV(hi), value(v) {
        focusable = true;
        thumb.value = thumb.target = fraction();
    }

    Size measure(Renderer&, float, float) override {
        float h = 22.0f;
        return Size{prefW >= 0 ? prefW : 180.0f, h};
    }
    Role accessibleRole() const override { return Role::None; }

    bool onMouseDown(float x, float, int, bool, bool) override {
        setFromX(x);
        return true;
    }
    void onMouseDrag(float x, float) override { setFromX(x); }
    bool onKey(Key key, bool, bool, bool) override {
        float s = step > 0 ? step : (maxV - minV) / 20.0f;
        switch (key) {
            case Key::Left: setValue(value - s); return true;
            case Key::Right: setValue(value + s); return true;
            case Key::Home: setValue(minV); return true;
            case Key::End: setValue(maxV); return true;
            default: return false;
        }
    }
    void update(float dt) override {
        thumb.target = fraction();
        thumb.step(dt);
        Widget::update(dt);
    }
    void paint(Renderer& r, const Theme& th) override {
        float cy = rect.y + rect.h * 0.5f;
        float tx = rect.x + kThumbR, tw = rect.w - 2 * kThumbR;
        Color acc = th.color("accent", {96, 126, 205});
        Color trackBg = th.color("surface", {40, 43, 62});
        float f = thumb.value < 0 ? 0 : (thumb.value > 1 ? 1 : thumb.value);
        r.fillRoundedRect(tx + tw * 0.5f, cy, tw, 5.0f, 2.5f, trackBg, trackBg);                 // track
        r.fillRoundedRect(tx + (tw * f) * 0.5f, cy, tw * f, 5.0f, 2.5f, acc, acc);                // filled
        float kx = tx + tw * f;
        if (focused) {
            Color ring{acc.r, acc.g, acc.b, 110};
            r.fillRoundedRect(kx, cy, kThumbR * 2 + 6, kThumbR * 2 + 6, kThumbR + 3, ring, ring);
        }
        Color knob = (hovered || focused) ? Color{255, 255, 255, 255} : Color{232, 235, 244, 255};
        r.fillRoundedRect(kx, cy, kThumbR * 2, kThumbR * 2, kThumbR, knob, lerp(knob, acc, 0.25f));
    }

private:
    static constexpr float kThumbR = 9.0f;
    float fraction() const { return maxV > minV ? (value - minV) / (maxV - minV) : 0.0f; }
    void setFromX(float x) {
        float tx = rect.x + kThumbR, tw = rect.w - 2 * kThumbR;
        float f = tw > 0 ? (x - tx) / tw : 0.0f;
        setValue(minV + f * (maxV - minV));
    }
    void setValue(float v) {
        if (step > 0) v = minV + std::round((v - minV) / step) * step;
        // Clamp after snapping: a range that isn't a whole multiple of step could
        // otherwise round just past an end.
        if (v < minV) v = minV;
        if (v > maxV) v = maxV;
        if (v == value) return;
        value = v;
        if (onChange) onChange(value);
    }
};

} // namespace spry
