// Headless unit tests for Spry's input / focus / keyboard logic (#216). No GL or
// SDL — a stub renderer supplies deterministic text metrics so layout runs, then
// we drive events through Context and assert on the widget tree.
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

// Lay out the tree so widgets have rects (mouse off-screen, no hover).
void layout(Context& ctx, Renderer& r) {
    ctx.frame(r, 0.016f, -1.0f, -1.0f);
}

void clickAt(Context& ctx, Widget* w) {
    InputEvent d;
    d.type = InputEvent::MouseDown;
    d.x = w->rect.x + w->rect.w * 0.5f;
    d.y = w->rect.y + w->rect.h * 0.5f;
    ctx.handleEvent(d);
    InputEvent u = d;
    u.type = InputEvent::MouseUp;
    ctx.handleEvent(u);
}

void pressKey(Context& ctx, Key k, bool shift = false) {
    InputEvent e;
    e.type = InputEvent::KeyDown;
    e.key = k;
    e.shift = shift;
    ctx.handleEvent(e);
}

} // namespace

TEST_CASE("click activates a button and focuses it") {
    StubRenderer r;
    Context ctx;
    int clicks = 0;
    auto root = std::make_unique<Box>();
    Button* b = root->emplace<Button>("Go", [&] { clicks++; });
    ctx.setRoot(std::move(root));
    layout(ctx, r);

    clickAt(ctx, b);
    REQUIRE(clicks == 1);
    REQUIRE(ctx.focused() == b);
}

TEST_CASE("press then release off-target does not activate") {
    StubRenderer r;
    Context ctx;
    int clicks = 0;
    auto root = std::make_unique<Box>();
    Button* b = root->emplace<Button>("Go", [&] { clicks++; });
    ctx.setRoot(std::move(root));
    layout(ctx, r);

    InputEvent d;
    d.type = InputEvent::MouseDown;
    d.x = b->rect.x + b->rect.w * 0.5f;
    d.y = b->rect.y + b->rect.h * 0.5f;
    ctx.handleEvent(d);
    InputEvent u;
    u.type = InputEvent::MouseUp;
    u.x = b->rect.x - 100.0f; // released well outside the button
    u.y = d.y;
    ctx.handleEvent(u);

    REQUIRE(clicks == 0);
}

TEST_CASE("Tab cycles focus in tree order and wraps") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Button* b1 = root->emplace<Button>("A");
    Button* b2 = root->emplace<Button>("B");
    Checkbox* c = root->emplace<Checkbox>("C");
    ctx.setRoot(std::move(root));
    layout(ctx, r);

    REQUIRE(ctx.focused() == nullptr);
    pressKey(ctx, Key::Tab);
    REQUIRE(ctx.focused() == b1);
    pressKey(ctx, Key::Tab);
    REQUIRE(ctx.focused() == b2);
    pressKey(ctx, Key::Tab);
    REQUIRE(ctx.focused() == c);
    pressKey(ctx, Key::Tab);
    REQUIRE(ctx.focused() == b1); // wraps forward
    pressKey(ctx, Key::Tab, /*shift=*/true);
    REQUIRE(ctx.focused() == c); // wraps backward
}

TEST_CASE("Enter and Space activate the focused button") {
    StubRenderer r;
    Context ctx;
    int clicks = 0;
    auto root = std::make_unique<Box>();
    Button* b = root->emplace<Button>("Go", [&] { clicks++; });
    ctx.setRoot(std::move(root));
    layout(ctx, r);

    ctx.setFocus(b);
    pressKey(ctx, Key::Enter);
    pressKey(ctx, Key::Space);
    REQUIRE(clicks == 2);
}

TEST_CASE("checkbox toggles on click and on Enter") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Checkbox* c = root->emplace<Checkbox>("C", false);
    ctx.setRoot(std::move(root));
    layout(ctx, r);

    clickAt(ctx, c);
    REQUIRE(c->checked == true);
    ctx.setFocus(c);
    pressKey(ctx, Key::Enter);
    REQUIRE(c->checked == false);
}

TEST_CASE("clicking a non-focusable widget clears focus") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Button* b = root->emplace<Button>("A");
    Label* lbl = root->emplace<Label>("just text");
    ctx.setRoot(std::move(root));
    layout(ctx, r);

    ctx.setFocus(b);
    REQUIRE(ctx.focused() == b);
    clickAt(ctx, lbl);
    REQUIRE(ctx.focused() == nullptr);
}

TEST_CASE("visitAccessible exposes roles and labels") {
    Context ctx;
    auto root = std::make_unique<Box>();
    root->emplace<Label>("Hi");
    root->emplace<Button>("Go");
    root->emplace<Checkbox>("Chk");
    ctx.setRoot(std::move(root));

    bool hasLabel = false, hasButton = false, hasCheckbox = false;
    ctx.visitAccessible([&](Widget*, Role role, const std::string& lbl) {
        if (role == Role::Label && lbl == "Hi")
            hasLabel = true;
        if (role == Role::Button && lbl == "Go")
            hasButton = true;
        if (role == Role::Checkbox && lbl == "Chk")
            hasCheckbox = true;
    });
    REQUIRE(hasLabel);
    REQUIRE(hasButton);
    REQUIRE(hasCheckbox);
}
