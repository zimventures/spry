// Headless tests for the text editing model + field (#213). EditBuffer is pure
// logic; TextField is driven through Context with a stub renderer that supplies
// deterministic text metrics (matching test_input.cpp's 7px/char model).
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

void typeText(Context& ctx, const char* s) {
    InputEvent e;
    e.type = InputEvent::Text;
    e.text = s;
    ctx.handleEvent(e);
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

TEST_CASE("EditBuffer: insert, caret and selection") {
    EditBuffer b;
    b.insert("hello");
    REQUIRE(b.text() == "hello");
    REQUIRE(b.caret() == 5);
    REQUIRE_FALSE(b.hasSelection());

    b.moveHome(false);
    b.moveRight(false, true); // select 'h'
    b.moveRight(false, true); // select 'he'
    REQUIRE(b.selectedText() == "he");

    b.insert("HE"); // replaces selection
    REQUIRE(b.text() == "HEllo");
    REQUIRE(b.caret() == 2);
}

TEST_CASE("EditBuffer: backspace, delete and word motion") {
    EditBuffer b;
    b.setText("one two three");
    b.moveHome(false);
    b.moveRight(true, false); // jump past "one"
    REQUIRE(b.caret() == 3);
    b.moveRight(true, false); // past " two"
    REQUIRE(b.caret() == 7);

    b.moveEnd(false);
    b.backspace();
    REQUIRE(b.text() == "one two thre");
    b.moveHome(false);
    b.del();
    REQUIRE(b.text() == "ne two thre");
}

TEST_CASE("EditBuffer: clipboard copy/cut/paste") {
    setClipboardHandlers({}, {}); // use the in-process fallback buffer
    EditBuffer b;
    b.setText("copyme");
    b.selectAll();
    b.copy();
    b.moveEnd(false);
    b.paste();
    REQUIRE(b.text() == "copymecopyme");

    b.setText("abcdef");
    b.moveHome(false);
    b.moveRight(false, true);
    b.moveRight(false, true);
    b.moveRight(false, true);
    b.cut(); // removes "abc"
    REQUIRE(b.text() == "def");
    b.moveEnd(false);
    b.paste();
    REQUIRE(b.text() == "defabc");
}

TEST_CASE("EditBuffer: undo coalesces a typing run, redo restores") {
    EditBuffer b;
    b.insert("a");
    b.insert("b");
    b.insert("c");
    REQUIRE(b.text() == "abc");
    b.undo(); // the whole typing run is one step
    REQUIRE(b.text() == "");
    b.redo();
    REQUIRE(b.text() == "abc");

    // A non-typing edit breaks the run into its own step.
    b.backspace();
    REQUIRE(b.text() == "ab");
    b.undo();
    REQUIRE(b.text() == "abc");
}

TEST_CASE("EditBuffer: double-click word selection") {
    EditBuffer b;
    b.setText("alpha beta gamma");
    b.selectWordAt(7); // inside "beta"
    REQUIRE(b.selectedText() == "beta");
    b.selectWordAt(0);
    REQUIRE(b.selectedText() == "alpha");
}

TEST_CASE("EditBuffer: wordBoundsAt + setSelection") {
    EditBuffer b;
    b.setText("alpha beta gamma");
    std::size_t lo = 0, hi = 0;
    b.wordBoundsAt(8, lo, hi); // inside "beta"
    REQUIRE(lo == 6);
    REQUIRE(hi == 10);
    b.setSelection(6, 10);
    REQUIRE(b.selectedText() == "beta");
    REQUIRE(b.caret() == 10); // caret lands on the second argument
}

TEST_CASE("EditBuffer: UTF-8 caret moves by codepoint") {
    EditBuffer b;
    b.setText("a\xC3\xA9z"); // bytes: 'a', U+00E9 (e-acute, 2 bytes), 'z'
    b.moveHome(false);
    b.moveRight(false, false);
    REQUIRE(b.caret() == 1);
    b.moveRight(false, false); // step over the whole accented char
    REQUIRE(b.caret() == 3);
    b.backspace(); // removes it as one unit
    REQUIRE(b.text() == "az");
}

TEST_CASE("TextField: focus, typing and Enter submit through Context") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    TextField* tf = root->emplace<TextField>();
    int submits = 0;
    tf->onSubmit = [&] { submits++; };
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);

    ctx.setFocus(tf);
    typeText(ctx, "hi");
    REQUIRE(tf->text() == "hi");
    key(ctx, Key::Enter);
    REQUIRE(submits == 1);
}

TEST_CASE("TextField: shift+arrow selects, Ctrl+C/Ctrl+V via the field") {
    setClipboardHandlers({}, {});
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    TextField* tf = root->emplace<TextField>(std::string("word"));
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);
    ctx.setFocus(tf);

    key(ctx, Key::End);
    key(ctx, Key::Left, /*shift=*/true);
    key(ctx, Key::Left, /*shift=*/true); // select "rd"
    key(ctx, Key::C, /*shift=*/false, /*ctrl=*/true);
    key(ctx, Key::End);
    key(ctx, Key::V, /*shift=*/false, /*ctrl=*/true);
    REQUIRE(tf->text() == "wordrd");
}

TEST_CASE("TextField: click places caret, drag extends selection") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    TextField* tf = root->emplace<TextField>(std::string("abcdef"));
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1); // lays out + paints (sets the field's rect + renderer)

    // Click at the far left -> caret at 0.
    InputEvent d;
    d.type = InputEvent::MouseDown;
    d.x = tf->rect.x + 1.0f;
    d.y = tf->rect.y + tf->rect.h * 0.5f;
    ctx.handleEvent(d);
    REQUIRE(ctx.focused() == tf);

    // Drag to the far right selects through to the end.
    InputEvent m;
    m.type = InputEvent::MouseMove;
    m.x = tf->rect.x + tf->rect.w + 500.0f;
    m.y = d.y;
    ctx.handleEvent(m);
    InputEvent u = m;
    u.type = InputEvent::MouseUp;
    ctx.handleEvent(u);
    REQUIRE(tf->text() == "abcdef");
    // Typing replaces the whole (selected) contents.
    typeText(ctx, "Z");
    REQUIRE(tf->text() == "Z");
}

TEST_CASE("TextField: double-click selects a whole word and the drag keeps it") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    TextField* tf = root->emplace<TextField>(std::string("alpha beta gamma"));
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);

    float cy = tf->rect.y + tf->rect.h * 0.5f;
    float betaX = tf->rect.x + 10.0f + 8.0f * (7.0f * 1.4f); // ~byte 8, inside "beta"
    auto down = [&](float x) {
        InputEvent d;
        d.type = InputEvent::MouseDown;
        d.x = x;
        d.y = cy;
        ctx.handleEvent(d);
    };

    down(betaX);
    down(betaX);                       // double-click -> "beta"
    ctx.frame(r, 0.016f, betaX, cy);   // Context sustains the drag for a frame (the old bug)
    InputEvent u;
    u.type = InputEvent::MouseUp;
    u.x = betaX;
    u.y = cy;
    ctx.handleEvent(u);
    typeText(ctx, "Z");
    REQUIRE(tf->text() == "alpha Z gamma"); // the whole word was replaced, not a fragment
}

TEST_CASE("TextField: triple-click selects the whole line") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    TextField* tf = root->emplace<TextField>(std::string("alpha beta gamma"));
    ctx.setRoot(std::move(root));
    ctx.frame(r, 0.016f, -1, -1);

    float cy = tf->rect.y + tf->rect.h * 0.5f;
    float x = tf->rect.x + 10.0f + 8.0f * (7.0f * 1.4f);
    for (int i = 0; i < 3; ++i) { // three quick clicks at the same spot
        InputEvent d;
        d.type = InputEvent::MouseDown;
        d.x = x;
        d.y = cy;
        ctx.handleEvent(d);
    }
    typeText(ctx, "Q");
    REQUIRE(tf->text() == "Q"); // everything was selected
}
