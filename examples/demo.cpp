// Spry demo (#205 / seed for #223) — retained widget tree + flex layout (#209)
// + data-driven theming (#211), through the public API, decoupled from Cleat.
//   hover the cards   ·   resize the window   ·   press T to swap theme
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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

    root->emplace<Label>("Spry - tree + layout + theming (#209/#211)", 2.2f);
    auto* sub = root->emplace<Label>("hover the cards - resize the window - press T to swap theme", 1.4f);
    sub->role = "textDim";

    auto* row = root->emplace<Box>();
    row->axis = Axis::Row;
    row->spacing = 14;
    row->prefH = 130;
    const char* names[] = {"Terminal", "SFTP", "Connections", "Keys"};
    for (auto n : names) {
        auto* c = row->emplace<Card>(n);
        c->grow = 1.0f;
    }

    auto* content = root->emplace<Panel>();
    content->grow = 1.0f;
    auto* cbox = content->emplace<Box>();
    cbox->axis = Axis::Column;
    cbox->padding = Edges(18);
    cbox->spacing = 12;
    auto* cl = cbox->emplace<Label>("content area - all colours come from theme tokens", 1.5f);
    cl->role = "textDim";
    auto* lig = cbox->emplace<Label>("HarfBuzz shaping:  -> => != == >= <= |> <| :: www  (ligatures)", 1.6f);

    auto* bar = root->emplace<Box>();
    bar->axis = Axis::Row;
    bar->prefH = 30;
    bar->spacing = 16;
    auto* s1 = bar->emplace<Label>("ready", 1.4f);
    s1->colorOverride = Color{120, 200, 150}; // per-widget override beats the theme
    bar->emplace<Widget>()->grow = 1.0f;       // spacer
    auto* s2 = bar->emplace<Label>("spry::Theme - tokens from file, hot-swappable", 1.4f);
    s2->role = "textDim";

    return root;
}

struct App {
    SdlRenderer* ren;
    Context* ctx;
    Uint64 last;
};

static void renderFrame(App& app) {
    Uint64 now = SDL_GetTicks();
    float dt = std::min((now - app.last) / 1000.0f, 0.05f);
    app.last = now;

    float mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);

    app.ren->beginFrame(app.ctx->displayedTheme().color("background", Color{17, 18, 23}));
    app.ctx->frame(*app.ren, dt, mx, my);
    app.ren->endFrame();
}

static bool SDLCALL eventWatch(void* userdata, SDL_Event* e) {
    if (e->type == SDL_EVENT_WINDOW_RESIZED || e->type == SDL_EVENT_WINDOW_EXPOSED) {
        renderFrame(*static_cast<App*>(userdata));
    }
    return true;
}

int main(int, char**) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_Window* win = SDL_CreateWindow("Spry demo (#205)", 960, 640, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* sdl = SDL_CreateRenderer(win, nullptr);
    if (!win || !sdl) return 1;
    SDL_SetRenderVSync(sdl, 1);

    // Load themes from files next to the exe (fall back to the built-in dark).
    const char* base = SDL_GetBasePath();
    std::string dir = base ? std::string(base) + "themes/" : "themes/";
    Theme midnight = Theme::builtinDark();
    Theme::loadFromFile(dir + "midnight.theme", midnight);
    Theme ember = Theme::builtinDark();
    Theme::loadFromFile(dir + "ember.theme", ember);

    SdlRenderer ren(sdl);
    // Real text via FreeType (#212). Try a couple of common monospace/system
    // fonts; falls back to SDL's debug font if none load. The OSS demo will ship
    // a bundled OFL font later.
    const char* fonts[] = {"C:/Windows/Fonts/CascadiaCode.ttf", "C:/Windows/Fonts/CascadiaMono.ttf",
                           "C:/Windows/Fonts/consola.ttf",      "C:/Windows/Fonts/segoeui.ttf",
                           "/System/Library/Fonts/Menlo.ttc",
                           "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"};
    for (const char* f : fonts)
        if (ren.loadFont(f)) break;

    Context ctx;
    ctx.setRoot(buildUI());
    ctx.setThemeImmediate(midnight);
    bool onEmber = false;

    App app{&ren, &ctx, SDL_GetTicks()};
    SDL_AddEventWatch(eventWatch, &app);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_ESCAPE) running = false;
                if (e.key.key == SDLK_T) {
                    onEmber = !onEmber;
                    ctx.setTheme(onEmber ? ember : midnight); // animated crossfade
                }
            }
        }
        renderFrame(app);
    }

    SDL_RemoveEventWatch(eventWatch, &app);
    SDL_DestroyRenderer(sdl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
