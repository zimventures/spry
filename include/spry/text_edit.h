#pragma once
#include <cstddef>
#include <string>
#include <vector>

/// @file text_edit.h
/// The headless editing model behind `TextField` / `TextArea`.

namespace spry {

/// @addtogroup widgets
/// @{

/// The editing model behind `TextField` (#213): a single-line UTF-8 string with a
/// caret, a selection anchor, and undo/redo. It does no rendering, so it is fully
/// unit-testable headless. All indices are byte offsets on UTF-8 boundaries; the
/// helpers keep the caret and anchor on those boundaries.
class EditBuffer {
public:
    /// The current text.
    const std::string& text() const { return text_; }
    /// Replace the text (resets caret/selection/undo history).
    void setText(const std::string& t);

    /// Caret position, as a byte offset.
    std::size_t caret() const { return caret_; }
    /// Selection anchor, as a byte offset (equals the caret when there's no selection).
    std::size_t anchor() const { return anchor_; }
    /// True if there is a non-empty selection.
    bool hasSelection() const { return caret_ != anchor_; }
    /// Start (lower byte offset) of the selection.
    std::size_t selStart() const { return caret_ < anchor_ ? caret_ : anchor_; }
    /// End (higher byte offset) of the selection.
    std::size_t selEnd() const { return caret_ < anchor_ ? anchor_ : caret_; }
    /// The currently-selected text.
    std::string selectedText() const { return text_.substr(selStart(), selEnd() - selStart()); }

    /// Insert `utf8` at the caret, replacing the selection if any (records undo;
    /// consecutive typing coalesces into one step).
    void insert(const std::string& utf8);
    /// Delete left of the caret, or the selection.
    void backspace();
    /// Delete right of the caret, or the selection.
    void del();
    /// Copy the selection to the clipboard, then delete it.
    void cut();
    /// Copy the selection to the clipboard.
    void copy() const;
    /// Paste the clipboard at the caret (replacing the selection).
    void paste();

    /// Move the caret left; `word` moves by word, `extend` keeps the anchor (shift-select).
    void moveLeft(bool word, bool extend);
    /// Move the caret right; `word` moves by word, `extend` keeps the anchor.
    void moveRight(bool word, bool extend);
    /// Move the caret to the start; `extend` keeps the anchor.
    void moveHome(bool extend);
    /// Move the caret to the end; `extend` keeps the anchor.
    void moveEnd(bool extend);
    /// Set the caret to `byte` (clamped to a UTF-8 boundary); `extend` keeps the anchor.
    void setCaret(std::size_t byte, bool extend);
    /// Select the whole buffer.
    void selectAll();
    /// Select the word at `byte`.
    void selectWordAt(std::size_t byte);
    /// Set both ends of the selection (each snapped to a UTF-8 boundary).
    void setSelection(std::size_t anchor, std::size_t caret);
    /// Byte range of the word at `byte` (its char-class run), without moving the
    /// selection — used to drive word-granular drag selection.
    void wordBoundsAt(std::size_t byte, std::size_t& lo, std::size_t& hi) const;

    /// Undo the last edit.
    void undo();
    /// Redo the last undone edit.
    void redo();
    /// True if there's anything to @ref undo.
    bool canUndo() const { return !undo_.empty(); }
    /// True if there's anything to @ref redo.
    bool canRedo() const { return !redo_.empty(); }

private:
    struct Snapshot {
        std::string text;
        std::size_t caret, anchor;
    };
    void record(bool coalesceTyping); // push current state onto the undo stack
    void deleteSelection();           // remove [selStart,selEnd); caller records first

    std::string text_;
    std::size_t caret_ = 0, anchor_ = 0;
    std::vector<Snapshot> undo_, redo_;
    bool typing_ = false; // last edit was an insert (for coalescing a typing run)
};

/// @}

} // namespace spry
