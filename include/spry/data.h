#pragma once
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "anim.h"
#include "renderer.h"
#include "theme.h"
#include "widget.h"

// Data / container widgets (#215): the chrome Cleat's views need. A virtualized
// list base (only visible rows are drawn) powers ListView, Table (sortable), and
// TreeView; plus a generic ScrollView for arbitrary content and a TabBar. All
// scroll with proper clipping via the renderer clip stack (#213) and support
// keyboard + mouse selection.
namespace spry {

// ---- VirtualList -----------------------------------------------------------
// Base for row-oriented, vertically-scrolled widgets. Subclasses supply a row
// count + a per-row painter; the base owns scroll, clipping, the scrollbar, wheel
// + keyboard navigation, and click selection. Rows are never materialized as
// widgets, so 100k rows cost the same as a screenful.
class VirtualList : public Widget {
public:
    float rowHeight = 26.0f;
    int selected = -1;              // the lead / active row (also the single selection)
    bool multiSelect = false;       // enable shift-range + ctrl-toggle selection
    std::function<void(int)> onSelect; // fired with the lead row whenever selection changes

    VirtualList() { focusable = true; }

    int numRows() const { return rowCount(); } // public view of the row count

    // Multi-selection (#215). With multiSelect off, only the lead row is ever in
    // the set, so isSelected(selected) and selection() == {selected}.
    bool isSelected(int i) const { return sel_.count(i) > 0; }
    std::vector<int> selection() const { return std::vector<int>(sel_.begin(), sel_.end()); }
    void clearSelection() {
        sel_.clear();
        selected = -1;
        anchor_ = -1;
    }

    bool onWheel(float, float dy) override {
        scrollBy(-dy * rowHeight * 3.0f); // ~3 rows per notch
        return true;
    }
    bool onMouseDown(float x, float y, int, bool shift, bool ctrl) override {
        if (scrollbarVisible() && x >= rect.x + rect.w - kScrollW) {
            sbDrag_ = true;
            scrollToThumb(y);
            return true;
        }
        if (headerHeight() > 0.0f && y < rect.y + headerHeight()) {
            headerClick(x - rect.x);
            return true;
        }
        int row = rowAtY(y);
        if (row >= 0 && row < rowCount()) {
            if (!rowMouseDown(row, x - bodyViewport().x, shift)) {
                if (multiSelect && shift)
                    selectTo(row); // contiguous range from the anchor
                else if (multiSelect && ctrl)
                    toggleSelect(row); // add/remove this row
                else
                    selectSingle(row);
            }
        }
        return true;
    }
    bool onMouseUp(float, float, int) override {
        sbDrag_ = false;
        return false;
    }
    void onMouseDrag(float, float y) override {
        if (sbDrag_) scrollToThumb(y);
    }
    bool onKey(Key key, bool shift, bool ctrl, bool) override {
        if (key == Key::Up) {
            moveSel(-1, shift);
            return true;
        }
        if (key == Key::Down) {
            moveSel(1, shift);
            return true;
        }
        if (multiSelect && ctrl && key == Key::A) {
            selectAll();
            return true;
        }
        return false;
    }

    void paint(Renderer& r, const Theme& th) override {
        float rad = th.metric("radius", 8.0f) * 0.6f;
        r.fillRoundedRect(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, rect.w, rect.h, rad,
                          th.color("surfaceAlt", {32, 34, 48}), th.color("surface", {40, 43, 62}));
        if (headerHeight() > 0.0f)
            paintHeader(r, th, Rect{rect.x, rect.y, rect.w - kScrollW, headerHeight()});

        Rect vp = bodyViewport();
        clampScroll(vp);
        r.pushClip(vp);
        int n = rowCount();
        int first = std::max(0, (int)(scrollY_ / rowHeight));
        int last = std::min(n, (int)((scrollY_ + vp.h) / rowHeight) + 1);
        for (int i = first; i < last; ++i) {
            Rect rr{vp.x, vp.y + (float)i * rowHeight - scrollY_, vp.w, rowHeight};
            drawRow(r, th, i, rr, isSelected(i), false);
            if (multiSelect && i == selected) { // a thin ring marks the keyboard lead row
                Color a = th.color("accent", {96, 126, 205});
                r.fillRect(rr.x, rr.y, rr.w, 1.0f, a);
                r.fillRect(rr.x, rr.y + rr.h - 1.0f, rr.w, 1.0f, a);
            }
        }
        r.popClip();
        paintScrollbar(r, th, vp);
    }

protected:
    virtual int rowCount() const = 0;
    virtual void drawRow(Renderer&, const Theme&, int index, Rect rowRect, bool selected, bool hovered) = 0;
    virtual float headerHeight() const { return 0.0f; }
    virtual void paintHeader(Renderer&, const Theme&, Rect) {}
    virtual void headerClick(float /*localX*/) {}
    // Return true to consume the click (e.g. a tree toggle) and suppress selection.
    virtual bool rowMouseDown(int /*row*/, float /*localX*/, bool /*shift*/) { return false; }

    Rect bodyViewport() const {
        float hh = headerHeight();
        return Rect{rect.x, rect.y + hh, rect.w - kScrollW, rect.h - hh};
    }
    float contentHeight() const { return (float)rowCount() * rowHeight; }
    bool scrollbarVisible() const { return contentHeight() > bodyViewport().h; }
    float scrollY() const { return scrollY_; }

    static constexpr float kScrollW = 12.0f;

private:
    int rowAtY(float y) const {
        Rect vp = bodyViewport();
        if (y < vp.y || y > vp.y + vp.h) return -1;
        return (int)((y - vp.y + scrollY_) / rowHeight);
    }
    void scrollBy(float dy) {
        scrollY_ += dy;
        clampScroll(bodyViewport());
    }
    void clampScroll(Rect vp) {
        float maxS = std::max(0.0f, contentHeight() - vp.h);
        scrollY_ = std::max(0.0f, std::min(scrollY_, maxS));
    }
    void moveSel(int d, bool extend) {
        int n = rowCount();
        if (n == 0) return;
        int lead = selected < 0 ? (d > 0 ? 0 : n - 1) : selected + d;
        lead = std::max(0, std::min(lead, n - 1));
        if (extend && multiSelect)
            selectTo(lead);
        else
            selectSingle(lead);
        ensureVisible(lead);
    }
    void selectSingle(int row) {
        sel_.clear();
        sel_.insert(row);
        selected = row;
        anchor_ = row;
        if (onSelect) onSelect(row);
    }
    void toggleSelect(int row) {
        if (sel_.count(row))
            sel_.erase(row);
        else
            sel_.insert(row);
        selected = row;
        anchor_ = row;
        if (onSelect) onSelect(row);
    }
    void selectTo(int row) { // contiguous range [anchor_, row], replacing the set
        if (anchor_ < 0) anchor_ = row;
        sel_.clear();
        for (int i = std::min(anchor_, row); i <= std::max(anchor_, row); ++i) sel_.insert(i);
        selected = row;
        if (onSelect) onSelect(row);
    }
    void selectAll() {
        sel_.clear();
        int n = rowCount();
        for (int i = 0; i < n; ++i) sel_.insert(i);
        if (n > 0) {
            selected = n - 1;
            if (onSelect) onSelect(selected);
        }
    }
    void ensureVisible(int row) {
        Rect vp = bodyViewport();
        float top = (float)row * rowHeight, bot = top + rowHeight;
        if (top < scrollY_) scrollY_ = top;
        if (bot > scrollY_ + vp.h) scrollY_ = bot - vp.h;
        clampScroll(vp);
    }
    void scrollToThumb(float y) {
        Rect vp = bodyViewport();
        float thumbH = std::max(kMinThumb, vp.h * vp.h / contentHeight());
        float f = (vp.h - thumbH) > 0 ? (y - vp.y - thumbH * 0.5f) / (vp.h - thumbH) : 0.0f;
        f = std::max(0.0f, std::min(1.0f, f));
        scrollY_ = f * std::max(0.0f, contentHeight() - vp.h);
        clampScroll(vp);
    }
    void paintScrollbar(Renderer& r, const Theme& th, Rect vp) {
        if (!scrollbarVisible()) return;
        float ch = contentHeight();
        float thumbH = std::max(kMinThumb, vp.h * vp.h / ch);
        float maxS = ch - vp.h;
        float t = maxS > 0 ? scrollY_ / maxS : 0.0f;
        float ty = vp.y + t * (vp.h - thumbH);
        float sx = rect.x + rect.w - kScrollW * 0.5f - 2.0f;
        Color c = th.color("textDim", {140, 144, 160});
        r.fillRoundedRect(sx, ty + thumbH * 0.5f, kScrollW - 6.0f, thumbH, (kScrollW - 6.0f) * 0.5f, c, c);
    }

    static constexpr float kMinThumb = 28.0f;
    float scrollY_ = 0.0f;
    bool sbDrag_ = false;
    std::set<int> sel_; // selected rows (lead row mirrored in `selected`)
    int anchor_ = -1;   // range anchor for shift-select
};

// ---- ListView --------------------------------------------------------------
class ListView : public VirtualList {
public:
    std::vector<std::string> items;
    float scale = 1.4f;

protected:
    int rowCount() const override { return (int)items.size(); }
    void drawRow(Renderer& r, const Theme& th, int i, Rect rr, bool sel, bool) override {
        if (sel) {
            Color a = th.color("accent", {96, 126, 205});
            r.fillRect(rr.x, rr.y, rr.w, rr.h, Color{a.r, a.g, a.b, 80});
        }
        r.text(rr.x + 10.0f, rr.y + (rr.h - textLineH(scale)) * 0.5f, scale, th.color("text", {226, 229, 242}),
               items[i].c_str());
    }
};

// ---- Table -----------------------------------------------------------------
struct Column {
    std::string title;
    float weight = 1.0f; // share of the body width
};

// A sortable data table. Cells are row-major strings; clicking a header sorts by
// that column (numeric when both cells parse as numbers, else lexicographic) and
// toggles direction. Rows are virtualized via VirtualList; selection is by the
// displayed row.
class Table : public VirtualList {
public:
    std::vector<Column> columns;
    std::vector<std::vector<std::string>> rows;
    float scale = 1.3f;
    int sortCol = -1;
    bool sortAsc = true;

    Table() { rowHeight = 28.0f; }

    // Data row backing the i-th displayed row (sorting permutes the order).
    int dataRow(int displayRow) const {
        return (displayRow >= 0 && displayRow < (int)order_.size()) ? order_[displayRow] : displayRow;
    }

protected:
    int rowCount() const override {
        syncOrder();
        return (int)rows.size();
    }
    float headerHeight() const override { return 30.0f; }

    void paintHeader(Renderer& r, const Theme& th, Rect hr) override {
        r.fillRect(hr.x, hr.y, hr.w, hr.h, th.color("surface", {40, 43, 62}));
        float x = hr.x;
        float total = totalWeight();
        for (int c = 0; c < (int)columns.size(); ++c) {
            float cw = hr.w * (columns[c].weight / total);
            std::string t = columns[c].title;
            if (c == sortCol) t += sortAsc ? "  ^" : "  v";
            r.pushClip(Rect{x, hr.y, cw, hr.h});
            r.text(x + 10.0f, hr.y + (hr.h - textLineH(scale)) * 0.5f, scale, th.color("textDim", {150, 154, 170}),
                   t.c_str());
            r.popClip();
            x += cw;
        }
        Color line = th.color("textDim", {90, 94, 110});
        r.fillRect(hr.x, hr.y + hr.h - 1.0f, hr.w, 1.0f, Color{line.r, line.g, line.b, 120});
    }
    void headerClick(float localX) override {
        float total = totalWeight();
        float bodyW = rect.w - kScrollW;
        float x = 0.0f;
        for (int c = 0; c < (int)columns.size(); ++c) {
            float cw = bodyW * (columns[c].weight / total);
            if (localX >= x && localX < x + cw) {
                if (sortCol == c)
                    sortAsc = !sortAsc;
                else {
                    sortCol = c;
                    sortAsc = true;
                }
                resort();
                return;
            }
            x += cw;
        }
    }
    void drawRow(Renderer& r, const Theme& th, int i, Rect rr, bool sel, bool hov) override {
        if (sel) {
            Color a = th.color("accent", {96, 126, 205});
            r.fillRect(rr.x, rr.y, rr.w, rr.h, Color{a.r, a.g, a.b, 90});
        } else if (i % 2) {
            r.fillRect(rr.x, rr.y, rr.w, rr.h, Color{255, 255, 255, 6}); // zebra striping
        }
        (void)hov;
        const auto& row = rows[order_[i]];
        float total = totalWeight();
        float x = rr.x;
        for (int c = 0; c < (int)columns.size(); ++c) {
            float cw = rr.w * (columns[c].weight / total);
            const char* cell = c < (int)row.size() ? row[c].c_str() : "";
            r.pushClip(Rect{x, rr.y, cw - 4.0f, rr.h});
            r.text(x + 10.0f, rr.y + (rr.h - textLineH(scale)) * 0.5f, scale, th.color("text", {226, 229, 242}),
                   cell);
            r.popClip();
            x += cw;
        }
    }

private:
    float totalWeight() const {
        float t = 0.0f;
        for (const auto& c : columns) t += c.weight;
        return t > 0 ? t : 1.0f;
    }
    // Keep the order map sized to the data; (re)build identity when row count drifts.
    void syncOrder() const {
        if (order_.size() != rows.size()) {
            order_.resize(rows.size());
            for (int i = 0; i < (int)order_.size(); ++i) order_[i] = i;
            if (sortCol >= 0) const_cast<Table*>(this)->resort();
        }
    }
    void resort() {
        if (sortCol < 0) return;
        if (order_.size() != rows.size()) { // ensure the order map exists before sorting
            order_.resize(rows.size());
            for (int i = 0; i < (int)order_.size(); ++i) order_[i] = i;
        }
        int col = sortCol;
        bool asc = sortAsc;
        const auto& data = rows;
        std::sort(order_.begin(), order_.end(), [&](int a, int b) {
            const std::string& sa = col < (int)data[a].size() ? data[a][col] : kEmpty;
            const std::string& sb = col < (int)data[b].size() ? data[b][col] : kEmpty;
            bool less = compareCells(sa, sb);
            bool eq = !less && !compareCells(sb, sa);
            if (eq) return a < b; // stable on ties
            return asc ? less : !less;
        });
    }
    static bool compareCells(const std::string& a, const std::string& b) {
        char* ea = nullptr;
        char* eb = nullptr;
        double na = std::strtod(a.c_str(), &ea);
        double nb = std::strtod(b.c_str(), &eb);
        bool aNum = ea != a.c_str() && *ea == '\0';
        bool bNum = eb != b.c_str() && *eb == '\0';
        if (aNum && bNum) return na < nb;
        return a < b;
    }

    static inline const std::string kEmpty{};
    mutable std::vector<int> order_; // display row -> data row
};

// ---- TreeView --------------------------------------------------------------
struct TreeNode {
    std::string label;
    bool expanded = false;
    std::vector<std::unique_ptr<TreeNode>> children;

    explicit TreeNode(std::string l = {}) : label(std::move(l)) {}
    TreeNode& add(std::string l) {
        children.push_back(std::make_unique<TreeNode>(std::move(l)));
        return *children.back();
    }
};

// A hierarchical list. Expanded nodes are flattened into the virtualized row set;
// clicking the disclosure triangle toggles a node, clicking elsewhere selects it.
class TreeView : public VirtualList {
public:
    std::vector<std::unique_ptr<TreeNode>> roots;
    float scale = 1.4f;

    TreeNode& addRoot(std::string label) {
        roots.push_back(std::make_unique<TreeNode>(std::move(label)));
        return *roots.back();
    }
    void rebuild() {
        flat_.clear();
        for (auto& r : roots) flatten(r.get(), 0);
    }

    // Right expands the highlighted branch, Left collapses it (when it has
    // children); Up/Down fall through to the base list navigation.
    bool onKey(Key key, bool shift, bool ctrl, bool alt) override {
        if ((key == Key::Right || key == Key::Left) && selected >= 0 && selected < (int)flat_.size()) {
            TreeNode* n = flat_[selected].node;
            if (!n->children.empty()) {
                bool open = key == Key::Right;
                if (n->expanded != open) {
                    n->expanded = open;
                    rebuild();
                }
                return true;
            }
        }
        return VirtualList::onKey(key, shift, ctrl, alt);
    }

protected:
    int rowCount() const override {
        if (flat_.empty() && !roots.empty()) const_cast<TreeView*>(this)->rebuild();
        return (int)flat_.size();
    }
    bool rowMouseDown(int row, float localX, bool) override {
        const Flat& f = flat_[row];
        float arrowX = kPad + (float)f.depth * kIndent;
        if (!f.node->children.empty() && localX >= arrowX && localX < arrowX + kIndent) {
            f.node->expanded = !f.node->expanded;
            rebuild();
            return true; // consumed: toggle, not select
        }
        return false;
    }
    void drawRow(Renderer& r, const Theme& th, int i, Rect rr, bool sel, bool) override {
        const Flat& f = flat_[i];
        if (sel) {
            Color a = th.color("accent", {96, 126, 205});
            r.fillRect(rr.x, rr.y, rr.w, rr.h, Color{a.r, a.g, a.b, 80});
        }
        float ax = rr.x + kPad + (float)f.depth * kIndent;
        float cy = rr.y + rr.h * 0.5f;
        if (!f.node->children.empty()) drawTriangle(r, ax + kIndent * 0.5f, cy, f.node->expanded, th);
        r.text(ax + kIndent + 4.0f, rr.y + (rr.h - textLineH(scale)) * 0.5f, scale,
               th.color("text", {226, 229, 242}), f.node->label.c_str());
    }

private:
    struct Flat {
        TreeNode* node;
        int depth;
    };
    void flatten(TreeNode* n, int depth) {
        flat_.push_back({n, depth});
        if (n->expanded)
            for (auto& c : n->children) flatten(c.get(), depth + 1);
    }
    static void drawTriangle(Renderer& r, float cx, float cy, bool expanded, const Theme& th) {
        Color c = th.color("textDim", {150, 154, 170});
        float s = 4.0f;
        std::vector<Vertex> v;
        if (expanded)
            v = {{cx - s, cy - s, c}, {cx + s, cy - s, c}, {cx, cy + s, c}}; // pointing down
        else
            v = {{cx - s, cy - s, c}, {cx + s, cy, c}, {cx - s, cy + s, c}}; // pointing right
        std::vector<int> idx{0, 1, 2};
        r.fillMesh(v, idx);
    }

    static constexpr float kIndent = 18.0f;
    static constexpr float kPad = 6.0f;
    std::vector<Flat> flat_;
};

// ---- ScrollView ------------------------------------------------------------
// Scrolls a single content child (arbitrary widget tree), clipping it to the
// viewport. For row data prefer the virtualized widgets above; this is for things
// like a tall settings panel.
class ScrollView : public Widget {
public:
    Widget* setContent(std::unique_ptr<Widget> c) {
        content_ = c.get();
        children_.clear();
        return add(std::move(c));
    }

    bool onWheel(float, float dy) override {
        scrollY_ -= dy * 48.0f;
        clamp();
        return true;
    }
    bool onMouseDown(float x, float y, int, bool, bool) override {
        if (scrollbarVisible() && x >= rect.x + rect.w - kScrollW) {
            sbDrag_ = true;
            scrollToThumb(y);
            return true;
        }
        return false; // let the hit child handle it
    }
    bool onMouseUp(float, float, int) override {
        sbDrag_ = false;
        return false;
    }
    void onMouseDrag(float, float y) override {
        if (sbDrag_) scrollToThumb(y);
    }

    void arrange(Renderer& r, Rect rc) override {
        rect = rc;
        if (!content_) return;
        Size cs = content_->measure(r, rc.w - kScrollW, rc.h);
        contentH_ = cs.h;
        clamp();
        content_->arrange(r, Rect{rc.x, rc.y - scrollY_, rc.w - kScrollW, cs.h});
    }
    void draw(Renderer& r, const Theme& th) override {
        if (!visible) return;
        paint(r, th);
        Rect vp{rect.x, rect.y, rect.w - kScrollW, rect.h};
        r.pushClip(vp);
        for (auto& c : children_) c->draw(r, th);
        r.popClip();
        paintScrollbar(r, th);
    }

private:
    bool scrollbarVisible() const { return contentH_ > rect.h; }
    void clamp() { scrollY_ = std::max(0.0f, std::min(scrollY_, std::max(0.0f, contentH_ - rect.h))); }
    void scrollToThumb(float y) {
        float thumbH = std::max(28.0f, rect.h * rect.h / contentH_);
        float f = (rect.h - thumbH) > 0 ? (y - rect.y - thumbH * 0.5f) / (rect.h - thumbH) : 0.0f;
        f = std::max(0.0f, std::min(1.0f, f));
        scrollY_ = f * std::max(0.0f, contentH_ - rect.h);
    }
    void paintScrollbar(Renderer& r, const Theme& th) {
        if (!scrollbarVisible()) return;
        float thumbH = std::max(28.0f, rect.h * rect.h / contentH_);
        float maxS = contentH_ - rect.h;
        float t = maxS > 0 ? scrollY_ / maxS : 0.0f;
        float ty = rect.y + t * (rect.h - thumbH);
        float sx = rect.x + rect.w - kScrollW * 0.5f - 2.0f;
        Color c = th.color("textDim", {140, 144, 160});
        r.fillRoundedRect(sx, ty + thumbH * 0.5f, kScrollW - 6.0f, thumbH, (kScrollW - 6.0f) * 0.5f, c, c);
    }

    static constexpr float kScrollW = 12.0f;
    Widget* content_ = nullptr;
    float scrollY_ = 0.0f;
    float contentH_ = 0.0f;
    bool sbDrag_ = false;
};

// ---- TabBar ----------------------------------------------------------------
// A horizontal row of tabs with an animated active indicator. Click or
// Left/Right (when focused) to switch; onChange reports the new index.
class TabBar : public Widget {
public:
    std::vector<std::string> tabs;
    int active = 0;
    std::function<void(int)> onChange;
    float scale = 1.4f;

    TabBar() { focusable = true; }

    Size measure(Renderer& r, float, float) override {
        float w = 0.0f;
        for (auto& t : tabs) w += tabWidth(r, t);
        return Size{w, textLineH(scale) + 18.0f};
    }
    bool onMouseDown(float x, float, int, bool, bool) override {
        Renderer* rr = lastR_;
        if (!rr) return true;
        float tx = rect.x;
        for (int i = 0; i < (int)tabs.size(); ++i) {
            float tw = tabWidth(*rr, tabs[i]);
            if (x >= tx && x < tx + tw) {
                setActive(i);
                break;
            }
            tx += tw;
        }
        return true;
    }
    bool onKey(Key key, bool, bool, bool) override {
        if (key == Key::Left) {
            setActive(active - 1);
            return true;
        }
        if (key == Key::Right) {
            setActive(active + 1);
            return true;
        }
        return false;
    }
    void update(float dt) override {
        indX_.step(dt);
        indW_.step(dt);
        Widget::update(dt);
    }
    void paint(Renderer& r, const Theme& th) override {
        lastR_ = &r;
        Color textC = th.color("text", {226, 229, 242});
        Color dim = th.color("textDim", {150, 154, 170});
        Color acc = th.color("accent", {96, 126, 205});
        float tx = rect.x;
        float activeX = rect.x, activeW = 0.0f;
        for (int i = 0; i < (int)tabs.size(); ++i) {
            float tw = tabWidth(r, tabs[i]);
            bool on = i == active;
            r.text(tx + 14.0f, rect.y + (rect.h - textLineH(scale)) * 0.5f, scale, on ? textC : dim,
                   tabs[i].c_str());
            if (on) {
                activeX = tx;
                activeW = tw;
            }
            tx += tw;
        }
        // Initialize/seek the indicator springs to the active tab.
        if (indW_.target != activeW) {
            indX_.target = activeX;
            indW_.target = activeW;
            if (indW_.value == 0.0f) { // first layout: no slide-in from zero
                indX_.value = activeX;
                indW_.value = activeW;
            }
        }
        r.fillRoundedRect(indX_.value + indW_.value * 0.5f, rect.y + rect.h - 3.0f, indW_.value - 20.0f, 3.0f,
                          1.5f, acc, acc);
    }

private:
    void setActive(int i) {
        if (tabs.empty()) return;
        i = std::max(0, std::min(i, (int)tabs.size() - 1));
        if (i == active) return;
        active = i;
        if (onChange) onChange(active);
    }
    float tabWidth(Renderer& r, const std::string& t) const { return r.measureText(scale, t.c_str()).w + 28.0f; }

    Spring indX_, indW_;
    Renderer* lastR_ = nullptr;
};

} // namespace spry
