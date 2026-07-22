// Spry WASM demo host (#38): one module serving every demo scene. The scene id
// comes from argv[1] (the host page sets `Module.arguments = [scene]` from the URL
// `?scene=<id>`); the matching scene from scenes.h is built and driven. Same public
// API + SdlRenderer backend as the desktop examples; the browser main loop is
// emscripten_set_main_loop, and the font + themes load from the embedded FS.
// Also compiles natively (for testing) via the #else branch. Build: scripts/build-web.sh.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <algorithm>
#include <string>

#include "spry/sdl_host.h"     // pumpEvent for interactive scenes
#include "spry/sdl_renderer.h"
#include "spry/spry.h"

#include "scenes.h"

using namespace spry;

struct App {
    SDL_Window* win = nullptr;
    SDL_Renderer* sdl = nullptr;
    SdlRenderer* ren = nullptr;
    Context* ctx = nullptr;
    Uint64 last = 0;
    Theme midnight, ember;
    bool onEmber = false;
    bool running = true;
    const demos::Scene* scene = nullptr;
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
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)
            g.running = false;

        if (g.scene && g.scene->interactive) {
            pumpEvent(*g.ctx, e, g.win); // translate + dispatch real input to the widgets
        } else {
            // Showcase scenes: the click / T gesture crossfades the theme.
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_T)
                swapTheme();
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                swapTheme();
        }
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
    SDL_GetMouseState(&mx, &my);

    g.ren->beginFrame(g.ctx->displayedTheme().color("background", Color{17, 18, 23}));
    g.ctx->frame(*g.ren, dt, mx, my);
    g.ren->endFrame();
}

int main(int argc, char** argv) {
    SDL_SetMainReady();
    g.scene = demos::find(argc > 1 ? argv[1] : "theming");

    if (!SDL_Init(SDL_INIT_VIDEO))
        return 1;
    g.win = SDL_CreateWindow(g.scene ? g.scene->title : "Spry demo", 900, 560, 0);
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
    g.ctx->setRoot(g.scene ? g.scene->build() : demos::buildTheming());
    g.ctx->setThemeImmediate(g.midnight);
    if (g.scene && g.scene->interactive)
        installSdlHost(*g.ctx, g.win); // clipboard + text-input/IME for interactive scenes
    g.last = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1); // browser drives the loop
#else
    while (g.running) {
        frame();
        SDL_Delay(16);
    }
    delete g.ctx;
    delete g.ren;
    SDL_DestroyRenderer(g.sdl);
    SDL_DestroyWindow(g.win);
    SDL_Quit();
#endif
    return 0;
}
