// Headless tests for widget set I (#214): the new core controls (Slider, Toggle)
// and the overlay layer (Modal/Menu dismissal + lifecycle) driven through Context
// with a stub renderer that supplies deterministic text metrics.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
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

void clickAt(Context& ctx, float x, float y) {
    InputEvent d;
    d.type = InputEvent::MouseDown;
    d.x = x;
    d.y = y;
    ctx.handleEvent(d);
    InputEvent u = d;
    u.type = InputEvent::MouseUp;
    ctx.handleEvent(u);
}

void pump(Context& ctx, Renderer& r, int frames, float dt = 0.05f) {
    for (int i = 0; i < frames; ++i) ctx.frame(r, dt, -1, -1);
}

} // namespace

TEST_CASE("Toggle flips on click and reports the new state") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    bool last = false;
    Toggle* t = root->emplace<Toggle>("enabled", false);
    t->onChange = [&](bool v) { last = v; };
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);

    clickAt(ctx, t->rect.x + 5, t->rect.y + t->rect.h * 0.5f);
    REQUIRE(t->on == true);
    REQUIRE(last == true);
}

TEST_CASE("Slider maps click position to a value and clamps") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Slider* s = root->emplace<Slider>(0.0f, 100.0f, 0.0f);
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);

    float cy = s->rect.y + s->rect.h * 0.5f;
    // Mid-track ~= mid value.
    clickAt(ctx, s->rect.x + s->rect.w * 0.5f, cy);
    REQUIRE(s->value > 45.0f);
    REQUIRE(s->value < 55.0f);

    // Dragging past the ends clamps (a drag routes to the pressed widget even when
    // the pointer leaves its rect; a bare click outside it would never reach it).
    InputEvent d;
    d.type = InputEvent::MouseDown;
    d.x = s->rect.x + s->rect.w * 0.5f;
    d.y = cy;
    ctx.handleEvent(d);
    InputEvent m;
    m.type = InputEvent::MouseMove;
    m.y = cy;
    m.x = s->rect.x - 50.0f; // drag past the left end
    ctx.handleEvent(m);
    REQUIRE(s->value == 0.0f);
    m.x = s->rect.x + s->rect.w + 50.0f; // drag past the right end
    ctx.handleEvent(m);
    REQUIRE(s->value == 100.0f);
    InputEvent u;
    u.type = InputEvent::MouseUp;
    u.x = m.x;
    u.y = cy;
    ctx.handleEvent(u);
}

TEST_CASE("Slider arrow keys nudge the value") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Slider* s = root->emplace<Slider>(0.0f, 10.0f, 5.0f);
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);
    ctx.setFocus(s);

    InputEvent e;
    e.type = InputEvent::KeyDown;
    e.key = Key::Right;
    ctx.handleEvent(e);
    REQUIRE(s->value > 5.0f);
    e.key = Key::Home;
    ctx.handleEvent(e);
    REQUIRE(s->value == 0.0f);
    e.key = Key::End;
    ctx.handleEvent(e);
    REQUIRE(s->value == 10.0f);
}

TEST_CASE("Modal: Escape dismisses and Context prunes it") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());

    auto modal = std::make_unique<Modal>();
    auto body = std::make_unique<Box>();
    body->padding = Edges(20);
    body->emplace<Label>("Are you sure?", 1.4f);
    modal->setContent(std::move(body));
    ctx.addOverlay(std::move(modal));

    pump(ctx, r, 3);
    REQUIRE(ctx.hasInteractiveOverlay());

    InputEvent esc;
    esc.type = InputEvent::KeyDown;
    esc.key = Key::Escape;
    ctx.handleEvent(esc);

    pump(ctx, r, 80); // let the close animation finish, then prune
    REQUIRE_FALSE(ctx.hasInteractiveOverlay());
}

TEST_CASE("Menu: clicking an item runs its action and closes") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());

    auto menu = std::make_unique<Menu>();
    menu->anchorX = 100;
    menu->anchorY = 100;
    int chosen = 0;
    menu->addItem("Rename", [&] { chosen = 1; });
    menu->addItem("Delete", [&] { chosen = 2; });
    Menu* mp = menu.get();
    ctx.addOverlay(std::move(menu));
    pump(ctx, r, 3);

    // Second item ("Delete") lives in the content list's child #1.
    Widget* list = mp->content();
    REQUIRE(list->children().size() == 2);
    Widget* deleteItem = list->children()[1].get();
    clickAt(ctx, deleteItem->rect.x + deleteItem->rect.w * 0.5f, deleteItem->rect.y + deleteItem->rect.h * 0.5f);
    REQUIRE(chosen == 2);

    pump(ctx, r, 80);
    REQUIRE_FALSE(ctx.hasInteractiveOverlay());
}

TEST_CASE("Menu: a click outside the content dismisses it") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());

    auto menu = std::make_unique<Menu>();
    menu->anchorX = 100;
    menu->anchorY = 100;
    int chosen = 0;
    menu->addItem("Only", [&] { chosen = 1; });
    Menu* mp = menu.get();
    ctx.addOverlay(std::move(menu));
    pump(ctx, r, 3);

    Rect c = mp->contentRect();
    clickAt(ctx, c.x + c.w + 200.0f, c.y + c.h + 200.0f); // far outside
    REQUIRE(chosen == 0);                                 // nothing selected
    pump(ctx, r, 80);
    REQUIRE_FALSE(ctx.hasInteractiveOverlay());
}

TEST_CASE("Tooltip appears after hovering a widget and dismisses on leave") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    root->padding = Edges(10);
    Label* l = root->emplace<Label>("hover me", 1.4f);
    l->tooltip = "an explanation";
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1); // lay out with the pointer off-screen

    float mx = l->rect.x + l->rect.w * 0.5f, my = l->rect.y + l->rect.h * 0.5f;
    REQUIRE(ctx.overlayCount() == 0);
    for (int i = 0; i < 20; ++i) ctx.frame(r, 0.1f, mx, my); // rest on it past the delay
    REQUIRE(ctx.overlayCount() == 1);

    for (int i = 0; i < 80; ++i) ctx.frame(r, 0.1f, -1, -1); // move away -> dismiss + prune
    REQUIRE(ctx.overlayCount() == 0);
}

TEST_CASE("Toasts stack instead of overlapping") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    auto a = std::make_unique<Toast>("first", 999.0f);
    Toast* ta = a.get();
    ctx.addOverlay(std::move(a));
    auto b = std::make_unique<Toast>("second", 999.0f);
    Toast* tb = b.get();
    ctx.addOverlay(std::move(b));

    for (int i = 0; i < 60; ++i) ctx.frame(r, 0.05f, -1, -1); // let the slot springs settle
    REQUIRE(std::abs(ta->contentRect().y - tb->contentRect().y) > 30.0f);
}

TEST_CASE("Toast self-closes after its lifetime") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());

    auto toast = std::make_unique<Toast>("Saved", 0.2f);
    bool closed = false;
    toast->onClosed = [&] { closed = true; };
    ctx.addOverlay(std::move(toast));

    pump(ctx, r, 2);
    REQUIRE_FALSE(closed); // still within its lifetime
    pump(ctx, r, 120);     // past lifetime + close animation
    REQUIRE(closed);
}
