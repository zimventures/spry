#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "text_edit.h"
#include "widget.h"

// A multi-line editable text area (#250): word-wrapped lines, hard line breaks on
// Enter, vertical caret navigation (Up/Down, line-relative Home/End), click-to-place
// across rows, drag + shift selection spanning rows, clipboard/undo, and a vertical
// scroll. Shares the EditBuffer model with TextField (byte-level edits work across
// newlines); this widget adds the 2D layout + navigation. Colours/metrics come from
// the Theme.
namespace spry {

class TextArea : public Widget {
public:
    std::string placeholder;
    float scale = 1.4f;
    int visibleRows = 4;                               // height, in text rows
    std::function<void(const std::string&)> onChange; // fired after any edit

    TextArea() { focusable = true; }
    explicit TextArea(std::string initial) : TextArea() { edit_.setText(std::move(initial)); }

    void setText(const std::string& t) { edit_.setText(t); }
    const std::string& text() const { return edit_.text(); }

    Size measure(Renderer& r, float availW, float availH) override;
    void arrange(Renderer& r, Rect rc) override;
    void update(float dt) override;
    void paint(Renderer& r, const Theme& th) override;

    bool onMouseDown(float x, float y, int button, bool shift, bool ctrl) override;
    void onMouseDrag(float x, float y) override;
    bool onMouseUp(float x, float y, int button) override;
    bool onWheel(float dx, float dy) override;
    bool onKey(Key key, bool shift, bool ctrl, bool alt) override;
    void onText(const char* utf8) override;
    void onTextEditing(const char* utf8) override;
    void onFocusChanged(bool focused) override;

    bool wantsTextInput() const override { return true; }
    Rect caretRect() const override { return caretRect_; }
    Role accessibleRole() const override { return Role::TextField; }
    std::string accessibleLabel() const override { return edit_.text(); }

private:
    struct VRow {
        std::size_t start, end; // [start, end) byte range (end excludes a trailing '\n')
    };

    float innerWidth() const;
    float lineH() const;
    float viewportHeight() const { return rect.h - 2.0f * kPad; }
    float contentHeight() const; // total height of all rows
    bool scrollbarVisible() const { return contentHeight() > viewportHeight() + 0.5f; }
    void scrollbarDragTo(float y); // map a pointer y to a scroll offset
    void layout(Renderer& r);                                    // (re)build rows_ for rect width
    std::size_t rowOfByte(std::size_t b) const;                  // visual row containing the caret byte
    float xForByteInRow(const VRow& row, std::size_t b) const;   // x within the row (text space)
    std::size_t byteAtXInRow(const VRow& row, float localX) const;
    std::size_t byteAt(float x, float y) const;                  // window point -> caret byte
    void moveVertical(int dir, bool extend);                     // Up/Down preserving a sticky x
    void ensureCaretVisible();
    void changed();

    EditBuffer edit_;
    std::vector<VRow> rows_;
    std::string preedit_;       // IME composition, drawn but not committed
    float scrollY_ = 0.0f;      // vertical scroll so the caret row stays visible
    float blink_ = 0.0f;        // caret blink phase
    float clock_ = 0.0f;        // monotonic-ish time for double-click timing
    Rect caretRect_{};          // caret in Spry coords (for the IME area)
    float desiredX_ = -1.0f;    // sticky target x for Up/Down (-1 = recompute from caret)
    float lastClickT_ = -1.0f;  // clock_ at the previous mouse-down
    float lastClickX_ = 0.0f, lastClickY_ = 0.0f;
    int clickCount_ = 0;
    bool draggingBar_ = false;  // dragging the scrollbar thumb
    bool scrollToCaret_ = false; // a caret move requested re-centering on next paint
    Renderer* r_ = nullptr;     // last renderer seen (events need it to measure)
    static constexpr float kPad = 8.0f;
    static constexpr float kScrollW = 12.0f;  // scrollbar gutter width
    static constexpr float kMinThumb = 24.0f; // minimum thumb height
};

} // namespace spry
