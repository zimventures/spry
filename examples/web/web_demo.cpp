// Spry web demo (WASM) — the layout + theming demo running natively in the browser
// via Emscripten. Same public API + SdlRenderer backend as examples/demo.cpp; the
// only differences are the browser main loop (emscripten_set_main_loop) and loading
// the font + themes from the embedded filesystem instead of disk. Build: see
// scripts/build-web.sh. Also compiles natively (for testing) via the #else branch.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
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
    SDL_Window* win = nullptr;
    SDL_Renderer* sdl = nullptr;
    SdlRenderer* ren = nullptr; // heap-owned so cleanup order is explicit (see main)
    Context* ctx = nullptr;
    Uint64 last = 0;
    Theme midnight, ember;
    bool onEmber = false;
    bool running = true;
};
static App g;

static void swapTheme() {
    g.onEmber = !g.onEmber;
    g.ctx->setTheme(g.onEmber ? g.ember : g.midnight); // animated crossfade
}

static void frame() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT)
            g.running = false;
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_ESCAPE)
                g.running = false;
            if (e.key.key == SDLK_T)
                swapTheme();
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            swapTheme(); // click to swap — no keyboard focus needed in an iframe
    }
#ifdef __EMSCRIPTEN__
    if (!g.running) {
        emscripten_cancel_main_loop();
        return;
    }
#endif
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
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 1;
    g.win = SDL_CreateWindow("Spry web demo", 900, 560, 0);
    g.sdl = g.win ? SDL_CreateRenderer(g.win, nullptr) : nullptr;
    if (!g.win || !g.sdl)
        return 1;

    g.ren = new SdlRenderer(g.sdl);
    g.ren->loadFont("/font.ttf"); // embedded into the WASM filesystem at build time

    g.midnight = Theme::builtinDark();
    Theme::loadFromFile("/themes/midnight.theme", g.midnight);
    g.ember = Theme::builtinDark();
    Theme::loadFromFile("/themes/ember.theme", g.ember);

    g.ctx = new Context();
    g.ctx->setRoot(buildUI());
    g.ctx->setThemeImmediate(g.midnight);
    g.last = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1); // browser drives the loop
#else
    while (g.running) {
        frame();
        SDL_Delay(16);
    }
    // Native cleanup (destroy the renderer wrapper before the SDL renderer it uses).
    delete g.ctx;
    delete g.ren;
    SDL_DestroyRenderer(g.sdl);
    SDL_DestroyWindow(g.win);
    SDL_Quit();
#endif
    return 0;
}
