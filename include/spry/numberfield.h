#pragma once
#include <algorithm>
#include <functional>
#include <string>

#include "box.h"
#include "textfield.h"
#include "widget.h"

/// @file numberfield.h
/// A non-negative integer entry with up/down steppers.

namespace spry {

/// @addtogroup widgets
/// @{

/// A whole-number entry with a stepper column (#66).
///
/// A `TextField` accepts anything, which is the wrong contract for a count: a field
/// that lets "8 workers" be typed leaves every consumer to re-derive what a number is,
/// and they do not all agree. This one refuses the keystroke instead — non-digits never
/// enter the buffer, from typing or from a paste — so what the field shows is always
/// what it means.
///
/// **Non-negative by construction.** Digits are the only accepted input and there is no
/// sign, so the value cannot go below `minValue`, which itself cannot go below zero.
/// Counts of things — replicas, retries, ports, seconds — are what this is for; a field
/// that needs negatives wants a different control rather than a flag on this one.
///
/// The box may be *empty* while it is being retyped, and empty is not zero: @ref valid
/// reports false and @ref value falls back to `minValue`. Consumers gate their commit
/// on `valid()` rather than reading a number that nobody typed. Deliberately no
/// snap-to-minimum on blur for the same reason — landing on a real value by walking
/// away from an empty box is how a service gets scaled to zero by accident.
class NumberField : public Box {
public:
    /// Fired whenever the value changes — typing, stepping, or setValue().
    /// Not fired while the box is empty; @ref valid is the signal for that.
    std::function<void(int)> onValue;

    /// Construct over `initial`, clamped into [`lo`, `hi`].
    explicit NumberField(int initial = 0, int lo = 0, int hi = 999999) {
        axis = Axis::Row;
        spacing = 0;
        cross = Align::Center;
        lo_ = std::max(0, lo);
        hi_ = std::max(lo_, hi);

        auto entry = std::make_unique<Entry>(this);
        entry_ = entry.get();
        entry_->prefW = 78.0f;
        add(std::move(entry));

        auto stepper = std::make_unique<Stepper>(this);
        stepper_ = stepper.get();
        add(std::move(stepper));

        setValue(initial);
    }

    /// The current value, clamped into range. `minValue` while the box is empty —
    /// check @ref valid before treating it as something the operator asked for.
    int value() const {
        const std::string& t = entry_->text();
        if (t.empty())
            return lo_;
        long long n = 0;
        for (char c : t) {
            n = n * 10 + (c - '0');
            if (n > hi_)
                return hi_; // saturate rather than wrap on a very long paste
        }
        return static_cast<int>(std::clamp<long long>(n, lo_, hi_));
    }

    /// Whether the box holds a number. False while it is empty.
    bool valid() const { return !entry_->text().empty(); }

    void setValue(int v) {
        const int clamped = std::clamp(v, lo_, hi_);
        entry_->setText(std::to_string(clamped));
        if (onValue)
            onValue(clamped);
    }

    /// Bounds. `lo` is floored at zero — this control has no sign.
    void setRange(int lo, int hi) {
        lo_ = std::max(0, lo);
        hi_ = std::max(lo_, hi);
        if (valid())
            setValue(value());
    }
    int minValue() const { return lo_; }
    int maxValue() const { return hi_; }

    /// How much one step, or one Up/Down keypress, moves the value.
    void setStep(int s) { step_ = std::max(1, s); }

    /// The text box itself, for a placeholder or an explicit width.
    TextField& field() { return *entry_; }

    void step(int delta) {
        // From the *current* value, so stepping out of an empty box starts at the
        // minimum rather than at whatever was there before it was cleared.
        setValue(value() + delta * step_);
        entry_->takeFocus();
    }

private:
    /// The text half: digits only, and arrow keys step.
    class Entry : public TextField {
    public:
        explicit Entry(NumberField* owner) : owner_(owner) {
            onChange = [this](const std::string&) {
                if (owner_->onValue && owner_->valid())
                    owner_->onValue(owner_->value());
            };
        }

        void takeFocus() {
            if (Context* c = Context::current())
                c->setFocus(this);
        }

        // The whole point: a non-digit never reaches the buffer, so there is no
        // sanitising pass afterwards and no caret to put back.
        void onText(const char* utf8) override {
            if (!utf8)
                return;
            std::string digits;
            for (const char* p = utf8; *p; ++p)
                if (*p >= '0' && *p <= '9')
                    digits.push_back(*p);
            if (!digits.empty())
                TextField::onText(digits.c_str());
        }

        bool onKey(Key key, bool shift, bool ctrl, bool alt) override {
            if (key == Key::Up) {
                owner_->step(+1);
                return true;
            }
            if (key == Key::Down) {
                owner_->step(-1);
                return true;
            }
            const bool handled = TextField::onKey(key, shift, ctrl, alt);
            // Paste is the one route that puts text in without going through onText.
            if (handled && ctrl && key == Key::V)
                stripNonDigits();
            return handled;
        }

    private:
        void stripNonDigits() {
            const std::string& t = text();
            std::string digits;
            digits.reserve(t.size());
            for (char c : t)
                if (c >= '0' && c <= '9')
                    digits.push_back(c);
            if (digits != t)
                setText(digits); // caret lands at the end, which is where a paste leaves it
        }
        NumberField* owner_;
    };

    /// The stepper column: two triangles, top increments and bottom decrements.
    class Stepper : public Widget {
    public:
        explicit Stepper(NumberField* owner) : owner_(owner) {}

        Size measure(Renderer&, float, float) override { return Size{kWidth, 30.0f}; }

        bool onMouseDown(float, float y, int button, bool, bool) override {
            if (button != 0)
                return false;
            owner_->step(y < rect.y + rect.h * 0.5f ? +1 : -1);
            return true;
        }

        void paint(Renderer& r, const Theme& th) override {
            // Both arrows share one hover state: Widget reports *that* the pointer is
            // over the column, not where, and the half is only known once a click
            // arrives. Lighting the pair is honest about that; guessing a half from
            // stale coordinates would highlight the wrong arrow after a move.
            const Color c = hovered ? th.color(tokens::Text, {226, 229, 242}) : th.color(tokens::TextDim, {150, 154, 170});
            const float cx = rect.x + rect.w * 0.5f;
            tri(r, cx, rect.y + rect.h * 0.30f, 4.0f, true, c);
            tri(r, cx, rect.y + rect.h * 0.70f, 4.0f, false, c);
        }

    private:
        static constexpr float kWidth = 20.0f;
        static void tri(Renderer& r, float cx, float cy, float s, bool up, Color c) {
            const float dy = up ? -s * 0.6f : s * 0.6f;
            std::vector<Vertex> v{{cx - s, cy - dy, c}, {cx + s, cy - dy, c}, {cx, cy + dy, c}};
            std::vector<int> idx{0, 1, 2};
            r.fillMesh(v, idx);
        }
        NumberField* owner_;
    };

    Entry* entry_ = nullptr;
    Stepper* stepper_ = nullptr;
    int lo_ = 0, hi_ = 999999, step_ = 1;
};

/// @}

} // namespace spry
