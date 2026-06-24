// Headless tests for the data/container widgets (#215): list/table/tree selection
// + sorting + expansion, tab switching, and scroll (virtualized and generic),
// driven through Context with a stub renderer for deterministic text metrics.
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>
#include <string>

#include "spry/spry.h"

using namespace spry;

namespace {

struct StubRenderer : Renderer {
    void beginFrame(Color) override {}
    void endFrame() override {}
    void outputSize(int& w, int& h) override {
        w = 800;
        h = 600;
    }
    void fillMesh(const std::vector<Vertex>&, const std::vector<int>&) override {}
    void fillRoundedRect(float, float, float, float, float, Color, Color) override {}
    void fillRect(float, float, float, float, Color) override {}
    void text(float, float, float, Color, const char*) override {}
    Size measureText(float scale, const char* s) override {
        return Size{(float)std::strlen(s) * 7.0f * scale, 13.0f * scale + 7.0f};
    }
};

void clickAt(Context& ctx, float x, float y, bool shift = false, bool ctrl = false) {
    InputEvent d;
    d.type = InputEvent::MouseDown;
    d.x = x;
    d.y = y;
    d.shift = shift;
    d.ctrl = ctrl;
    ctx.handleEvent(d);
    InputEvent u = d;
    u.type = InputEvent::MouseUp;
    ctx.handleEvent(u);
}

void wheelAt(Context& ctx, float x, float y, float notches) {
    InputEvent w;
    w.type = InputEvent::Wheel;
    w.x = x;
    w.y = y;
    w.wheel = notches;
    ctx.handleEvent(w);
}

void key(Context& ctx, Key k, bool shift = false, bool ctrl = false) {
    InputEvent e;
    e.type = InputEvent::KeyDown;
    e.key = k;
    e.shift = shift;
    e.ctrl = ctrl;
    ctx.handleEvent(e);
}

} // namespace

TEST_CASE("ListView selects by click and keyboard") {
    StubRenderer r;
    Context ctx;
    auto lv = std::make_unique<ListView>();
    ListView* l = lv.get();
    l->rowHeight = 20.0f;
    for (int i = 0; i < 100; ++i) l->items.push_back("item " + std::to_string(i));
    ctx.setRoot(std::move(lv));
    ctx.frame(r, 0.016f, -1, -1);

    clickAt(ctx, l->rect.x + 20, l->rect.y + 10); // first visible row
    REQUIRE(l->selected == 0);
    key(ctx, Key::Down);
    key(ctx, Key::Down);
    REQUIRE(l->selected == 2);
}

TEST_CASE("ListView wheel scrolls the virtualized rows") {
    StubRenderer r;
    Context ctx;
    auto lv = std::make_unique<ListView>();
    ListView* l = lv.get();
    l->rowHeight = 20.0f;
    for (int i = 0; i < 100; ++i) l->items.push_back("item " + std::to_string(i));
    ctx.setRoot(std::move(lv));
    ctx.frame(r, 0.016f, -1, -1);

    float x = l->rect.x + 20, y = l->rect.y + 10;
    clickAt(ctx, x, y);
    REQUIRE(l->selected == 0);
    wheelAt(ctx, x, y, -3.0f); // scroll down
    clickAt(ctx, x, y);        // same screen position now lands on a later row
    REQUIRE(l->selected > 0);
}

TEST_CASE("ListView multi-select: ctrl toggles, shift ranges, ctrl+A all") {
    StubRenderer r;
    Context ctx;
    auto lv = std::make_unique<ListView>();
    ListView* l = lv.get();
    l->rowHeight = 20.0f;
    l->multiSelect = true;
    for (int i = 0; i < 50; ++i) l->items.push_back("item " + std::to_string(i));
    ctx.setRoot(std::move(lv));
    ctx.frame(r, 0.016f, -1, -1);

    auto rowY = [&](int i) { return l->rect.y + i * l->rowHeight + 10.0f; };
    float x = l->rect.x + 20.0f;

    clickAt(ctx, x, rowY(1));                       // select row 1
    REQUIRE(l->selection() == std::vector<int>{1});
    clickAt(ctx, x, rowY(3), false, true);          // ctrl-click row 3 -> {1,3}
    REQUIRE(l->selection() == std::vector<int>{1, 3});
    clickAt(ctx, x, rowY(3), false, true);          // ctrl-click row 3 again -> deselect
    REQUIRE(l->selection() == std::vector<int>{1});

    clickAt(ctx, x, rowY(2));                        // plain click resets to {2}
    clickAt(ctx, x, rowY(5), true, false);          // shift-click row 5 -> 2..5
    REQUIRE(l->selection() == std::vector<int>{2, 3, 4, 5});

    ctx.setFocus(l);
    key(ctx, Key::A, false, true); // ctrl+A selects all
    REQUIRE((int)l->selection().size() == 50);
}

TEST_CASE("ListView shift+arrow extends the selection") {
    StubRenderer r;
    Context ctx;
    auto lv = std::make_unique<ListView>();
    ListView* l = lv.get();
    l->rowHeight = 20.0f;
    l->multiSelect = true;
    for (int i = 0; i < 50; ++i) l->items.push_back("item " + std::to_string(i));
    ctx.setRoot(std::move(lv));
    ctx.frame(r, 0.016f, -1, -1);

    clickAt(ctx, l->rect.x + 20.0f, l->rect.y + 10.0f); // anchor at row 0
    ctx.setFocus(l);
    key(ctx, Key::Down, true);
    key(ctx, Key::Down, true);
    REQUIRE(l->selection() == std::vector<int>{0, 1, 2});
    REQUIRE(l->selected == 2);
    key(ctx, Key::Down); // plain Down collapses back to a single selection
    REQUIRE(l->selection() == std::vector<int>{3});
}

TEST_CASE("Single-select (default) keeps just one row") {
    StubRenderer r;
    Context ctx;
    auto lv = std::make_unique<ListView>();
    ListView* l = lv.get();
    l->rowHeight = 20.0f; // multiSelect stays false
    for (int i = 0; i < 50; ++i) l->items.push_back("item " + std::to_string(i));
    ctx.setRoot(std::move(lv));
    ctx.frame(r, 0.016f, -1, -1);

    float x = l->rect.x + 20.0f;
    clickAt(ctx, x, l->rect.y + 1 * l->rowHeight + 10.0f);
    clickAt(ctx, x, l->rect.y + 3 * l->rowHeight + 10.0f, false, true); // ctrl ignored
    REQUIRE(l->selection() == std::vector<int>{3});
    REQUIRE(l->selected == 3);
}

TEST_CASE("Table sorts by a clicked column, numerically, toggling direction") {
    StubRenderer r;
    Context ctx;
    auto tb = std::make_unique<Table>();
    Table* t = tb.get();
    t->columns = {{"Name", 1.0f}, {"Size", 1.0f}};
    t->rows = {{"bob", "30"}, {"amy", "10"}, {"cy", "20"}};
    ctx.setRoot(std::move(tb));
    ctx.frame(r, 0.016f, -1, -1);

    float hx = t->rect.x + t->rect.w * 0.75f; // within the "Size" column header
    float hy = t->rect.y + 10.0f;
    clickAt(ctx, hx, hy); // ascending
    REQUIRE(t->dataRow(0) == 1); // amy / 10
    clickAt(ctx, hx, hy); // toggle to descending
    REQUIRE(t->dataRow(0) == 0); // bob / 30
}

TEST_CASE("TreeView expands/collapses and selects") {
    StubRenderer r;
    Context ctx;
    auto tv = std::make_unique<TreeView>();
    TreeView* t = tv.get();
    TreeNode& a = t->addRoot("A");
    a.add("a1");
    a.add("a2");
    t->addRoot("B");
    ctx.setRoot(std::move(tv));
    ctx.frame(r, 0.016f, -1, -1);

    REQUIRE(t->numRows() == 2); // A (collapsed) + B

    // Click A's disclosure triangle (row 0, inside the arrow column).
    float ay = t->rect.y + t->rowHeight * 0.5f;
    float ax = t->rect.x + 6.0f + 18.0f * 0.5f;
    clickAt(ctx, ax, ay);
    REQUIRE(t->numRows() == 4); // A, a1, a2, B

    // Click a child's label to select it (past the arrow column).
    float labelX = t->rect.x + 60.0f;
    clickAt(ctx, labelX, t->rect.y + t->rowHeight * 1.5f); // row 1 = "a1"
    REQUIRE(t->selected == 1);
}

TEST_CASE("TreeView expands/collapses the selected node with Left/Right") {
    StubRenderer r;
    Context ctx;
    auto tv = std::make_unique<TreeView>();
    TreeView* t = tv.get();
    TreeNode& a = t->addRoot("A");
    a.add("a1");
    a.add("a2");
    t->addRoot("B"); // leaf root
    ctx.setRoot(std::move(tv));
    ctx.frame(r, 0.016f, -1, -1);

    REQUIRE(t->numRows() == 2);
    ctx.setFocus(t);
    t->selected = 0; // highlight "A"

    key(ctx, Key::Right); // expand A
    REQUIRE(t->numRows() == 4);
    key(ctx, Key::Right); // already open: no-op
    REQUIRE(t->numRows() == 4);
    key(ctx, Key::Left); // collapse A
    REQUIRE(t->numRows() == 2);

    // On a leaf, Left/Right don't change the row count.
    t->selected = 1; // "B"
    key(ctx, Key::Right);
    REQUIRE(t->numRows() == 2);
}

TEST_CASE("TabBar switches the active tab on click") {
    StubRenderer r;
    Context ctx;
    auto tb = std::make_unique<TabBar>();
    TabBar* t = tb.get();
    t->tabs = {"One", "Two", "Three"};
    int changed = -1;
    t->onChange = [&](int i) { changed = i; };
    ctx.setRoot(std::move(tb));
    ctx.frame(r, 0.016f, -1, -1);

    // Tab 0 width = 3*7*1.4 + 28 ~= 57; x=80 lands in tab 1.
    clickAt(ctx, t->rect.x + 80.0f, t->rect.y + t->rect.h * 0.5f);
    REQUIRE(t->active == 1);
    REQUIRE(changed == 1);
}

TEST_CASE("ScrollView scrolls its content child on the wheel") {
    StubRenderer r;
    Context ctx;
    auto sv = std::make_unique<ScrollView>();
    ScrollView* s = sv.get();
    auto content = std::make_unique<Box>();
    content->axis = Axis::Column;
    for (int i = 0; i < 60; ++i) content->emplace<Label>("row " + std::to_string(i), 1.4f);
    Widget* c = s->setContent(std::move(content));
    ctx.setRoot(std::move(sv));
    ctx.frame(r, 0.016f, -1, -1);

    float y0 = c->rect.y;
    wheelAt(ctx, s->rect.x + 10, s->rect.y + 10, -3.0f); // scroll down
    ctx.frame(r, 0.016f, -1, -1);                        // re-arrange with the new offset
    REQUIRE(c->rect.y < y0);
}
