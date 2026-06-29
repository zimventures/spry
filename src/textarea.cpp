#include "spry/textarea.h"

#include <algorithm>
#include <cmath>

namespace spry {

namespace {
// Advance one UTF-8 codepoint from byte i.
std::size_t nextCp(const std::string& s, std::size_t i) {
    if (i >= s.size()) return s.size();
    std::size_t j = i + 1;
    while (j < s.size() && ((unsigned char)s[j] & 0xC0) == 0x80) ++j;
    return j;
}
} // namespace

// ---- geometry --------------------------------------------------------------
// Reserve the scrollbar gutter permanently (like ScrollView) so wrapping is stable
// — adding/removing the thumb never reflows the text.
float TextArea::innerWidth() const { return rect.w - 2.0f * kPad - kScrollW; }
float TextArea::lineH() const { return textLineH(scale); }
float TextArea::contentHeight() const { return static_cast<float>(rows_.size()) * lineH(); }

void TextArea::scrollbarDragTo(float y) {
    float vh = viewportHeight();
    float ch = contentHeight();
    float thumbH = std::max(kMinThumb, vh * vh / std::max(ch, 1.0f));
    float f = (vh - thumbH) > 0 ? (y - (rect.y + kPad) - thumbH * 0.5f) / (vh - thumbH) : 0.0f;
    scrollY_ = std::clamp(f * std::max(0.0f, ch - vh), 0.0f, std::max(0.0f, ch - vh));
}

void TextArea::layout(Renderer& r) {
    r_ = &r;
    rows_.clear();
    const std::string& s = edit_.text();
    const float width = innerWidth();
    std::size_t ls = 0;
    while (true) {
        std::size_t nl = s.find('\n', ls);
        std::size_t logEnd = (nl == std::string::npos) ? s.size() : nl;

        // Greedy word-wrap [ls, logEnd) to `width`. Rows stay contiguous (a break
        // point sits just after the breaking space) so every byte maps to one row.
        std::size_t rowStart = ls, cursor = ls, lastBreak = std::string::npos;
        while (cursor < logEnd) {
            std::size_t nxt = nextCp(s, cursor);
            float w = r.measureText(scale, s.substr(rowStart, nxt - rowStart).c_str()).w;
            if (w > width && cursor > rowStart) {
                std::size_t breakAt = (lastBreak != std::string::npos && lastBreak > rowStart) ? lastBreak : cursor;
                rows_.push_back({rowStart, breakAt});
                rowStart = breakAt;
                cursor = breakAt;
                lastBreak = std::string::npos;
                continue;
            }
            if (s[cursor] == ' ') lastBreak = nxt; // can break right after this space
            cursor = nxt;
        }
        rows_.push_back({rowStart, logEnd});

        if (nl == std::string::npos) break;
        ls = nl + 1;
        if (ls == s.size()) { // trailing newline => one empty final row
            rows_.push_back({s.size(), s.size()});
            break;
        }
    }
    if (rows_.empty()) rows_.push_back({0, 0});
}

std::size_t TextArea::rowOfByte(std::size_t b) const {
    // Prefer the latest row that contains b, so a soft-wrap boundary lands at the
    // start of the next row rather than the dangling end of the previous one.
    std::size_t found = 0;
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (b >= rows_[i].start && b <= rows_[i].end) found = i;
        if (b < rows_[i].start) break;
    }
    return found;
}

float TextArea::xForByteInRow(const VRow& row, std::size_t b) const {
    if (!r_ || b <= row.start) return 0.0f;
    std::size_t e = std::min(b, row.end);
    return r_->measureText(scale, edit_.text().substr(row.start, e - row.start).c_str()).w;
}

std::size_t TextArea::byteAtXInRow(const VRow& row, float localX) const {
    if (!r_ || localX <= 0.0f) return row.start;
    std::size_t best = row.start;
    float bestDx = std::abs(localX);
    for (std::size_t b = row.start; b <= row.end;) {
        float dx = std::abs(xForByteInRow(row, b) - localX);
        if (dx < bestDx) {
            bestDx = dx;
            best = b;
        }
        if (b == row.end) break;
        b = nextCp(edit_.text(), b);
    }
    return best;
}

std::size_t TextArea::byteAt(float x, float y) const {
    if (rows_.empty()) return 0;
    float localY = y - (rect.y + kPad) + scrollY_;
    int ri = static_cast<int>(std::floor(localY / lineH()));
    ri = std::clamp(ri, 0, static_cast<int>(rows_.size()) - 1);
    return byteAtXInRow(rows_[ri], x - (rect.x + kPad));
}

void TextArea::moveVertical(int dir, bool extend) {
    if (rows_.empty()) return;
    std::size_t cr = rowOfByte(edit_.caret());
    if (desiredX_ < 0.0f) desiredX_ = xForByteInRow(rows_[cr], edit_.caret());
    int target = static_cast<int>(cr) + dir;
    if (target < 0 || target >= static_cast<int>(rows_.size())) {
        // Past the top/bottom edge: snap to the very start / end.
        edit_.setCaret(dir < 0 ? 0 : edit_.text().size(), extend);
        return;
    }
    edit_.setCaret(byteAtXInRow(rows_[target], desiredX_), extend);
}

void TextArea::ensureCaretVisible() {
    if (rows_.empty()) return;
    std::size_t cr = rowOfByte(edit_.caret());
    float top = static_cast<float>(cr) * lineH();
    float viewH = rect.h - 2.0f * kPad;
    if (top - scrollY_ < 0) scrollY_ = top;
    if (top + lineH() - scrollY_ > viewH) scrollY_ = top + lineH() - viewH;
    float maxScroll = std::max(0.0f, static_cast<float>(rows_.size()) * lineH() - viewH);
    scrollY_ = std::clamp(scrollY_, 0.0f, maxScroll);
}

void TextArea::changed() {
    if (onChange) onChange(edit_.text());
}

// ---- layout / frame --------------------------------------------------------
Size TextArea::measure(Renderer& r, float, float) {
    r_ = &r;
    float w = prefW >= 0 ? prefW : 260.0f; // grows along the row if grow > 0
    float h = lineH() * static_cast<float>(std::max(1, visibleRows)) + 2.0f * kPad;
    return Size{w, h};
}

void TextArea::arrange(Renderer& r, Rect rc) {
    Widget::arrange(r, rc);
    layout(r);
}

void TextArea::update(float dt) {
    clock_ += dt;
    blink_ += dt;
    Widget::update(dt);
}

void TextArea::paint(Renderer& r, const Theme& th) {
    r_ = &r;
    layout(r); // text may have changed since arrange (onChange edits)
    float rad = th.metric("radius", 8.0f) * 0.7f;
    float cx = rect.x + rect.w * 0.5f, cy = rect.y + rect.h * 0.5f;

    Color surface = th.color("surfaceAlt", {32, 34, 48});
    Color acc = th.color("accent", {96, 126, 205});
    if (focused) {
        Color ring{acc.r, acc.g, acc.b, 150};
        r.fillRoundedRect(cx, cy, rect.w, rect.h, rad, ring, ring);
        r.fillRoundedRect(cx, cy, rect.w - 3, rect.h - 3, rad - 1, surface, surface);
    } else {
        Color border = hovered ? Color{acc.r, acc.g, acc.b, 90} : th.color("textDim", {140, 144, 160});
        r.fillRoundedRect(cx, cy, rect.w, rect.h, rad, border, border);
        r.fillRoundedRect(cx, cy, rect.w - 2, rect.h - 2, rad - 1, surface, surface);
    }

    ensureCaretVisible();

    Color textCol = th.color("text", {226, 229, 242});
    Color dimCol = th.color("textDim", {140, 144, 160});
    const std::string& s = edit_.text();
    float originX = rect.x + kPad;

    r.pushClip(Rect{rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4});

    if (s.empty() && preedit_.empty() && !placeholder.empty())
        r.text(originX, rect.y + kPad, scale, dimCol, placeholder.c_str());

    std::size_t selLo = edit_.selStart(), selHi = edit_.selEnd();
    std::size_t caret = edit_.caret();
    std::size_t caretRow = rowOfByte(caret);

    for (std::size_t i = 0; i < rows_.size(); ++i) {
        float ry = rect.y + kPad + static_cast<float>(i) * lineH() - scrollY_;
        if (ry + lineH() < rect.y || ry > rect.y + rect.h) continue; // off-screen
        const VRow& row = rows_[i];

        // Selection highlight (intersection of the selection with this row).
        if (edit_.hasSelection() && selHi > row.start && selLo < row.end + 1) {
            std::size_t a = std::max(selLo, row.start);
            std::size_t b = std::min(selHi, row.end);
            if (b >= a) {
                float x0 = xForByteInRow(row, a), x1 = xForByteInRow(row, b);
                // A selected hard line-break shows a small trailing sliver.
                float w = (selHi > row.end) ? (x1 - x0 + 5.0f) : (x1 - x0);
                Color sel{acc.r, acc.g, acc.b, (uint8_t)(focused ? 110 : 70)};
                r.fillRect(originX + x0, ry, w, lineH(), sel);
            }
        }

        if (row.end > row.start)
            r.text(originX, ry, scale, textCol, s.substr(row.start, row.end - row.start).c_str());

        // Caret on this row.
        bool caretOn = focused && std::fmod(blink_, 1.0f) < 0.5f;
        if (i == caretRow && caretOn) {
            float cxp = originX + xForByteInRow(row, caret);
            r.fillRect(cxp, ry + 1.0f, 1.5f, lineH() - 2.0f, textCol);
            caretRect_ = Rect{cxp, ry, 2.0f, lineH()};
        }
    }

    r.popClip();

    // Vertical scrollbar — only when the content overflows the visible rows.
    if (scrollbarVisible()) {
        float vh = viewportHeight();
        float ch = contentHeight();
        float thumbH = std::max(kMinThumb, vh * vh / ch);
        float maxS = ch - vh;
        float t = maxS > 0 ? scrollY_ / maxS : 0.0f;
        float ty = rect.y + kPad + t * (vh - thumbH);
        float sx = rect.x + rect.w - kScrollW * 0.5f - 2.0f;
        float tw = kScrollW - 6.0f;
        Color trackC{dimCol.r, dimCol.g, dimCol.b, 40};
        r.fillRoundedRect(sx, rect.y + kPad + vh * 0.5f, tw, vh, tw * 0.5f, trackC, trackC);
        Color thumbC = (draggingBar_ || hovered) ? acc : dimCol;
        r.fillRoundedRect(sx, ty + thumbH * 0.5f, tw, thumbH, tw * 0.5f, thumbC, thumbC);
    }
}

// ---- input -----------------------------------------------------------------
bool TextArea::onMouseDown(float x, float y, int /*button*/, bool shift, bool /*ctrl*/) {
    // A press in the scrollbar gutter drags the thumb instead of placing the caret.
    if (scrollbarVisible() && x >= rect.x + rect.w - kScrollW) {
        draggingBar_ = true;
        scrollbarDragTo(y);
        return true;
    }
    bool quick = (lastClickT_ >= 0.0f) && (clock_ - lastClickT_ < 0.45f) && (std::abs(x - lastClickX_) < 6.0f) &&
                 (std::abs(y - lastClickY_) < 6.0f);
    clickCount_ = quick ? clickCount_ + 1 : 1;
    lastClickT_ = clock_;
    lastClickX_ = x;
    lastClickY_ = y;
    blink_ = 0.0f;
    desiredX_ = -1.0f;

    std::size_t b = byteAt(x, y);
    if (clickCount_ >= 3) { // select the visual row
        std::size_t ri = rowOfByte(b);
        edit_.setSelection(rows_[ri].start, rows_[ri].end);
    } else if (clickCount_ == 2) {
        edit_.selectWordAt(b);
    } else {
        edit_.setCaret(b, shift);
    }
    return true;
}

void TextArea::onMouseDrag(float x, float y) {
    if (draggingBar_) {
        scrollbarDragTo(y);
        return;
    }
    edit_.setCaret(byteAt(x, y), /*extend=*/true);
    desiredX_ = -1.0f;
    blink_ = 0.0f;
}

bool TextArea::onMouseUp(float /*x*/, float /*y*/, int /*button*/) {
    draggingBar_ = false;
    return false;
}

bool TextArea::onWheel(float /*dx*/, float dy) {
    float viewH = rect.h - 2.0f * kPad;
    float maxScroll = std::max(0.0f, static_cast<float>(rows_.size()) * lineH() - viewH);
    if (maxScroll <= 0.0f) return false; // nothing to scroll — let a parent take it
    scrollY_ = std::clamp(scrollY_ - dy * lineH() * 2.0f, 0.0f, maxScroll);
    return true;
}

bool TextArea::onKey(Key key, bool shift, bool ctrl, bool alt) {
    bool word = ctrl || alt;
    blink_ = 0.0f;
    bool vertical = (key == Key::Up || key == Key::Down);
    if (!vertical) desiredX_ = -1.0f; // any horizontal move resets the sticky column

    switch (key) {
        case Key::Left: edit_.moveLeft(word, shift); return true;
        case Key::Right: edit_.moveRight(word, shift); return true;
        case Key::Up: moveVertical(-1, shift); return true;
        case Key::Down: moveVertical(+1, shift); return true;
        case Key::Home: { // start of the visual row
            std::size_t r = rowOfByte(edit_.caret());
            edit_.setCaret(rows_[r].start, shift);
            return true;
        }
        case Key::End: { // end of the visual row
            std::size_t r = rowOfByte(edit_.caret());
            edit_.setCaret(rows_[r].end, shift);
            return true;
        }
        case Key::Enter: edit_.insert("\n"); changed(); return true;
        case Key::Backspace: edit_.backspace(); changed(); return true;
        case Key::Delete: edit_.del(); changed(); return true;
        case Key::A:
            if (ctrl) { edit_.selectAll(); return true; }
            return false;
        case Key::C:
            if (ctrl) { edit_.copy(); return true; }
            return false;
        case Key::X:
            if (ctrl) { edit_.cut(); changed(); return true; }
            return false;
        case Key::V:
            if (ctrl) { edit_.paste(); changed(); return true; }
            return false;
        case Key::Z:
            if (ctrl) { shift ? edit_.redo() : edit_.undo(); changed(); return true; }
            return false;
        case Key::Y:
            if (ctrl) { edit_.redo(); changed(); return true; }
            return false;
        default: return false;
    }
}

void TextArea::onText(const char* utf8) {
    if (!utf8 || !utf8[0]) return;
    preedit_.clear();
    edit_.insert(utf8);
    desiredX_ = -1.0f;
    blink_ = 0.0f;
    changed();
}

void TextArea::onTextEditing(const char* utf8) { preedit_ = utf8 ? utf8 : ""; }

void TextArea::onFocusChanged(bool focusedNow) {
    if (!focusedNow) preedit_.clear();
    blink_ = 0.0f;
}

} // namespace spry
