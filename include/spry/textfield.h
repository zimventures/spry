#pragma once
#include <cstddef>
#include <functional>
#include <string>

#include "anim.h"
#include "text_edit.h"
#include "widget.h"

/// @file textfield.h
/// A single-line editable text field.

namespace spry {

/// @addtogroup widgets
/// @{

/// A single-line editable text field (#213): caret, click-to-place, drag + shift
/// selection, double-click word select, clipboard cut/copy/paste, undo/redo, and
/// IME composition. Colors and metrics come from the active `Theme`. The editing
/// logic lives in @ref EditBuffer; this widget adds hit-testing, scrolling, and paint.
class TextField : public Widget {
public:
    std::string placeholder;  ///< Dimmed hint shown when the field is empty.
    float scale = 1.4f;       ///< Text scale (multiple of the base size).
    bool masked = false;      ///< Render each character as a bullet (password fields).
    std::function<void(const std::string&)> onChange;  ///< Fired after any edit, with the text.
    std::function<void()> onSubmit;                    ///< Fired when Enter is pressed.

    TextField() { focusable = true; }
    /// Construct with initial text.
    explicit TextField(std::string initial) : TextField() { edit_.setText(std::move(initial)); }

    /// Replace the contents.
    void setText(const std::string& t) { edit_.setText(t); }
    /// The current text.
    const std::string& text() const { return edit_.text(); }

    Size measure(Renderer& r, float availW, float availH) override;
    void arrange(Renderer& r, Rect rc) override;
    void update(float dt) override;
    void paint(Renderer& r, const Theme& th) override;

    bool onMouseDown(float x, float y, int button, bool shift, bool ctrl) override;
    void onMouseDrag(float x, float y) override;
    bool onKey(Key key, bool shift, bool ctrl, bool alt) override;
    void onText(const char* utf8) override;
    void onTextEditing(const char* utf8) override;
    void onFocusChanged(bool focused) override;

    bool wantsTextInput() const override { return true; }
    Rect caretRect() const override { return caretRect_; }
    Role accessibleRole() const override { return Role::TextField; }
    // Masked fields expose bullets, never the secret, to the a11y tree.
    std::string accessibleLabel() const override { return masked ? maskOf(edit_.text()) : edit_.text(); }

private:
    float textOriginX() const; // left edge of the text content (inside padding)
    float innerWidth() const;
    std::string maskOf(const std::string& s) const; // s, or one bullet per codepoint when masked
    std::size_t byteAtX(float x) const; // window x -> nearest caret byte index
    float xForByte(std::size_t byte) const;
    void ensureCaretVisible();
    void changed();

    EditBuffer edit_;
    std::string preedit_;      // IME composition, drawn but not yet committed
    float scrollX_ = 0.0f;     // horizontal scroll so the caret stays visible
    float blink_ = 0.0f;       // caret blink phase (seconds)
    Rect caretRect_{};         // caret in Spry coords (for the IME area)
    float clock_ = 0.0f;       // monotonic-ish time for double-click timing
    float lastClickT_ = -1.0f; // clock_ at the previous mouse-down
    float lastClickX_ = 0.0f;  // x of the previous mouse-down (spot check)
    int clickCount_ = 0;
    // Drag granularity from the initiating click: a word/line selection must keep
    // its granularity as the drag extends (and survive Context's per-frame drag).
    enum class SelMode { Char, Word, Line };
    SelMode selMode_ = SelMode::Char;
    std::size_t wordLo_ = 0, wordHi_ = 0; // anchor word bounds, for word-granular drag
    Renderer* r_ = nullptr; // last renderer seen (events need it to measure)
    static constexpr float kPad = 10.0f;
};

/// @}

} // namespace spry
