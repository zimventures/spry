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

#include "../../examples/web/scenes.h" // the WASM demo scenes — captured as fallbacks

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
// Widget-catalog boards (#7): each groups a category so every built-in widget
// appears in a screenshot.
// ---------------------------------------------------------------------------
static Box* labeledPanel(Widget* parent, const char* title, float w, float h) {
    auto* p = parent->emplace<Panel>();
    p->prefW = w;
    p->prefH = h;
    auto* col = column(p, 12, 8);
    col->emplace<Label>(title, 1.2f);
    return col;
}

static void sceneCatControls(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(26);
    root->spacing = 14;
    root->emplace<Label>("Buttons & controls", 1.8f);
    auto* r = row(root.get(), 12);
    r->emplace<Button>("Primary", [] {});
    auto* bf = r->emplace<Button>("Focused", [] {});
    bf->focused = true; // show the keyboard focus ring
    auto* checks = row(root.get(), 20);
    checks->emplace<Checkbox>("Checked", true);
    checks->emplace<Checkbox>("Unchecked", false);
    auto* radios = row(root.get(), 20);
    radios->emplace<RadioButton>("Selected", true);
    radios->emplace<RadioButton>("Option", false);
    root->emplace<Toggle>("Toggle (on)", true);
    auto* sl = root->emplace<Slider>(0.0f, 100.0f, 60.0f);
    sl->prefW = 300;
    auto* pb = root->emplace<ProgressBar>();
    pb->value = 0.6f;
    pb->prefW = 300;
    ctx.setRoot(std::move(root));
}

static void sceneCatInputs(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(26);
    root->spacing = 14;
    root->emplace<Label>("Text input & selection", 1.8f);
    auto* tf = root->emplace<TextField>(std::string("Editable text"));
    tf->prefW = 320;
    auto* tf2 = root->emplace<TextField>();
    tf2->placeholder = "Placeholder…";
    tf2->prefW = 320;
    auto* ta = root->emplace<TextArea>(std::string("Multi-line\ntext area"));
    ta->prefW = 320;
    ta->prefH = 68;
    auto* cb = root->emplace<Combo>(std::vector<std::string>{"Small", "Medium", "Large"}, 1);
    cb->prefW = 220;
    auto* colors = row(root.get(), 16);
    colors->cross = Align::Start; // don't stretch the ColorField to the pad's height
    colors->emplace<ColorField>(Color{96, 126, 205});
    auto* pad = colors->emplace<ColorPickerPad>(Color{96, 126, 205});
    pad->prefW = 200;
    pad->prefH = 150;
    ctx.setRoot(std::move(root));
}

static void sceneCatText(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(26);
    root->spacing = 12;
    root->emplace<Label>("Text & surfaces", 1.8f);
    root->emplace<Label>("Label — a single line of text", 1.4f);
    root->emplace<Paragraph>("Paragraph wraps text across multiple lines and honors explicit "
                             "breaks; its height grows with the content.",
                             1.3f);
    auto* r = row(root.get(), 14);
    auto* c = r->emplace<Card>("Card");
    c->prefW = 150;
    c->prefH = 78;
    auto* p = r->emplace<Panel>();
    p->prefW = 150;
    p->prefH = 78;
    column(p, 16, 6)->emplace<Label>("Panel", 1.4f);
    // Image: a procedurally-generated RGBA gradient (buffer + handle persist across
    // the capture frames so the one-time upload survives).
    static unsigned char imgPix[72 * 72 * 4];
    static ImageHandle imgHandle = 0;
    for (int y = 0; y < 72; ++y)
        for (int x = 0; x < 72; ++x) {
            int i = (y * 72 + x) * 4;
            imgPix[i] = (unsigned char)(x * 3 + 40);
            imgPix[i + 1] = (unsigned char)(y * 3 + 40);
            imgPix[i + 2] = 200;
            imgPix[i + 3] = 255;
        }
    auto* img = r->emplace<Image>();
    img->pixels = imgPix;
    img->srcW = img->srcH = 72;
    img->drawW = img->drawH = 78;
    img->handle = &imgHandle;
    ctx.setRoot(std::move(root));
}

static void sceneCatData(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(20);
    root->spacing = 12;
    root->emplace<Label>("Data containers", 1.8f);

    auto* top = row(root.get(), 14);
    auto* lvCol = labeledPanel(top, "ListView", 236, 178);
    auto* lv = lvCol->emplace<ListView>();
    lv->items = {"Alpha", "Bravo", "Charlie", "Delta", "Echo"};
    lv->selected = 1;
    lv->prefH = 128;
    auto* tvCol = labeledPanel(top, "TreeView", 236, 178);
    auto* tv = tvCol->emplace<TreeView>();
    auto& src = tv->addRoot("src");
    src.expanded = true;
    src.add("main.cpp");
    src.add("util.h");
    tv->addRoot("README.md");
    tv->rebuild();
    tv->prefH = 128;
    auto* svCol = labeledPanel(top, "ScrollView", 236, 178);
    auto* sv = svCol->emplace<ScrollView>();
    sv->setContent(std::make_unique<Paragraph>(
        "A ScrollView is a fixed viewport over taller content — drag the scrollbar "
        "or use the wheel. This text is intentionally long so it overflows and a "
        "scrollbar appears on the right edge of the viewport.",
        1.2f));
    sv->prefW = 200;
    sv->prefH = 128;

    auto* bot = row(root.get(), 14);
    auto* tbCol = labeledPanel(bot, "Table", 300, 168);
    auto* tb = tbCol->emplace<Table>();
    tb->columns = {{"Name", 2.0f}, {"Size", 1.0f}};
    tb->rows = {{"main.cpp", "4.2 KB"}, {"util.h", "1.1 KB"}, {"README.md", "0.8 KB"}};
    tb->prefH = 116;
    auto* tabCol = labeledPanel(bot, "TabBar", 236, 168);
    auto* tab = tabCol->emplace<TabBar>();
    tab->tabs = {"Files", "Search", "Git"};
    tab->active = 0;
    ctx.setRoot(std::move(root));
}

static void sceneCatModal(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 10;
    root->emplace<Label>("Files", 1.8f);
    auto* p = root->emplace<Panel>();
    p->grow = 1.0f;
    ctx.setRoot(std::move(root));

    // A Modal centers host-arranged content (via setContent) and dims the page.
    auto modal = std::make_unique<Modal>();
    auto body = std::make_unique<Box>();
    body->axis = Axis::Column;
    body->padding = Edges(22);
    body->spacing = 12;
    body->prefW = 300;
    body->emplace<Label>("Delete file?", 1.6f);
    body->emplace<Paragraph>("This can't be undone.", 1.3f);
    auto* btns = row(body.get(), 10);
    btns->emplace<Button>("Cancel", [] {});
    btns->emplace<Button>("Delete", [] {});
    modal->setContent(std::move(body));
    ctx.addOverlay(std::move(modal));
}

static void sceneCatNotify(Context& ctx) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 12;
    root->emplace<Label>("Tooltip & Toast", 1.6f);
    ctx.setRoot(std::move(root));

    auto tip = std::make_unique<Tooltip>("A hover tooltip", 0.0f);
    tip->anchorX = 60;
    tip->anchorY = 70;
    ctx.addOverlay(std::move(tip));
    ctx.addOverlay(std::make_unique<Toast>("Saved to disk", 0.0f));
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
        // Widget-catalog boards (#7)
        {"cat-controls", 560, 430, &dark, sceneCatControls},
        {"cat-inputs", 580, 560, &dark, sceneCatInputs},
        {"cat-text", 620, 340, &dark, sceneCatText},
        {"cat-data", 760, 470, &dark, sceneCatData},
        {"cat-modal", 620, 400, &dark, sceneCatModal},
        {"cat-notify", 560, 320, &dark, sceneCatNotify},
        // WASM demo fallback stills (#40): rendered from the same scenes.h builders
        // the live demos use, so the no-JS/no-WASM fallback matches the live view.
        {"wasm/scene-theming", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildTheming()); }},
        {"wasm/scene-controls", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildControls()); }},
        {"wasm/scene-layout", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildLayout()); }},
        {"wasm/scene-animation", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildAnimation()); }},
    };

    int failures = 0;
    std::printf("Capturing %zu screenshots into %s\n", jobs.size(), dir.c_str());
    SDL_CreateDirectory((dir + "wasm").c_str()); // some jobs write into a wasm/ subdir
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
