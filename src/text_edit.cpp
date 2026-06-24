#include "spry/text_edit.h"

#include "spry/clipboard.h"

namespace spry {

// ---- UTF-8 + word-class helpers -------------------------------------------
namespace {

bool isCont(unsigned char c) { return (c & 0xC0) == 0x80; } // a continuation byte

std::size_t u8Next(const std::string& s, std::size_t i) {
    if (i >= s.size()) return s.size();
    ++i;
    while (i < s.size() && isCont((unsigned char)s[i])) ++i;
    return i;
}

std::size_t u8Prev(const std::string& s, std::size_t i) {
    if (i == 0) return 0;
    --i;
    while (i > 0 && isCont((unsigned char)s[i])) --i;
    return i;
}

// Snap an arbitrary byte index back to the start of its codepoint.
std::size_t u8Snap(const std::string& s, std::size_t i) {
    if (i >= s.size()) return s.size();
    while (i > 0 && isCont((unsigned char)s[i])) --i;
    return i;
}

// Word characters for double-click / word navigation: ASCII alphanumerics and
// underscore, plus any non-ASCII byte (so accented/CJK text reads as one run).
bool isWordByte(unsigned char c) {
    if (c >= 0x80) return true;
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

} // namespace

// ---- history ---------------------------------------------------------------
void EditBuffer::record(bool coalesceTyping) {
    // Coalesce a run of plain typing into a single undo step: only the snapshot
    // taken before the first keystroke is kept.
    if (coalesceTyping && typing_) return;
    undo_.push_back({text_, caret_, anchor_});
    redo_.clear();
}

void EditBuffer::undo() {
    if (undo_.empty()) return;
    redo_.push_back({text_, caret_, anchor_});
    Snapshot s = undo_.back();
    undo_.pop_back();
    text_ = s.text;
    caret_ = s.caret;
    anchor_ = s.anchor;
    typing_ = false;
}

void EditBuffer::redo() {
    if (redo_.empty()) return;
    undo_.push_back({text_, caret_, anchor_});
    Snapshot s = redo_.back();
    redo_.pop_back();
    text_ = s.text;
    caret_ = s.caret;
    anchor_ = s.anchor;
    typing_ = false;
}

// ---- mutation --------------------------------------------------------------
void EditBuffer::setText(const std::string& t) {
    text_ = t;
    caret_ = anchor_ = text_.size();
    undo_.clear();
    redo_.clear();
    typing_ = false;
}

void EditBuffer::deleteSelection() {
    std::size_t a = selStart(), b = selEnd();
    text_.erase(a, b - a);
    caret_ = anchor_ = a;
}

void EditBuffer::insert(const std::string& utf8) {
    if (utf8.empty()) return;
    record(/*coalesceTyping=*/!hasSelection());
    if (hasSelection()) deleteSelection();
    text_.insert(caret_, utf8);
    caret_ += utf8.size();
    anchor_ = caret_;
    typing_ = true;
}

void EditBuffer::backspace() {
    if (hasSelection()) {
        record(false);
        deleteSelection();
        typing_ = false;
        return;
    }
    if (caret_ == 0) return;
    record(false);
    std::size_t p = u8Prev(text_, caret_);
    text_.erase(p, caret_ - p);
    caret_ = anchor_ = p;
    typing_ = false;
}

void EditBuffer::del() {
    if (hasSelection()) {
        record(false);
        deleteSelection();
        typing_ = false;
        return;
    }
    if (caret_ >= text_.size()) return;
    record(false);
    std::size_t n = u8Next(text_, caret_);
    text_.erase(caret_, n - caret_);
    anchor_ = caret_;
    typing_ = false;
}

void EditBuffer::copy() const {
    if (hasSelection()) setClipboardText(selectedText());
}

void EditBuffer::cut() {
    if (!hasSelection()) return;
    setClipboardText(selectedText());
    record(false);
    deleteSelection();
    typing_ = false;
}

void EditBuffer::paste() {
    std::string s = getClipboardText();
    if (s.empty()) return;
    // Single-line field: drop newlines so a pasted block stays on one line.
    std::string clean;
    clean.reserve(s.size());
    for (char c : s)
        if (c != '\n' && c != '\r') clean += c;
    if (clean.empty()) return;
    record(false);
    if (hasSelection()) deleteSelection();
    text_.insert(caret_, clean);
    caret_ += clean.size();
    anchor_ = caret_;
    typing_ = false;
}

// ---- navigation ------------------------------------------------------------
void EditBuffer::setCaret(std::size_t byte, bool extend) {
    caret_ = u8Snap(text_, byte);
    if (!extend) anchor_ = caret_;
    typing_ = false;
}

void EditBuffer::moveLeft(bool word, bool extend) {
    // Collapse a selection to its near edge when moving without extending.
    if (!extend && hasSelection()) {
        caret_ = anchor_ = selStart();
        typing_ = false;
        return;
    }
    if (word) {
        std::size_t i = caret_;
        while (i > 0 && !isWordByte((unsigned char)text_[u8Prev(text_, i)])) i = u8Prev(text_, i);
        while (i > 0 && isWordByte((unsigned char)text_[u8Prev(text_, i)])) i = u8Prev(text_, i);
        caret_ = i;
    } else {
        caret_ = u8Prev(text_, caret_);
    }
    if (!extend) anchor_ = caret_;
    typing_ = false;
}

void EditBuffer::moveRight(bool word, bool extend) {
    if (!extend && hasSelection()) {
        caret_ = anchor_ = selEnd();
        typing_ = false;
        return;
    }
    if (word) {
        std::size_t i = caret_;
        while (i < text_.size() && !isWordByte((unsigned char)text_[i])) i = u8Next(text_, i);
        while (i < text_.size() && isWordByte((unsigned char)text_[i])) i = u8Next(text_, i);
        caret_ = i;
    } else {
        caret_ = u8Next(text_, caret_);
    }
    if (!extend) anchor_ = caret_;
    typing_ = false;
}

void EditBuffer::moveHome(bool extend) {
    caret_ = 0;
    if (!extend) anchor_ = caret_;
    typing_ = false;
}

void EditBuffer::moveEnd(bool extend) {
    caret_ = text_.size();
    if (!extend) anchor_ = caret_;
    typing_ = false;
}

void EditBuffer::selectAll() {
    anchor_ = 0;
    caret_ = text_.size();
    typing_ = false;
}

void EditBuffer::setSelection(std::size_t anchor, std::size_t caret) {
    anchor_ = u8Snap(text_, anchor);
    caret_ = u8Snap(text_, caret);
    typing_ = false;
}

void EditBuffer::wordBoundsAt(std::size_t byte, std::size_t& lo, std::size_t& hi) const {
    byte = u8Snap(text_, byte);
    if (text_.empty()) {
        lo = hi = 0;
        return;
    }
    // Pick the class of the char under (or just before) the position, then expand
    // over the matching run in both directions.
    std::size_t probe = byte < text_.size() ? byte : u8Prev(text_, byte);
    bool word = isWordByte((unsigned char)text_[probe]);
    std::size_t a = byte, b = byte;
    while (a > 0 && isWordByte((unsigned char)text_[u8Prev(text_, a)]) == word) a = u8Prev(text_, a);
    while (b < text_.size() && isWordByte((unsigned char)text_[b]) == word) b = u8Next(text_, b);
    lo = a;
    hi = b;
}

void EditBuffer::selectWordAt(std::size_t byte) {
    std::size_t lo, hi;
    wordBoundsAt(byte, lo, hi);
    anchor_ = lo;
    caret_ = hi;
    typing_ = false;
}

} // namespace spry
