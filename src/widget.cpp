#include "spry/widget.h"

#include <algorithm>

namespace spry {

Widget* Widget::add(std::unique_ptr<Widget> child) {
    Widget* raw = child.get();
    raw->parent_ = this;
    children_.push_back(std::move(child));
    return raw;
}

// Default: content size is the max of children's desired sizes (a stack),
// overridden by any fixed pref.
Size Widget::measure(Renderer& r, float availW, float availH) {
    float w = 0, h = 0;
    for (auto& c : children_) {
        if (!c->visible) continue;
        Size cs = c->measure(r, availW, availH);
        w = std::max(w, cs.w);
        h = std::max(h, cs.h);
    }
    return Size{prefW >= 0 ? prefW : w, prefH >= 0 ? prefH : h};
}

// Default: stack — every child fills our rect (minus its own margins).
void Widget::arrange(Renderer& r, Rect rc) {
    rect = rc;
    for (auto& c : children_) {
        if (!c->visible) continue;
        c->arrange(r, Rect{rc.x + c->margin.l, rc.y + c->margin.t, rc.w - c->margin.l - c->margin.r,
                           rc.h - c->margin.t - c->margin.b});
    }
}

void Widget::update(float dt) {
    for (auto& c : children_)
        if (c->visible) c->update(dt);
}

void Widget::draw(Renderer& r, const Theme& theme) {
    if (!visible) return;
    paint(r, theme);
    for (auto& c : children_) c->draw(r, theme);
}

Widget* Widget::hitTest(float x, float y) {
    if (!visible || !rect.contains(x, y)) return nullptr;
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (Widget* h = (*it)->hitTest(x, y)) return h;
    }
    return this;
}

void Widget::collectFocusable(std::vector<Widget*>& out) {
    if (!visible) return;
    if (focusable) out.push_back(this);
    for (auto& c : children_) c->collectFocusable(out);
}

} // namespace spry
