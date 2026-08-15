// Headless tests for ReorderableList (#60): hit-test opt-out, subtree hover, and
// the drag-to-reorder contract — reported on drop, never applied to the children.
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "spry/spry.h"

using namespace spry;

namespace {

struct StubRenderer : Renderer {
    void beginFrame(Color) override {}
    void endFrame() override {}
    void outputSize(int& w, int& h) override {
        w = 400;
        h = 400;
    }
    void fillMesh(const std::vector<Vertex>&, const std::vector<int>&) override {}
    void fillRoundedRect(float, float, float, float, float, Color, Color) override {}
    void fillRect(float, float, float, float, Color) override {}
    void text(float, float, float, Color, const char*) override {}
    Size measureText(float scale, const char* s) override {
        return Size{(float)std::strlen(s) * 7.0f * scale, 13.0f * scale + 7.0f};
    }
};

// A row of fixed height, so the tests can talk about slots without depending on
// text metrics.
Box* addFixedRow(ReorderableList& list, float height = 30.0f) {
    auto* row = list.emplaceRow<Box>();
    row->axis = Axis::Row;
    row->prefH = height;
    return row;
}

void layout(ReorderableList& list, Renderer& r, float w = 200.0f, float h = 400.0f) {
    list.measure(r, w, h);
    list.arrange(r, Rect{0, 0, w, h});
}

void drag(ReorderableList& list, float fromY, float toY) {
    list.onMouseDown(10.0f, fromY, 0, false, false);
    list.onMouseDrag(10.0f, toY);
    list.onMouseUp(10.0f, toY, 0);
}

} // namespace

// --- the hit-test opt-out ----------------------------------------------------

TEST_CASE("a non-interactive widget passes the pointer through to its parent") {
    Box parent;
    parent.rect = Rect{0, 0, 100, 100};
    auto* child = parent.emplace<Box>();
    child->rect = Rect{0, 0, 100, 100};

    REQUIRE(parent.hitTest(50, 50) == child); // the default: deepest wins

    child->interactive = false;
    REQUIRE(parent.hitTest(50, 50) == &parent);
}

TEST_CASE("an inert widget's children are still hit-tested") {
    // Only the widget itself steps aside — a button inside inert decoration keeps
    // its clicks.
    Box parent;
    parent.rect = Rect{0, 0, 100, 100};
    auto* wrapper = parent.emplace<Box>();
    wrapper->rect = Rect{0, 0, 100, 100};
    wrapper->interactive = false;
    auto* inner = wrapper->emplace<Box>();
    inner->rect = Rect{10, 10, 20, 20};

    REQUIRE(parent.hitTest(15, 15) == inner);
    REQUIRE(parent.hitTest(50, 50) == &parent);
}

TEST_CASE("an inert widget doesn't swallow a sibling behind it") {
    Box parent;
    parent.rect = Rect{0, 0, 100, 100};
    auto* behind = parent.emplace<Box>();
    behind->rect = Rect{0, 0, 100, 100};
    auto* front = parent.emplace<Box>(); // later child = tested first
    front->rect = Rect{0, 0, 100, 100};
    front->interactive = false;

    REQUIRE(parent.hitTest(50, 50) == behind);
}

// --- hover across a subtree --------------------------------------------------

TEST_CASE("hoveredWithin sees a hovered descendant") {
    Box row;
    auto* label = row.emplace<Box>();
    auto* button = label->emplace<Box>();

    REQUIRE_FALSE(row.hoveredWithin());
    button->hovered = true;
    REQUIRE(row.hoveredWithin()); // the row stays lit while the pointer is on a child
    button->hovered = false;
    row.hovered = true;
    REQUIRE(row.hoveredWithin());
}

// --- reordering --------------------------------------------------------------

TEST_CASE("dragging a row down reports the move on drop") {
    StubRenderer r;
    ReorderableList list;
    for (int i = 0; i < 4; ++i) addFixedRow(list);
    int from = -1, to = -1;
    list.onReorder = [&](int f, int t) {
        from = f;
        to = t;
    };
    layout(list, r);

    drag(list, 15.0f, 95.0f); // row 0 -> the slot of row 2
    REQUIRE(from == 0);
    REQUIRE(to == 2);
}

TEST_CASE("dragging a row up reports the move") {
    StubRenderer r;
    ReorderableList list;
    for (int i = 0; i < 4; ++i) addFixedRow(list);
    int from = -1, to = -1;
    list.onReorder = [&](int f, int t) {
        from = f;
        to = t;
    };
    layout(list, r);

    drag(list, 115.0f, 15.0f); // row 3 -> row 0
    REQUIRE(from == 3);
    REQUIRE(to == 0);
}

TEST_CASE("the list never reorders its own children") {
    // The model is the source of truth; the caller rebuilds. A list that shuffled
    // itself would fight whatever the caller does next.
    StubRenderer r;
    ReorderableList list;
    Widget* first = addFixedRow(list);
    addFixedRow(list);
    list.onReorder = [](int, int) {};
    layout(list, r);

    drag(list, 15.0f, 50.0f);
    REQUIRE(list.children().front().get() == first);
}

TEST_CASE("a drag that ends where it started reports nothing") {
    StubRenderer r;
    ReorderableList list;
    for (int i = 0; i < 3; ++i) addFixedRow(list);
    bool fired = false;
    list.onReorder = [&](int, int) { fired = true; };
    layout(list, r);

    drag(list, 15.0f, 20.0f); // within the same row
    REQUIRE_FALSE(fired);
}

TEST_CASE("a press with no drag reports nothing") {
    StubRenderer r;
    ReorderableList list;
    for (int i = 0; i < 3; ++i) addFixedRow(list);
    bool fired = false;
    list.onReorder = [&](int, int) { fired = true; };
    layout(list, r);

    list.onMouseDown(10.0f, 15.0f, 0, false, false);
    list.onMouseUp(10.0f, 15.0f, 0);
    REQUIRE_FALSE(fired);
}

TEST_CASE("dragging past the ends clamps to the first and last slots") {
    StubRenderer r;
    ReorderableList list;
    for (int i = 0; i < 3; ++i) addFixedRow(list);
    int to = -1;
    list.onReorder = [&](int, int t) { to = t; };
    layout(list, r);

    drag(list, 50.0f, -500.0f);
    REQUIRE(to == 0);
    drag(list, 50.0f, 5000.0f);
    REQUIRE(to == 2);
}

TEST_CASE("rows of different heights still resolve the right slot") {
    // The drop target comes from the laid-out row rects, not from a row-height
    // guess, so a tall row doesn't skew everything below it.
    StubRenderer r;
    ReorderableList list;
    list.spacing = 0.0f;
    addFixedRow(list, 20.0f);  // slot 0: y 0..20
    addFixedRow(list, 100.0f); // slot 1: y 20..120
    addFixedRow(list, 20.0f);  // slot 2: y 120..140
    int to = -1;
    list.onReorder = [&](int, int t) { to = t; };
    layout(list, r);

    drag(list, 5.0f, 110.0f); // still inside the tall row
    REQUIRE(to == 1);

    drag(list, 5.0f, 130.0f);
    REQUIRE(to == 2);
}

TEST_CASE("the right mouse button doesn't start a drag") {
    StubRenderer r;
    ReorderableList list;
    for (int i = 0; i < 3; ++i) addFixedRow(list);
    bool fired = false;
    list.onReorder = [&](int, int) { fired = true; };
    layout(list, r);

    list.onMouseDown(10.0f, 15.0f, 1, false, false); // 1 = right
    list.onMouseDrag(10.0f, 95.0f);
    list.onMouseUp(10.0f, 95.0f, 1);
    REQUIRE_FALSE(fired);
}

TEST_CASE("the dragged row follows the pointer") {
    StubRenderer r;
    ReorderableList list;
    addFixedRow(list);
    addFixedRow(list);
    layout(list, r);
    const float restY = list.children().front()->rect.y;

    list.onMouseDown(10.0f, 15.0f, 0, false, false);
    list.onMouseDrag(10.0f, 45.0f);
    layout(list, r); // layout runs every frame while dragging

    REQUIRE(list.children().front()->rect.y > restY);
    REQUIRE(list.draggedRow() == 0);
}

TEST_CASE("a dragged row's children move with it") {
    StubRenderer r;
    ReorderableList list;
    Box* row = addFixedRow(list);
    auto* inner = row->emplace<Box>();
    inner->prefH = 10.0f;
    addFixedRow(list);
    layout(list, r);
    const float restY = inner->rect.y;

    list.onMouseDown(10.0f, 15.0f, 0, false, false);
    list.onMouseDrag(10.0f, 45.0f);
    layout(list, r);

    REQUIRE(inner->rect.y > restY); // the whole subtree is displaced, not just the row
}

TEST_CASE("addRow makes a row's decoration inert but leaves its controls alone") {
    ReorderableList list;
    auto row = std::make_unique<Box>();
    auto* label = row->emplace<Box>(); // decoration
    auto* button = row->emplace<Box>();
    button->focusable = true; // a control
    auto* tipped = row->emplace<Box>();
    tipped->tooltip = "explain";

    Widget* adopted = list.addRow(std::move(row));

    REQUIRE_FALSE(adopted->interactive);
    REQUIRE_FALSE(label->interactive);
    REQUIRE(button->interactive); // keeps its clicks
    REQUIRE(tipped->interactive); // keeps its hover, or the tooltip never shows
}

TEST_CASE("markRowsInert can be turned off") {
    ReorderableList list;
    list.markRowsInert = false;
    auto row = std::make_unique<Box>();
    auto* label = row->emplace<Box>();

    list.addRow(std::move(row));
    REQUIRE(label->interactive);
}

TEST_CASE("an empty list ignores a drag instead of crashing") {
    StubRenderer r;
    ReorderableList list;
    bool fired = false;
    list.onReorder = [&](int, int) { fired = true; };
    layout(list, r);

    drag(list, 10.0f, 90.0f);
    REQUIRE_FALSE(fired);
}

TEST_CASE("a hidden row is not a slot") {
    StubRenderer r;
    ReorderableList list;
    addFixedRow(list);
    Box* hidden = addFixedRow(list);
    hidden->visible = false;
    addFixedRow(list);
    int to = -1;
    list.onReorder = [&](int, int t) { to = t; };
    layout(list, r);

    // Two visible rows, so the second slot is index 1 — not 2.
    drag(list, 15.0f, 50.0f);
    REQUIRE(to == 1);
}
