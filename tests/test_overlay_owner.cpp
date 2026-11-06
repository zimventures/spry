// Overlay owners: an overlay that outlives what it acts for is dropped before
// anything can reach it, rather than calling back into freed memory. Headless; run
// under AddressSanitizer for the use-after-free cases to mean anything.
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

void pump(Context& ctx, Renderer& r, int frames) {
    for (int i = 0; i < frames; ++i) ctx.frame(r, 0.016f, -1.0f, -1.0f);
}

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

void clickOn(Context& ctx, Widget* w) { clickAt(ctx, w->rect.x + w->rect.w * 0.5f, w->rect.y + w->rect.h * 0.5f); }

// A menu with one item, whose action bumps `hits`.
std::unique_ptr<Menu> menuCounting(int& hits) {
    auto menu = std::make_unique<Menu>();
    menu->anchorX = 100;
    menu->anchorY = 100;
    menu->addItem("Item", [&hits] { ++hits; });
    return menu;
}

} // namespace

TEST_CASE("an overlay whose owner has gone is dropped before the next event reaches it") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    int rootClicks = 0;
    Button* under = root->emplace<Button>("Under", [&rootClicks] { ++rootClicks; });
    under->prefW = 800;
    under->prefH = 600;
    ctx.setRoot(std::move(root));

    int hits = 0, closed = 0;
    auto owner = std::make_shared<int>(0);
    auto menu = menuCounting(hits);
    menu->onClosed = [&closed] { ++closed; };
    menu->setOwner(owner);
    Menu* m = static_cast<Menu*>(ctx.addOverlay(std::move(menu)));
    pump(ctx, r, 3);
    Widget* item = m->content()->children()[0].get();
    const float x = item->rect.x + item->rect.w * 0.5f, y = item->rect.y + item->rect.h * 0.5f;
    REQUIRE(ctx.hasInteractiveOverlay());

    owner.reset();
    CHECK_FALSE(ctx.hasInteractiveOverlay()); // already, before any event or frame
    clickAt(ctx, x, y);                        // where the item was
    CHECK(hits == 0);
    CHECK(rootClicks == 1); // the click went to what was beneath
    CHECK(ctx.overlayCount() == 0);
    pump(ctx, r, 80);
    CHECK(closed == 0); // and onClosed never fired
}

TEST_CASE("an orphaned overlay is dropped by the next frame without an event") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    int closed = 0;
    auto owner = std::make_shared<int>(0);
    auto modal = std::make_unique<Modal>();
    modal->setContent(std::make_unique<Box>());
    modal->onClosed = [&closed] { ++closed; };
    modal->setOwner(owner);
    ctx.addOverlay(std::move(modal));
    pump(ctx, r, 3);

    owner.reset();
    pump(ctx, r, 1);
    CHECK(ctx.overlayCount() == 0);
    CHECK_FALSE(ctx.animationsActive());
    CHECK(closed == 0);
}

TEST_CASE("an overlay with no owner is left alone") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    int hits = 0;
    ctx.addOverlay(menuCounting(hits));
    pump(ctx, r, 3);
    CHECK_FALSE(Overlay::isOwner(ctx.topOverlay()->owner()));
    CHECK(ctx.hasInteractiveOverlay());
}

TEST_CASE("an overlay opened from a click acts for the nearest owner above the widget") {
    StubRenderer r;
    Context ctx;
    auto outer = std::make_shared<int>(0), inner = std::make_shared<int>(0);
    auto root = std::make_unique<Box>();
    root->overlayOwner = outer;
    Box* view = root->emplace<Box>();
    view->overlayOwner = inner;
    int hits = 0;
    Button* open = view->emplace<Button>("Open", [&] { Context::current()->addOverlay(menuCounting(hits)); });
    ctx.setRoot(std::move(root));
    pump(ctx, r, 1);

    clickOn(ctx, open);
    REQUIRE(ctx.hasInteractiveOverlay());
    CHECK(ctx.topOverlay()->owner().lock() == inner);

    inner.reset();
    CHECK_FALSE(ctx.hasInteractiveOverlay());
}

TEST_CASE("an overlay opened from a key press acts for the focused widget's owner") {
    StubRenderer r;
    Context ctx;
    auto owner = std::make_shared<int>(0);
    auto root = std::make_unique<Box>();
    root->overlayOwner = owner;
    int hits = 0;
    Button* open = root->emplace<Button>("Open", [&] { Context::current()->addOverlay(menuCounting(hits)); });
    ctx.setRoot(std::move(root));
    pump(ctx, r, 1);
    ctx.setFocus(open);

    InputEvent enter;
    enter.type = InputEvent::KeyDown;
    enter.key = Key::Enter;
    ctx.handleEvent(enter);
    REQUIRE(ctx.hasInteractiveOverlay());
    CHECK(ctx.topOverlay()->owner().lock() == owner);
}

TEST_CASE("an overlay opened from inside another acts for the same owner") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    auto owner = std::make_shared<int>(0);
    auto menu = std::make_unique<Menu>();
    menu->anchorX = 100;
    menu->anchorY = 100;
    menu->addItem("Rename…", [] {
        auto dialog = std::make_unique<Modal>();
        dialog->setContent(std::make_unique<Box>());
        Context::current()->addOverlay(std::move(dialog));
    });
    menu->setOwner(owner);
    Menu* m = static_cast<Menu*>(ctx.addOverlay(std::move(menu)));
    pump(ctx, r, 3);

    clickOn(ctx, m->content()->children()[0].get());
    REQUIRE(ctx.overlayCount() == 2);
    CHECK(ctx.topOverlay()->owner().lock() == owner); // the dialog, not the closing menu
}

TEST_CASE("an OwnerScope names the owner outside event handling; the innermost wins; setOwner beats both") {
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    auto a = std::make_shared<int>(0), b = std::make_shared<int>(0), c = std::make_shared<int>(0);
    Overlay* inA = nullptr;
    Overlay* inB = nullptr;
    Overlay* told = nullptr;
    {
        Context::OwnerScope sa(ctx, a);
        inA = ctx.addOverlay(std::make_unique<Modal>());
        {
            Context::OwnerScope sb(ctx, b);
            inB = ctx.addOverlay(std::make_unique<Modal>());
            auto explicitly = std::make_unique<Modal>();
            explicitly->setOwner(c);
            told = ctx.addOverlay(std::move(explicitly));
        }
    }
    Overlay* after = ctx.addOverlay(std::make_unique<Modal>());
    CHECK(inA->owner().lock() == a);
    CHECK(inB->owner().lock() == b);
    CHECK(told->owner().lock() == c);
    CHECK_FALSE(Overlay::isOwner(after->owner())); // no scope, no event: unowned
}

TEST_CASE("an overlay opened from another's onClosed acts for the same owner") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    auto owner = std::make_shared<int>(0);
    auto first = std::make_unique<Modal>();
    first->setContent(std::make_unique<Box>());
    first->onClosed = [&ctx] {
        auto next = std::make_unique<Modal>();
        next->setContent(std::make_unique<Box>());
        ctx.addOverlay(std::move(next));
    };
    first->setOwner(owner);
    Overlay* f = ctx.addOverlay(std::move(first));
    pump(ctx, r, 3);
    f->requestClose();
    pump(ctx, r, 80);
    REQUIRE(ctx.overlayCount() == 1);
    CHECK(ctx.topOverlay()->owner().lock() == owner);
}

TEST_CASE("a combo's dropdown goes with the combo") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Box* host = root->emplace<Box>();
    Combo* c = host->emplace<Combo>(std::vector<std::string>{"Red", "Green", "Blue"}, 0);
    int changes = 0;
    c->onChange = [&changes](int, const std::string&) { ++changes; };
    ctx.setRoot(std::move(root));
    pump(ctx, r, 1);

    clickAt(ctx, c->rect.x + 5, c->rect.y + c->rect.h * 0.5f);
    pump(ctx, r, 3);
    REQUIRE(ctx.hasInteractiveOverlay());
    Widget* blue = ctx.topOverlay()->content()->children()[2].get();
    const float x = blue->rect.x + blue->rect.w * 0.5f, y = blue->rect.y + blue->rect.h * 0.5f;

    // The scene is rebuilt with the dropdown still open.
    ctx.dropInteractionWithin(host);
    host->clearChildren();
    clickAt(ctx, x, y); // where "Blue" was: before this, a use-after-free on the combo
    CHECK(changes == 0);
    CHECK(ctx.overlayCount() == 0);
}

TEST_CASE("a colour field's picker goes with the field") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Box* host = root->emplace<Box>();
    ColorField* f = host->emplace<ColorField>(Color{10, 20, 30});
    ctx.setRoot(std::move(root));
    pump(ctx, r, 1);

    clickOn(ctx, f);
    pump(ctx, r, 3);
    REQUIRE(ctx.hasInteractiveOverlay());

    ctx.dropInteractionWithin(host);
    host->clearChildren();
    pump(ctx, r, 1);
    CHECK(ctx.overlayCount() == 0);
}
