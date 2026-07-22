// Spry web demo (WASM) — the layout + theming demo running natively in the browser
// via Emscripten. Same public API + SdlRenderer backend as examples/demo.cpp; the
// only differences are the browser main loop (emscripten_set_main_loop) and loading
// the font + themes from the embedded filesystem instead of disk. Build: see
// scripts/build-web.sh. Also compiles natively (for testing) via the #else branch.
#include <SDL3/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <algorithm>
#include <memory>
#include <string>

#include "spry/sdl_renderer.h"
#include "spry/spry.h"

using namespace spry;

static std::unique_ptr<Widget> buildUI() {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 16;

    root->emplace<Label>("Spry — tree + layout + theming", 2.2f);
    auto* sub = root->emplace<Label>("hover the cards · click (or press T) to swap theme", 1.4f);
    sub->role = "textDim";

    auto* row = root->emplace<Box>();
    row->axis = Axis::Row;
    row->spacing = 14;
    row->prefH = 130;
    for (const char* n : {"Terminal", "SFTP", "Connections", "Keys"}) {
        auto* c = row->emplace<Card>(n);
        c->grow = 1.0f;
    }

    auto* content = root->emplace<Panel>();
    content->grow = 1.0f;
    auto* cbox = content->emplace<Box>();
    cbox->axis = Axis::Column;
    cbox->padding = Edges(18);
    cbox->spacing = 12;
    auto* cl = cbox->emplace<Label>("content area — all colors come from theme tokens", 1.5f);
    cl->role = "textDim";
    cbox->emplace<Label>("HarfBuzz shaping:  -> => != == >= <= |> <| :: www  (ligatures)", 1.6f);

    auto* bar = root->emplace<Box>();
    bar->axis = Axis::Row;
    bar->prefH = 30;
    bar->spacing = 16;
    auto* s1 = bar->emplace<Label>("running in the browser via WebAssembly", 1.4f);
    s1->colorOverride = Color{120, 200, 150};
    bar->emplace<Widget>()->grow = 1.0f; // spacer
    auto* s2 = bar->emplace<Label>("spry::Theme — hot-swappable, animated crossfade", 1.4f);
    s2->role = "textDim";

    return root;
}

struct App {
    SdlRenderer* ren = nullptr;
    Context* ctx = nullptr;
    Uint64 last = 0;
    Theme midnight, ember;
    bool onEmber = false;
};
static App g;

static void swapTheme() {
    g.onEmber = !g.onEmber;
    g.ctx->setTheme(g.onEmber ? g.ember : g.midnight); // animated crossfade
}

static void frame() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_T)
            swapTheme();
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            swapTheme(); // click to swap — no keyboard focus needed in an iframe
    }
    Uint64 now = SDL_GetTicks();
    float dt = std::min((now - g.last) / 1000.0f, 0.05f);
    g.last = now;

    float mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my); // per-frame hover position (canvas-relative)

    g.ren->beginFrame(g.ctx->displayedTheme().color("background", Color{17, 18, 23}));
    g.ctx->frame(*g.ren, dt, mx, my);
    g.ren->endFrame();
}

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 1;
    SDL_Window* win = SDL_CreateWindow("Spry web demo", 900, 560, 0);
    SDL_Renderer* sdl = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
    if (!win || !sdl)
        return 1;

    static SdlRenderer ren(sdl); // static: outlives main() once the loop takes over
    g.ren = &ren;
    ren.loadFont("/font.ttf"); // embedded into the WASM filesystem at build time

    g.midnight = Theme::builtinDark();
    Theme::loadFromFile("/themes/midnight.theme", g.midnight);
    g.ember = Theme::builtinDark();
    Theme::loadFromFile("/themes/ember.theme", g.ember);

    static Context ctx;
    g.ctx = &ctx;
    ctx.setRoot(buildUI());
    ctx.setThemeImmediate(g.midnight);
    g.last = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1); // browser drives the loop
#else
    while (true) {
        frame();
        SDL_Delay(16);
    }
#endif
    return 0;
}
