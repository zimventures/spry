// Headless screenshot capture for the Spry docs (#11).
//
// Renders a fixed set of scenes × themes to PNG through an OFFSCREEN SDL software
// renderer — no window or display is needed — so `spry_capture` regenerates every
// site screenshot in one run (see docs/assets/README.md). It links only the public
// Spry API + SDL3, exactly like a consumer would.
//
// Usage:  spry_capture [output_dir]      (default: ./docs/assets)
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <spry/spry.h>
#include <spry/sdl_renderer.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace spry;

// ---------------------------------------------------------------------------
// Themes. builtinDark() is the only built-in; the docs also want a light theme,
// so define one here (kept in sync with examples/themes/daylight.theme).
// ---------------------------------------------------------------------------
static Theme builtinLight() {
    Theme t;
    t.colors = {
        {tokens::Background, {245, 246, 250}}, {tokens::Surface, {255, 255, 255}},
        {tokens::SurfaceAlt, {228, 231, 240}}, {tokens::Text, {26, 28, 36}},
        {tokens::TextDim, {110, 116, 132}},    {tokens::Accent, {74, 110, 210}},
        {tokens::AccentText, {248, 250, 255}}, {tokens::Scrim, {12, 14, 22, 150}},
    };
    t.metrics = {{tokens::Radius, 10.0f}, {"pad", 20.0f}};
    return t;
}

static bool loadAnyFont(SdlRenderer& ren) {
    const char* fonts[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };
    for (const char* f : fonts)
        if (ren.loadFont(f)) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Scenes. Each populates a Context (root tree + optional overlays). The capture
// driver sets the theme and runs a few frames so springs/overlays settle.
// ---------------------------------------------------------------------------
using SceneFn = std::function<void(Context&)>;

static Box* column(Widget* parent, float pad, float gap) {
    auto* b = parent->emplace<Box>();
    b->axis = Axis::Column;
    b->padding = Edges(pad);
    b->spacing = gap;
    return b;
}
static Box* row(Widget* parent, float gap) {
    auto* b = parent->emplace<Box>();
    b->axis = Axis::Row;
    b->spacing = gap;
    return b;
}

// The "hello" app: a heading, a panel with a counter label, and a button.
static void sceneHello(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(28);
    root->spacing = 18;
    root->emplace<Label>("Hello, Spry", 2.4f);

    auto* panel = root->emplace<Panel>();
    panel->grow = 1.0f;
    auto* pbox = column(panel, 20, 14);
    pbox->emplace<Label>("Clicked 3 times", 1.6f);
    auto* btn = pbox->emplace<Button>("Click me", [] {});
    btn->prefW = 150;
    ctx.setRoot(std::move(root));
}

// A control gallery: labels, buttons, a checkbox/radio/toggle, slider, progress.
static void sceneWidgets(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(28);
    root->spacing = 16;
    root->emplace<Label>("Widgets", 2.0f);

    auto* r1 = row(root.get(), 12);
    r1->emplace<Button>("Primary", [] {});
    r1->emplace<Button>("Secondary", [] {});

    auto* panel = root->emplace<Panel>();
    panel->grow = 1.0f;
    auto* col = column(panel, 20, 14);
    col->emplace<Checkbox>("Enable feature", true);
    col->emplace<RadioButton>("Option A", true);
    col->emplace<RadioButton>("Option B", false);
    auto* tg = col->emplace<Toggle>("Dark mode", true);
    (void)tg;
    auto* sl = col->emplace<Slider>(0.0f, 100.0f, 65.0f);
    sl->prefW = 260;
    auto* pb = col->emplace<ProgressBar>();
    pb->value = 0.65f;
    pb->prefW = 260;
    ctx.setRoot(std::move(root));
}

// A flex layout showcase: a row of cards, then a two-column split.
static void sceneLayout(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 16;
    root->emplace<Label>("Layout", 2.0f);

    auto* cards = row(root.get(), 14);
    for (const char* name : {"Inbox", "Drafts", "Sent"}) {
        auto* c = cards->emplace<Card>(name);
        c->prefW = 150;
        c->prefH = 90;
    }

    auto* split = row(root.get(), 16);
    split->grow = 1.0f;
    auto* left = split->emplace<Panel>();
    left->prefW = 200;
    column(left, 18, 10)->emplace<Label>("Sidebar", 1.4f);
    auto* right = split->emplace<Panel>();
    right->grow = 1.0f;
    column(right, 18, 10)->emplace<Paragraph>(
        "A grow=1 panel eats the leftover space, while the sidebar keeps its fixed width.", 1.3f);
    ctx.setRoot(std::move(root));
}

// An open context menu over a simple shell — the "interactive" shot.
static void sceneMenu(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 12;
    root->emplace<Label>("Right-click a row", 1.6f);
    auto* panel = root->emplace<Panel>();
    panel->grow = 1.0f;
    ctx.setRoot(std::move(root));

    auto menu = std::make_unique<Menu>();
    menu->anchorX = 90;
    menu->anchorY = 90;
    menu->addItem("Rename", [] {});
    menu->addItem("Duplicate", [] {});
    menu->addItem("Delete", [] {});
    ctx.addOverlay(std::move(menu));
}

// ---------------------------------------------------------------------------
// Capture driver.
// ---------------------------------------------------------------------------
static bool capture(const std::string& path, int w, int h, const Theme& theme, const SceneFn& scene) {
    SDL_Surface* surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        std::fprintf(stderr, "  SDL_CreateSurface: %s\n", SDL_GetError());
        return false;
    }
    SDL_Renderer* sr = SDL_CreateSoftwareRenderer(surf);
    if (!sr) {
        std::fprintf(stderr, "  SDL_CreateSoftwareRenderer: %s\n", SDL_GetError());
        SDL_DestroySurface(surf);
        return false;
    }
    {
        SdlRenderer ren(sr);
        loadAnyFont(ren);
        Context ctx;
        scene(ctx);
        ctx.setThemeImmediate(theme);
        // Run enough frames for springs / overlay open animations to settle.
        const float dt = 1.0f / 60.0f;
        for (int i = 0; i < 40; ++i) {
            ren.beginFrame(ctx.displayedTheme().color(tokens::Background));
            ctx.frame(ren, dt, -1.0f, -1.0f); // off-screen mouse: no hover highlight
            ren.endFrame();
        }
    }
    bool ok = SDL_LockSurface(surf);
    if (ok) {
        ok = stbi_write_png(path.c_str(), w, h, 4, surf->pixels, surf->pitch) != 0;
        SDL_UnlockSurface(surf);
    }
    std::printf("  %-40s %s\n", path.c_str(), ok ? "ok" : "FAILED");
    SDL_DestroyRenderer(sr);
    SDL_DestroySurface(surf);
    return ok;
}

// Dump a numbered PNG frame sequence of an animated theme crossfade (dark → light
// → dark) for `scene`, into `frameDir`. A separate ffmpeg step (see the media
// script / docs/assets/README.md) assembles these into a GIF.
static int captureCrossfadeFrames(const std::string& frameDir, int w, int h, const SceneFn& scene) {
    SDL_Surface* surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* sr = surf ? SDL_CreateSoftwareRenderer(surf) : nullptr;
    if (!sr) {
        std::fprintf(stderr, "  crossfade: %s\n", SDL_GetError());
        if (surf) SDL_DestroySurface(surf);
        return 0;
    }
    int n = 0;
    {
        SdlRenderer ren(sr);
        loadAnyFont(ren);
        Context ctx;
        scene(ctx);
        const Theme dark = Theme::builtinDark(), light = builtinLight();
        ctx.setThemeImmediate(dark);
        const float dt = 1.0f / 30.0f;
        auto renderFrame = [&] {
            ren.beginFrame(ctx.displayedTheme().color(tokens::Background));
            ctx.frame(ren, dt, -1.0f, -1.0f);
            ren.endFrame();
        };
        auto dump = [&] {
            char name[64];
            std::snprintf(name, sizeof name, "frame_%03d.png", n); // advance only on success
            bool ok = false;
            if (SDL_LockSurface(surf)) {
                ok = stbi_write_png((frameDir + "/" + name).c_str(), w, h, 4, surf->pixels, surf->pitch) != 0;
                SDL_UnlockSurface(surf);
            }
            if (ok)
                ++n; // keep the sequence gap-free so ffmpeg's frame_%03d input works
            else
                std::fprintf(stderr, "  frame write failed: %s\n", name);
        };
        for (int i = 0; i < 12; ++i) { renderFrame(); }    // warm-up so layout is stable
        ctx.setTheme(light);
        for (int i = 0; i < 16; ++i) { renderFrame(); dump(); }
        for (int i = 0; i < 8; ++i) { renderFrame(); dump(); } // hold on light
        ctx.setTheme(dark);
        for (int i = 0; i < 16; ++i) { renderFrame(); dump(); }
        for (int i = 0; i < 8; ++i) { renderFrame(); dump(); } // hold on dark
    }
    SDL_DestroyRenderer(sr);
    SDL_DestroySurface(surf);
    std::printf("  %-40s %d frames\n", (frameDir + "/frame_*.png").c_str(), n);
    return n;
}

int main(int argc, char** argv) {
    SDL_SetMainReady();
    // Initialize video where it's available (some platforms require it before
    // creating surfaces/renderers). On a truly headless box the init may fail —
    // the offscreen software renderer can still work, so warn and continue.
    if (!SDL_Init(SDL_INIT_VIDEO))
        std::fprintf(stderr, "SDL_Init(VIDEO) failed (%s); continuing headless\n", SDL_GetError());

    std::string dir = argc > 1 ? argv[1] : "docs/assets";
    if (!dir.empty() && dir.back() != '/') dir += '/';

    const Theme dark = Theme::builtinDark();
    const Theme light = builtinLight();

    struct Job {
        const char* name;
        int w, h;
        const Theme* theme;
        SceneFn scene;
    };
    std::vector<Job> jobs = {
        {"hello-dark", 720, 460, &dark, sceneHello},
        {"layout-dark", 820, 520, &dark, sceneLayout},
        {"widgets-dark", 720, 560, &dark, sceneWidgets},
        {"widgets-light", 720, 560, &light, sceneWidgets},
        {"menu-dark", 720, 460, &dark, sceneMenu},
    };

    int failures = 0;
    std::printf("Capturing %zu screenshots into %s\n", jobs.size(), dir.c_str());
    for (const auto& j : jobs)
        if (!capture(dir + j.name + ".png", j.w, j.h, *j.theme, j.scene)) ++failures;

    // GIF frame sequence (assembled into a .gif by the ffmpeg step). Frames go in a
    // subdir the media script feeds to ffmpeg and then deletes.
    std::string frameDir = dir + "_gifframes";
    SDL_CreateDirectory(frameDir.c_str());
    captureCrossfadeFrames(frameDir, 720, 560, sceneWidgets);

    std::printf("Done: %zu ok, %d failed\n", jobs.size() - failures, failures);
    SDL_Quit();
    return failures ? 1 : 0;
}
