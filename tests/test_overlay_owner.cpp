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

// True if `o` acts for `owner`.
bool actsFor(const Overlay* o, const std::shared_ptr<int>& owner) {
    for (const auto& w : o->owners())
        if (w.lock() == owner) return true;
    return false;
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
    menu->addOwner(owner);
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

TEST_CASE("an orphaned overlay keeps frames coming until the next frame drops it") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    int closed = 0;
    auto owner = std::make_shared<int>(0);
    auto modal = std::make_unique<Modal>();
    modal->setContent(std::make_unique<Box>());
    modal->onClosed = [&closed] { ++closed; };
    modal->addOwner(owner);
    ctx.addOverlay(std::move(modal));
    pump(ctx, r, 80);
    REQUIRE_FALSE(ctx.animationsActive()); // settled

    owner.reset();
    CHECK(ctx.animationsActive()); // drawn last frame: an on-demand host must draw once more
    pump(ctx, r, 1);
    CHECK(ctx.overlayCount() == 0);
    CHECK_FALSE(ctx.animationsActive());
    CHECK(closed == 0);
}

TEST_CASE("dropping an orphan doesn't tell its focused widget it lost focus") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    struct Watch : Widget {
        int* lost;
        explicit Watch(int* l) : lost(l) { focusable = true; }
        void onFocusChanged(bool on) override {
            if (!on) ++*lost;
        }
    };
    int lost = 0;
    auto owner = std::make_shared<int>(0);
    auto modal = std::make_unique<Modal>();
    auto* w = static_cast<Watch*>(modal->setContent(std::make_unique<Watch>(&lost)));
    modal->addOwner(owner);
    ctx.addOverlay(std::move(modal));
    pump(ctx, r, 3);
    ctx.setFocus(w);

    owner.reset();
    pump(ctx, r, 1);
    CHECK(ctx.overlayCount() == 0);
    CHECK(ctx.focused() == nullptr);
    CHECK(lost == 0);
}

TEST_CASE("an overlay with no owner is left alone") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    int hits = 0;
    ctx.addOverlay(menuCounting(hits));
    pump(ctx, r, 3);
    CHECK(ctx.topOverlay()->owners().empty());
    CHECK(ctx.hasInteractiveOverlay());
}

TEST_CASE("an overlay's own overlayOwner counts as an owner") {
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    auto owner = std::make_shared<int>(0);
    auto modal = std::make_unique<Modal>();
    modal->overlayOwner = owner;
    Overlay* m = ctx.addOverlay(std::move(modal));
    CHECK(actsFor(m, owner));
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
    CHECK(actsFor(ctx.topOverlay(), inner));
    CHECK_FALSE(actsFor(ctx.topOverlay(), outer));

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
    CHECK(actsFor(ctx.topOverlay(), owner));
}

TEST_CASE("an overlay opened from a wheel handler acts for that widget, not the focused one") {
    StubRenderer r;
    Context ctx;
    auto a = std::make_shared<int>(0), b = std::make_shared<int>(0);
    struct Wheely : Widget {
        bool onWheel(float, float) override {
            Context::current()->addOverlay(std::make_unique<Modal>());
            return true;
        }
    };
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    Box* viewA = root->emplace<Box>();
    viewA->overlayOwner = a;
    Button* focusedInA = viewA->emplace<Button>("A");
    Box* viewB = root->emplace<Box>();
    viewB->overlayOwner = b;
    viewB->grow = 1;
    Wheely* wheely = viewB->emplace<Wheely>();
    wheely->prefW = 200;
    wheely->prefH = 200;
    ctx.setRoot(std::move(root));
    pump(ctx, r, 1);
    ctx.setFocus(focusedInA);

    InputEvent wheel;
    wheel.type = InputEvent::Wheel;
    wheel.x = wheely->rect.x + 10;
    wheel.y = wheely->rect.y + 10;
    wheel.wheel = 1;
    ctx.handleEvent(wheel);
    REQUIRE(ctx.overlayCount() == 1);
    CHECK(actsFor(ctx.topOverlay(), b));
    CHECK_FALSE(actsFor(ctx.topOverlay(), a));
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
    menu->addOwner(owner);
    Menu* m = static_cast<Menu*>(ctx.addOverlay(std::move(menu)));
    pump(ctx, r, 3);

    clickOn(ctx, m->content()->children()[0].get());
    REQUIRE(ctx.overlayCount() == 2);
    CHECK(actsFor(ctx.topOverlay(), owner)); // the dialog, not the closing menu
}

TEST_CASE("an OwnerScope adds its owner; the innermost counts; an empty one leaves the outer in force") {
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    auto a = std::make_shared<int>(0), b = std::make_shared<int>(0), c = std::make_shared<int>(0);
    Overlay* inA = nullptr;
    Overlay* inB = nullptr;
    Overlay* told = nullptr;
    Overlay* inEmpty = nullptr;
    {
        Context::OwnerScope sa(ctx, a);
        inA = ctx.addOverlay(std::make_unique<Modal>());
        {
            Context::OwnerScope empty(ctx, {});
            inEmpty = ctx.addOverlay(std::make_unique<Modal>());
        }
        {
            Context::OwnerScope sb(ctx, b);
            inB = ctx.addOverlay(std::make_unique<Modal>());
            auto explicitly = std::make_unique<Modal>();
            explicitly->addOwner(c);
            told = ctx.addOverlay(std::move(explicitly));
        }
    }
    Overlay* after = ctx.addOverlay(std::make_unique<Modal>());
    CHECK(actsFor(inA, a));
    CHECK(actsFor(inEmpty, a));
    CHECK(actsFor(inB, b));
    CHECK_FALSE(actsFor(inB, a));
    CHECK(actsFor(told, c)); // told, and in scope: both
    CHECK(actsFor(told, b));
    CHECK(after->owners().empty()); // no scope, no event: unowned
}

TEST_CASE("during an event, a scope's owner and the widget's both count") {
    StubRenderer r;
    Context ctx;
    auto shell = std::make_shared<int>(0), view = std::make_shared<int>(0);
    auto root = std::make_unique<Box>();
    root->overlayOwner = view;
    int hits = 0;
    Button* open = root->emplace<Button>("Open", [&] { Context::current()->addOverlay(menuCounting(hits)); });
    ctx.setRoot(std::move(root));
    pump(ctx, r, 1);
    {
        Context::OwnerScope s(ctx, shell); // a host wrapping its dispatch
        clickOn(ctx, open);
    }
    REQUIRE(ctx.hasInteractiveOverlay());
    CHECK(actsFor(ctx.topOverlay(), view));
    CHECK(actsFor(ctx.topOverlay(), shell));
}

TEST_CASE("an overlay opened from another's onClosed acts for exactly that one's owners") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    auto owner = std::make_shared<int>(0), host = std::make_shared<int>(0);
    const auto chain = [&ctx] {
        auto next = std::make_unique<Modal>();
        next->setContent(std::make_unique<Box>());
        ctx.addOverlay(std::move(next));
    };
    auto owned = std::make_unique<Modal>();
    owned->setContent(std::make_unique<Box>());
    owned->onClosed = chain;
    owned->addOwner(owner);
    Overlay* o = ctx.addOverlay(std::move(owned));
    auto unowned = std::make_unique<Modal>();
    unowned->setContent(std::make_unique<Box>());
    unowned->onClosed = chain;
    Overlay* u = ctx.addOverlay(std::move(unowned));
    pump(ctx, r, 3);
    o->requestClose();
    u->requestClose();
    {
        Context::OwnerScope s(ctx, host); // a host wrapping frame() in its own scope
        pump(ctx, r, 80);
    }
    REQUIRE(ctx.overlayCount() == 2); // both follow-ups are up
    owner.reset();
    pump(ctx, r, 1);
    REQUIRE(ctx.overlayCount() == 1);          // the owned one's went with its owner
    CHECK(ctx.topOverlay()->owners().empty()); // the unowned one's took nothing from the host
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

TEST_CASE("a combo's dropdown also goes with the view around it") {
    StubRenderer r;
    Context ctx;
    auto view = std::make_shared<int>(0);
    auto root = std::make_unique<Box>();
    root->overlayOwner = view;
    Combo* c = root->emplace<Combo>(std::vector<std::string>{"Red", "Green"}, 0);
    ctx.setRoot(std::move(root));
    pump(ctx, r, 1);

    clickAt(ctx, c->rect.x + 5, c->rect.y + c->rect.h * 0.5f);
    REQUIRE(ctx.hasInteractiveOverlay());
    view.reset(); // the view goes; its widgets haven't been freed yet
    CHECK_FALSE(ctx.hasInteractiveOverlay());
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

TEST_CASE("closeOverlaysFor closes what an owner opened, as chosen, and leaves the rest") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    int rootClicks = 0;
    Button* under = root->emplace<Button>("Under", [&rootClicks] { ++rootClicks; });
    under->prefW = 800;
    under->prefH = 600;
    ctx.setRoot(std::move(root));

    auto owner = std::make_shared<int>(0), other = std::make_shared<int>(0);
    int hits = 0, menuClosed = 0, dialogClosed = 0, strayClosed = 0;
    auto dialog = std::make_unique<Modal>();
    dialog->setContent(std::make_unique<Box>());
    dialog->onClosed = [&dialogClosed] { ++dialogClosed; };
    dialog->addOwner(owner);
    Overlay* d = ctx.addOverlay(std::move(dialog));
    auto stray = menuCounting(hits); // someone else's, out of the way of the click below
    stray->anchorX = 500;
    stray->anchorY = 400;
    stray->onClosed = [&strayClosed] { ++strayClosed; };
    stray->addOwner(other);
    Overlay* s = ctx.addOverlay(std::move(stray));
    auto menu = menuCounting(hits); // acts for both: matched by either
    menu->onClosed = [&menuClosed] { ++menuClosed; };
    menu->addOwner(other);
    menu->addOwner(owner);
    Menu* m = static_cast<Menu*>(ctx.addOverlay(std::move(menu)));
    pump(ctx, r, 3);
    Widget* item = m->content()->children()[0].get();
    const float x = item->rect.x + item->rect.w * 0.5f, y = item->rect.y + item->rect.h * 0.5f;

    ctx.closeOverlaysFor(owner, [](const Overlay& o) { return dynamic_cast<const Menu*>(&o) != nullptr; });
    CHECK_FALSE(m->interactive); // not even while it fades
    CHECK(d->interactive);       // a dialog wasn't chosen
    CHECK(s->interactive);       // nor someone else's menu

    ctx.closeOverlaysFor(other, [](const Overlay&) { return false; }); // chooses nothing
    CHECK(s->interactive);

    d->requestClose(); // out of the way, so the click below meets only the fading menu
    s->requestClose();
    pump(ctx, r, 2);
    clickAt(ctx, x, y); // where the menu's item was, mid-fade
    CHECK(hits == 0);
    pump(ctx, r, 80);
    CHECK(menuClosed == 1); // a close, not a drop: onClosed fires, once
    CHECK(dialogClosed == 1);
    CHECK(strayClosed == 1);
    CHECK(ctx.overlayCount() == 0);
}

TEST_CASE("closeOverlaysFor with no filter closes everything the owner opened") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    auto owner = std::make_shared<int>(0);
    int hits = 0;
    auto menu = menuCounting(hits);
    menu->addOwner(owner);
    Overlay* m = ctx.addOverlay(std::move(menu));
    auto dialog = std::make_unique<Modal>();
    dialog->setContent(std::make_unique<Box>());
    dialog->addOwner(owner);
    Overlay* d = ctx.addOverlay(std::move(dialog));
    Overlay* unowned = ctx.addOverlay(menuCounting(hits));
    pump(ctx, r, 3);

    ctx.closeOverlaysFor(owner);
    ctx.closeOverlaysFor({}); // an empty owner matches nothing, including the unowned
    CHECK_FALSE(m->interactive);
    CHECK_FALSE(d->interactive);
    CHECK(unowned->interactive);
    pump(ctx, r, 80);
    CHECK(ctx.overlayCount() == 1);
}
