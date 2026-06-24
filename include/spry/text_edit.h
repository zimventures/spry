#pragma once
#include <cstddef>
#include <string>
#include <vector>

// The editing model behind TextField (#213): a single-line UTF-8 string with a
// caret, a selection anchor, and undo/redo. It does no rendering, so it is fully
// unit-testable headless. All indices are byte offsets on UTF-8 boundaries; the
// helpers keep the caret and anchor on those boundaries.
namespace spry {

class EditBuffer {
public:
    const std::string& text() const { return text_; }
    void setText(const std::string& t); // resets caret/selection/undo history

    std::size_t caret() const { return caret_; }
    std::size_t anchor() const { return anchor_; }
    bool hasSelection() const { return caret_ != anchor_; }
    std::size_t selStart() const { return caret_ < anchor_ ? caret_ : anchor_; }
    std::size_t selEnd() const { return caret_ < anchor_ ? anchor_ : caret_; }
    std::string selectedText() const { return text_.substr(selStart(), selEnd() - selStart()); }

    // Content edits — each records an undo step (consecutive typing coalesces).
    void insert(const std::string& utf8); // replaces the selection if any
    void backspace();                      // delete left (or the selection)
    void del();                            // delete right (or the selection)
    void cut();                            // selection -> clipboard, then delete
    void copy() const;                     // selection -> clipboard
    void paste();                          // clipboard -> at caret (replaces selection)

    // Caret / selection movement. extend=true keeps the anchor (shift-select);
    // word=true moves by word instead of by codepoint.
    void moveLeft(bool word, bool extend);
    void moveRight(bool word, bool extend);
    void moveHome(bool extend);
    void moveEnd(bool extend);
    void setCaret(std::size_t byte, bool extend); // clamped to a UTF-8 boundary
    void selectAll();
    void selectWordAt(std::size_t byte);
    void setSelection(std::size_t anchor, std::size_t caret); // both snapped to UTF-8 boundaries
    // Byte range of the word at `byte` (its char-class run), without moving the
    // selection — used to drive word-granular drag selection.
    void wordBoundsAt(std::size_t byte, std::size_t& lo, std::size_t& hi) const;

    void undo();
    void redo();
    bool canUndo() const { return !undo_.empty(); }
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

} // namespace spry
