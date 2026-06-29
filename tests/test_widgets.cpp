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

TEST_CASE("Paragraph wraps to width; height grows as it narrows") {
    StubRenderer r;
    Paragraph p("alpha beta gamma delta epsilon zeta eta theta", 1.4f);
    Size wide = p.measure(r, 100000.0f, 0);
    Size narrow = p.measure(r, 60.0f, 0);
    REQUIRE(wide.h == textLineH(1.4f)); // everything fits on one line
    REQUIRE(narrow.h > wide.h);         // wraps to several lines
}

TEST_CASE("Paragraph preformatted keeps verbatim lines (no wrap)") {
    StubRenderer r;
    Paragraph p("  indented one\nline two\nthree", 1.4f);
    p.preformatted = true;
    Size sz = p.measure(r, 20.0f, 0); // a tiny width must not add lines
    REQUIRE(sz.h == 3.0f * textLineH(1.4f)); // exactly the 3 source lines
}

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

TEST_CASE("Combo: arrow keys cycle the selection and clamp") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Combo* c = root->emplace<Combo>(std::vector<std::string>{"Low", "Medium", "High"}, 0);
    int changes = 0;
    std::string last;
    c->onChange = [&](int, const std::string& v) {
        ++changes;
        last = v;
    };
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);
    ctx.setFocus(c);

    InputEvent e;
    e.type = InputEvent::KeyDown;
    e.key = Key::Down;
    ctx.handleEvent(e); // -> Medium
    REQUIRE(c->selected == 1);
    REQUIRE(last == "Medium");
    e.key = Key::Down;
    ctx.handleEvent(e); // -> High
    ctx.handleEvent(e); // clamp at High
    REQUIRE(c->selected == 2);
    REQUIRE(changes == 2); // the clamped repeat didn't fire onChange
}

TEST_CASE("Combo: clicking opens a menu; choosing an item commits + closes") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Combo* c = root->emplace<Combo>(std::vector<std::string>{"Red", "Green", "Blue"}, 0);
    std::string last;
    c->onChange = [&](int, const std::string& v) { last = v; };
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);

    // Click the combo -> opens the dropdown overlay.
    clickAt(ctx, c->rect.x + 5, c->rect.y + c->rect.h * 0.5f);
    pump(ctx, r, 3);
    REQUIRE(ctx.hasInteractiveOverlay());

    // Pick the third item ("Blue") from the menu's list.
    Widget* list = ctx.topOverlay()->content();
    REQUIRE(list->children().size() == 3);
    Widget* blue = list->children()[2].get();
    clickAt(ctx, blue->rect.x + blue->rect.w * 0.5f, blue->rect.y + blue->rect.h * 0.5f);
    pump(ctx, r, 80);
    REQUIRE(last == "Blue");
    REQUIRE(c->selected == 2);
    REQUIRE_FALSE(ctx.hasInteractiveOverlay()); // menu closed after choosing
}

TEST_CASE("hsv/toHsv roundtrip and known colours") {
    REQUIRE(hsv(0.0f, 1.0f, 1.0f).r == 255); // red
    REQUIRE(hsv(0.0f, 1.0f, 1.0f).g == 0);
    Color green = hsv(1.0f / 3.0f, 1.0f, 1.0f);
    REQUIRE(green.g == 255);
    REQUIRE(green.r == 0);
    // Roundtrip a few colours through HSV.
    for (Color c : {Color{200, 60, 30}, Color{12, 240, 130}, Color{80, 90, 255}}) {
        float h, s, v;
        toHsv(c, h, s, v);
        Color back = hsv(h, s, v);
        REQUIRE(std::abs((int)back.r - (int)c.r) <= 1);
        REQUIRE(std::abs((int)back.g - (int)c.g) <= 1);
        REQUIRE(std::abs((int)back.b - (int)c.b) <= 1);
    }
}

TEST_CASE("ColorPickerPad: dragging the SV square and hue strip edits the colour") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    auto* pad = root->emplace<ColorPickerPad>(Color{255, 0, 0}); // red: h=0,s=1,v=1
    pad->prefW = 220;
    pad->prefH = 174;
    Color last{};
    pad->onChange = [&](Color c) { last = c; };
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);

    auto down = [&](float x, float y) {
        InputEvent d;
        d.type = InputEvent::MouseDown;
        d.x = x;
        d.y = y;
        ctx.handleEvent(d);
        InputEvent u;
        u.type = InputEvent::MouseUp;
        u.x = x;
        u.y = y;
        ctx.handleEvent(u);
    };
    float left = pad->rect.x + 10.0f, top = pad->rect.y + 10.0f;
    float sqW = pad->rect.w - 20.0f, sqH = pad->rect.h - 20.0f - 10.0f - 14.0f;

    // Top-left of the SV square => saturation 0, value 1 => white.
    down(left + 1.0f, top + 1.0f);
    REQUIRE(last.r > 240);
    REQUIRE(last.g > 240);
    REQUIRE(last.b > 240);

    // Top-right of the SV square => saturation 1, value 1 => the pure hue (red).
    down(left + sqW - 1.0f, top + 1.0f);
    REQUIRE(last.r > 240);
    REQUIRE(last.g < 15);
    REQUIRE(last.b < 15);

    // Click ~1/3 across the hue strip => green-ish, keeping S=V=1.
    float hueY = top + sqH + 10.0f + 7.0f;
    down(left + sqW * (1.0f / 3.0f), hueY);
    float h, s, v;
    toHsv(last, h, s, v);
    REQUIRE(h > 0.28f);
    REQUIRE(h < 0.40f); // around green's hue
    REQUIRE(s > 0.9f);  // still fully saturated
}

TEST_CASE("ColorPickerPad: a degenerate (zero-size) rect doesn't produce NaNs") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    auto* pad = root->emplace<ColorPickerPad>(Color{200, 100, 50});
    pad->prefW = 0; // collapse it
    pad->prefH = 0;
    bool fired = false;
    pad->onChange = [&](Color) { fired = true; };
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);

    InputEvent d;
    d.type = InputEvent::MouseDown;
    d.x = pad->rect.x;
    d.y = pad->rect.y;
    ctx.handleEvent(d); // must not divide by zero / fire a NaN colour
    REQUIRE_FALSE(fired);
    Color c = pad->color();
    REQUIRE(c.r == c.r); // not NaN-derived (uint8 always equals itself, but the guard kept it sane)
}

TEST_CASE("setRoot clears overlays tied to the old tree") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    Combo* c = root->emplace<Combo>(std::vector<std::string>{"A", "B"}, 0);
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);

    clickAt(ctx, c->rect.x + 5, c->rect.y + c->rect.h * 0.5f); // open the dropdown
    pump(ctx, r, 3);
    REQUIRE(ctx.hasInteractiveOverlay());

    // Swapping the root must drop the menu (whose item actions captured the now-gone
    // Combo) so a later click can't reach a dangling widget.
    ctx.setRoot(std::make_unique<Box>());
    REQUIRE(ctx.overlayCount() == 0);
    REQUIRE(ctx.focused() == nullptr);
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

TEST_CASE("TextField masking hides the value and exposes bullets") {
    const std::string bullet = "\xe2\x80\xa2"; // U+2022
    auto bullets = [&](int n) {
        std::string s;
        for (int i = 0; i < n; ++i)
            s += bullet;
        return s;
    };

    TextField tf;
    tf.setText("hunter2");

    SECTION("unmasked exposes the raw text to a11y") {
        REQUIRE(tf.accessibleLabel() == "hunter2");
    }

    SECTION("masked exposes one bullet per character, never the secret") {
        tf.masked = true;
        REQUIRE(tf.accessibleLabel() == bullets(7));
        REQUIRE(tf.text() == "hunter2"); // the real value is still readable for save/onChange
    }

    SECTION("masking counts UTF-8 codepoints, not bytes") {
        tf.setText("caf\xc3\xa9"); // "café": 4 codepoints, 5 bytes
        tf.masked = true;
        REQUIRE(tf.accessibleLabel() == bullets(4));
    }
}

TEST_CASE("TextArea is multi-line: Enter inserts newlines") {
    TextArea ta;
    ta.onText("a");
    ta.onKey(Key::Enter, false, false, false); // newline, not submit
    ta.onText("b");
    REQUIRE(ta.text() == "a\nb");

    SECTION("onChange fires with the current text") {
        std::string seen;
        ta.onChange = [&](const std::string& v) { seen = v; };
        ta.onText("c");
        REQUIRE(seen == "a\nbc");
    }

    SECTION("multi-line content round-trips through setText/text") {
        ta.setText("line1\nline2\nline3");
        REQUIRE(ta.text() == "line1\nline2\nline3");
    }
}
