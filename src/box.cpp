#include "spry/box.h"

#include <algorithm>
#include <vector>

namespace spry {

Size Box::measure(Renderer& r, float availW, float availH) {
    bool col = axis == Axis::Column;
    float mainSum = 0, crossMax = 0;
    int n = 0;
    for (auto& c : children_) {
        if (!c->visible) continue;
        ++n;
        Size cs = c->measure(r, availW, availH);
        float cm = col ? cs.h : cs.w;
        float cc = col ? cs.w : cs.h;
        float pm = col ? c->prefH : c->prefW;
        if (pm >= 0) cm = pm;
        mainSum += cm + (col ? c->margin.t + c->margin.b : c->margin.l + c->margin.r);
        crossMax = std::max(crossMax, cc + (col ? c->margin.l + c->margin.r : c->margin.t + c->margin.b));
    }
    if (n > 1) mainSum += spacing * (n - 1);
    float mainTotal = mainSum + (col ? padding.t + padding.b : padding.l + padding.r);
    float crossTotal = crossMax + (col ? padding.l + padding.r : padding.t + padding.b);
    Size s = col ? Size{crossTotal, mainTotal} : Size{mainTotal, crossTotal};
    if (prefW >= 0) s.w = prefW;
    if (prefH >= 0) s.h = prefH;
    return s;
}

void Box::arrange(Renderer& r, Rect rc) {
    rect = rc;
    bool col = axis == Axis::Column;
    Rect inner{rc.x + padding.l, rc.y + padding.t, rc.w - padding.l - padding.r, rc.h - padding.t - padding.b};
    float mainExtent = col ? inner.h : inner.w;
    float crossExtent = col ? inner.w : inner.h;

    std::vector<Widget*> vis;
    for (auto& c : children_)
        if (c->visible) vis.push_back(c.get());

    std::vector<float> mainSize(vis.size(), 0.0f);
    float used = 0, growSum = 0;
    for (size_t i = 0; i < vis.size(); ++i) {
        Widget* c = vis[i];
        float pm = col ? c->prefH : c->prefW;
        float base;
        if (pm >= 0) {
            base = pm;
        } else {
            Size cs = c->measure(r, inner.w, inner.h);
            base = col ? cs.h : cs.w;
        }
        mainSize[i] = base;
        used += base + (col ? c->margin.t + c->margin.b : c->margin.l + c->margin.r);
        growSum += c->grow;
    }
    if (vis.size() > 1) used += spacing * (vis.size() - 1);
    float leftover = std::max(0.0f, mainExtent - used);
    if (growSum > 0) {
        for (size_t i = 0; i < vis.size(); ++i)
            if (vis[i]->grow > 0) mainSize[i] += leftover * (vis[i]->grow / growSum);
    }

    float cursor = col ? inner.y : inner.x;
    for (size_t i = 0; i < vis.size(); ++i) {
        Widget* c = vis[i];
        cursor += col ? c->margin.t : c->margin.l;
        float thisMain = mainSize[i];

        float marCross = col ? c->margin.l + c->margin.r : c->margin.t + c->margin.b;
        float crossAvail = crossExtent - marCross;
        float pc = col ? c->prefW : c->prefH;
        float crossSize;
        if (pc >= 0) {
            crossSize = pc;
        } else if (cross == Align::Stretch) {
            crossSize = crossAvail;
        } else {
            Size cs = c->measure(r, inner.w, inner.h);
            crossSize = col ? cs.w : cs.h;
        }
        crossSize = std::min(crossSize, crossAvail);

        float crossStart = (col ? inner.x : inner.y) + (col ? c->margin.l : c->margin.t);
        float freeCross = crossAvail - crossSize;
        if (cross == Align::Center) crossStart += freeCross * 0.5f;
        else if (cross == Align::End) crossStart += freeCross;

        Rect cr = col ? Rect{crossStart, cursor, crossSize, thisMain}
                      : Rect{cursor, crossStart, thisMain, crossSize};
        c->arrange(r, cr);
        cursor += thisMain + (col ? c->margin.b : c->margin.r) + spacing;
    }
}

} // namespace spry
