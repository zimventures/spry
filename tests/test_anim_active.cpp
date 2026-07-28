// Tests for the animations-active query (#57): Spring::settled(), the
// Widget::animating() recursion, and Context::animationsActive() — the signal
// on-demand hosts use to stop presenting frames once the scene settles.
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

// Run frames until the context reports settled (or the cap trips).
int framesUntilSettled(Context& ctx, StubRenderer& r, int cap = 600) {
    int n = 0;
    while (ctx.animationsActive() && n < cap) {
        ctx.frame(r, 1.0f / 60.0f, 0, 0);
        ++n;
    }
    return n;
}

} // namespace

TEST_CASE("Spring reports settled and step() snaps to the target", "[anim_active]") {
    Spring s;
    s.target = 1.0f;
    REQUIRE_FALSE(s.settled());
    for (int i = 0; i < 600 && !s.settled(); ++i)
        s.step(1.0f / 60.0f);
    REQUIRE(s.settled());
    s.step(1.0f / 60.0f); // settled step snaps exactly
    REQUIRE(s.value == 1.0f);
    REQUIRE(s.vel == 0.0f);

    s.kick(4.0f); // a kick un-settles it
    REQUIRE_FALSE(s.settled());
}

TEST_CASE("Widget::animating recurses over visible children", "[anim_active]") {
    auto root = std::make_unique<Box>();
    auto* toggle = root->emplace<Toggle>("t");
    REQUIRE_FALSE(root->animating()); // springs start at their targets

    toggle->on = true; // knob spring now has a new target on the next update
    toggle->update(1.0f / 60.0f);
    REQUIRE(root->animating());

    toggle->visible = false; // hidden subtrees don't count
    REQUIRE_FALSE(root->animating());
}

TEST_CASE("Context::animationsActive settles after a kick and stays settled", "[anim_active]") {
    StubRenderer r;
    Context ctx;
    auto root = std::make_unique<Box>();
    auto* toggle = root->emplace<Toggle>("t");
    ctx.setRoot(std::move(root));
    ctx.setThemeImmediate(Theme::builtinDark());

    ctx.frame(r, 1.0f / 60.0f, 0, 0);
    REQUIRE_FALSE(ctx.animationsActive());

    toggle->on = true;
    ctx.frame(r, 1.0f / 60.0f, 0, 0);
    REQUIRE(ctx.animationsActive());

    int frames = framesUntilSettled(ctx, r);
    REQUIRE(frames > 0);
    REQUIRE(frames < 600);
    REQUIRE_FALSE(ctx.animationsActive());
}

TEST_CASE("theme transition counts as animation; setThemeImmediate does not", "[anim_active]") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    ctx.setThemeImmediate(Theme::builtinDark());
    ctx.frame(r, 1.0f / 60.0f, 0, 0);
    REQUIRE_FALSE(ctx.animationsActive());

    ctx.setTheme(Theme::builtinDark()); // animated crossfade
    REQUIRE(ctx.animationsActive());
    REQUIRE(framesUntilSettled(ctx, r) < 600);
}

TEST_CASE("overlay lifecycle counts as animation until fully open, and again on close", "[anim_active]") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    ctx.setThemeImmediate(Theme::builtinDark());
    ctx.frame(r, 1.0f / 60.0f, 0, 0);
    REQUIRE_FALSE(ctx.animationsActive());

    auto modal = std::make_unique<Modal>();
    modal->setContent(std::make_unique<Label>("hi"));
    Overlay* raw = modal.get();
    ctx.addOverlay(std::move(modal));
    REQUIRE(ctx.animationsActive()); // presence spring opening
    REQUIRE(framesUntilSettled(ctx, r) < 600);

    raw->requestClose();
    REQUIRE(ctx.animationsActive()); // closing animation in flight
    REQUIRE(framesUntilSettled(ctx, r) < 600); // overlay pruned, scene settles
}

TEST_CASE("an auto-close overlay stays active while its countdown runs", "[anim_active]") {
    StubRenderer r;
    Context ctx;
    ctx.setRoot(std::make_unique<Box>());
    ctx.setThemeImmediate(Theme::builtinDark());

    auto toast = std::make_unique<Toast>("hello", 0.5f); // auto-closes in 0.5s
    ctx.addOverlay(std::move(toast));

    // Through open, dwell, and auto-close: never a settled moment until gone.
    int frames = framesUntilSettled(ctx, r, 600);
    REQUIRE(frames < 600);
    REQUIRE_FALSE(ctx.animationsActive());
}
