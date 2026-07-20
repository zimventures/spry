#include "spry/textfield.h"

#include <algorithm>
#include <cmath>

namespace spry {

// ---- geometry --------------------------------------------------------------
float TextField::textOriginX() const { return rect.x + kPad - scrollX_; }
float TextField::innerWidth() const { return rect.w - 2.0f * kPad; }

std::string TextField::maskOf(const std::string& s) const {
    if (!masked) return s;
    std::string out;
    for (char ch : s)
        if (((unsigned char)ch & 0xC0) != 0x80) // one bullet per UTF-8 codepoint start
            out += "\xe2\x80\xa2";              // U+2022 BULLET
    return out;
}

float TextField::xForByte(std::size_t byte) const {
    const std::string& s = edit_.text();
    if (byte == 0) return 0.0f;
    if (byte > s.size()) byte = s.size();
    if (!r_) return (float)byte * textCellW(scale); // fallback before first paint
    return r_->measureText(scale, maskOf(s.substr(0, byte)).c_str()).w;
}

std::size_t TextField::byteAtX(float x) const {
    const std::string& s = edit_.text();
    float localX = x - (rect.x + kPad) + scrollX_; // distance from the text start
    if (localX <= 0.0f) return 0;

    // Codepoint-boundary byte indices (0 .. size). Caret x is non-decreasing with
    // the index, so binary-search the boundaries by measured prefix width rather
    // than measuring every one — keeps clicks/drags off the O(n^2) shaping path.
    std::vector<std::size_t> bounds;
    bounds.reserve(s.size() + 1);
    for (std::size_t i = 0;; ) {
        bounds.push_back(i);
        if (i >= s.size()) break;
        std::size_t j = i + 1; // advance one UTF-8 codepoint
        while (j < s.size() && ((unsigned char)s[j] & 0xC0) == 0x80) ++j;
        i = j;
    }
    // Largest boundary whose caret x is <= localX.
    std::size_t lo = 0, hi = bounds.size() - 1;
    while (lo < hi) {
        std::size_t mid = (lo + hi + 1) / 2;
        if (xForByte(bounds[mid]) <= localX)
            lo = mid;
        else
            hi = mid - 1;
    }
    // Snap to whichever of the bracketing boundaries is nearer the click.
    if (lo + 1 < bounds.size()) {
        float xl = xForByte(bounds[lo]), xr = xForByte(bounds[lo + 1]);
        if (std::abs(xr - localX) < std::abs(localX - xl)) return bounds[lo + 1];
    }
    return bounds[lo];
}

void TextField::ensureCaretVisible() {
    float caretX = xForByte(edit_.caret());
    if (!preedit_.empty() && r_) caretX += r_->measureText(scale, preedit_.c_str()).w;
    float total = xForByte(edit_.text().size());
    if (!preedit_.empty() && r_) total += r_->measureText(scale, preedit_.c_str()).w;
    float iw = innerWidth();
    if (total <= iw) {
        scrollX_ = 0.0f; // everything fits — no scroll
        return;
    }
    if (caretX - scrollX_ > iw) scrollX_ = caretX - iw;
    if (caretX - scrollX_ < 0) scrollX_ = caretX;
    float maxScroll = total - iw;
    if (scrollX_ > maxScroll) scrollX_ = maxScroll;
    if (scrollX_ < 0) scrollX_ = 0;
}

void TextField::changed() {
    if (onChange) onChange(edit_.text());
}

// ---- layout / frame --------------------------------------------------------
Size TextField::measure(Renderer& r, float, float) {
    r_ = &r;
    float h = textLineH(scale) + 2.0f * kPad;
    float w = prefW >= 0 ? prefW : 220.0f; // grows along the row if grow > 0
    return Size{w, h};
}

void TextField::arrange(Renderer& r, Rect rc) {
    r_ = &r;
    Widget::arrange(r, rc);
}

void TextField::update(float dt) {
    clock_ += dt;
    blink_ += dt;
    Widget::update(dt);
}

void TextField::paint(Renderer& r, const Theme& th) {
    r_ = &r;
    float rad = th.metric(tokens::Radius, 8.0f) * 0.7f;
    float cx = rect.x + rect.w * 0.5f, cy = rect.y + rect.h * 0.5f;

    // Field background + focus border.
    Color surface = th.color(tokens::SurfaceAlt, {32, 34, 48});
    Color acc = th.color(tokens::Accent, {96, 126, 205});
    if (focused) {
        Color ring{acc.r, acc.g, acc.b, 150};
        r.fillRoundedRect(cx, cy, rect.w, rect.h, rad, ring, ring);
        r.fillRoundedRect(cx, cy, rect.w - 3, rect.h - 3, rad - 1, surface, surface);
    } else {
        Color border = hovered ? Color{acc.r, acc.g, acc.b, 90} : th.color(tokens::TextDim, {140, 144, 160});
        r.fillRoundedRect(cx, cy, rect.w, rect.h, rad, border, border);
        r.fillRoundedRect(cx, cy, rect.w - 2, rect.h - 2, rad - 1, surface, surface);
    }

    ensureCaretVisible();

    float lineH = textLineH(scale);
    float textY = rect.y + (rect.h - lineH) * 0.5f;
    float originX = textOriginX();
    const std::string& s = edit_.text();

    // Clip text/selection to the inner box so scrolled content can't spill.
    r.pushClip(Rect{rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4});

    Color textCol = th.color(tokens::Text, {226, 229, 242});
    Color dimCol = th.color(tokens::TextDim, {140, 144, 160});

    if (s.empty() && preedit_.empty() && !placeholder.empty())
        r.text(rect.x + kPad, textY, scale, dimCol, placeholder.c_str());

    float caretX; // text-space x of the caret (before applying originX)
    if (preedit_.empty()) {
        // Selection highlight behind the text.
        if (edit_.hasSelection()) {
            float x0 = xForByte(edit_.selStart()), x1 = xForByte(edit_.selEnd());
            Color sel{acc.r, acc.g, acc.b, (uint8_t)(focused ? 110 : 70)};
            r.fillRect(originX + x0, textY, x1 - x0, lineH, sel);
        }
        if (!s.empty()) r.text(originX, textY, scale, textCol, maskOf(s).c_str());
        caretX = xForByte(edit_.caret());
    } else {
        // IME composition: left | preedit (underlined) | right; caret after preedit.
        std::string left = maskOf(s.substr(0, edit_.caret()));
        std::string right = maskOf(s.substr(edit_.caret()));
        float wLeft = xForByte(edit_.caret());
        float wPre = r.measureText(scale, preedit_.c_str()).w;
        if (!left.empty()) r.text(originX, textY, scale, textCol, left.c_str());
        r.text(originX + wLeft, textY, scale, textCol, preedit_.c_str());
        r.fillRect(originX + wLeft, textY + lineH - 3.0f, wPre, 1.5f, acc); // composition underline
        if (!right.empty()) r.text(originX + wLeft + wPre, textY, scale, textCol, right.c_str());
        caretX = wLeft + wPre;
    }

    // Blinking caret (steady while composing / right after an edit).
    bool caretOn = focused && (!preedit_.empty() || std::fmod(blink_, 1.0f) < 0.5f);
    float cxp = originX + caretX;
    if (caretOn) r.fillRect(cxp, textY + 2.0f, 1.5f, lineH - 4.0f, textCol);

    r.popClip();

    // Report the caret rect (unclipped, Spry coords) for the IME candidate window.
    caretRect_ = Rect{cxp, textY, 2.0f, lineH};
}

// ---- input -----------------------------------------------------------------
bool TextField::onMouseDown(float x, float y, int /*button*/, bool shift, bool /*ctrl*/) {
    // Successive quick clicks at ~the same spot escalate: 1=caret, 2=word, 3=line.
    bool quick = (lastClickT_ >= 0.0f) && (clock_ - lastClickT_ < 0.45f) &&
                 (std::abs(x - lastClickX_) < 6.0f);
    clickCount_ = quick ? clickCount_ + 1 : 1;
    lastClickT_ = clock_;
    lastClickX_ = x;
    blink_ = 0.0f;

    std::size_t b = byteAtX(x);
    if (clickCount_ >= 3) {
        selMode_ = SelMode::Line; // single-line field: the line is the whole text
        edit_.selectAll();
    } else if (clickCount_ == 2) {
        selMode_ = SelMode::Word;
        edit_.selectWordAt(b);
        wordLo_ = edit_.selStart();
        wordHi_ = edit_.selEnd();
    } else {
        selMode_ = SelMode::Char;
        edit_.setCaret(b, shift);
    }
    (void)y;
    return true;
}

void TextField::onMouseDrag(float x, float y) {
    // Extend at the granularity the drag began with — Context drives this every
    // frame while the button is held, so a word/line selection must persist.
    if (selMode_ == SelMode::Line) {
        edit_.selectAll();
    } else if (selMode_ == SelMode::Word) {
        std::size_t lo, hi;
        edit_.wordBoundsAt(byteAtX(x), lo, hi);
        std::size_t selLo = std::min(wordLo_, lo), selHi = std::max(wordHi_, hi);
        // Put the caret on the end the pointer is dragging toward.
        if (lo >= wordHi_)
            edit_.setSelection(selLo, selHi);
        else
            edit_.setSelection(selHi, selLo);
    } else {
        edit_.setCaret(byteAtX(x), /*extend=*/true);
    }
    blink_ = 0.0f;
    (void)y;
}

bool TextField::onKey(Key key, bool shift, bool ctrl, bool alt) {
    bool word = ctrl || alt; // Ctrl (Win/Linux) or Alt (macOS) = move by word
    blink_ = 0.0f;
    switch (key) {
        case Key::Left: edit_.moveLeft(word, shift); return true;
        case Key::Right: edit_.moveRight(word, shift); return true;
        case Key::Home: edit_.moveHome(shift); return true;
        case Key::End: edit_.moveEnd(shift); return true;
        case Key::Backspace: edit_.backspace(); changed(); return true;
        case Key::Delete: edit_.del(); changed(); return true;
        case Key::Enter:
            if (onSubmit) onSubmit();
            return true;
        case Key::A:
            if (ctrl) {
                edit_.selectAll();
                return true;
            }
            return false;
        case Key::C:
            if (ctrl) {
                edit_.copy();
                return true;
            }
            return false;
        case Key::X:
            if (ctrl) {
                edit_.cut();
                changed();
                return true;
            }
            return false;
        case Key::V:
            if (ctrl) {
                edit_.paste();
                changed();
                return true;
            }
            return false;
        case Key::Z:
            if (ctrl) {
                if (shift)
                    edit_.redo();
                else
                    edit_.undo();
                changed();
                return true;
            }
            return false;
        case Key::Y:
            if (ctrl) {
                edit_.redo();
                changed();
                return true;
            }
            return false;
        default: return false;
    }
}

void TextField::onText(const char* utf8) {
    if (!utf8 || !utf8[0]) return;
    preedit_.clear(); // committed text supersedes any composition
    edit_.insert(utf8);
    blink_ = 0.0f;
    changed();
}

void TextField::onTextEditing(const char* utf8) { preedit_ = utf8 ? utf8 : ""; }

void TextField::onFocusChanged(bool focusedNow) {
    if (!focusedNow) preedit_.clear();
    blink_ = 0.0f;
}

} // namespace spry
