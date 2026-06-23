// Spry GL demo (#208 / path to #217) — the same retained UI as the SDL demo, but
// rendered through the OpenGL backend in a host-owned GL context. This is the
// rendering path that will compose inside Cleat (SDL3 + OpenGL + ImGui).
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <memory>
#include <string>

#include "spry/gl_renderer.h"
#include "spry/spry.h"

using namespace spry;

static std::unique_ptr<Widget> buildUI() {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 16;

    root->emplace<Label>("Spry - OpenGL backend (#208)  ·  same UI, rendered via GL", 2.2f);
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
    auto* cl = cbox->emplace<Label>("rendered through the GL backend - the path to Cleat (#217)", 1.5f);
    cl->role = "textDim";
    cbox->emplace<Label>("HarfBuzz shaping:  -> => != == >= <= |> <| :: www  (ligatures)", 1.6f);

    auto* bar = root->emplace<Box>();
    bar->axis = Axis::Row;
    bar->prefH = 30;
    bar->spacing = 16;
    auto* s1 = bar->emplace<Label>("ready", 1.4f);
    s1->colorOverride = Color{120, 200, 150};
    bar->emplace<Widget>()->grow = 1.0f;
    auto* s2 = bar->emplace<Label>("spry::GlRenderer - OpenGL 3.3", 1.4f);
    s2->role = "textDim";
    return root;
}

struct App {
    SDL_Window* win;
    GlRenderer* ren;
    Context* ctx;
    Uint64 last;
};

static void renderFrame(App& app) {
    Uint64 now = SDL_GetTicks();
    float dt = std::min((now - app.last) / 1000.0f, 0.05f);
    app.last = now;

    int wp = 0, hp = 0, ww = 0, hh = 0;
    SDL_GetWindowSizeInPixels(app.win, &wp, &hp);
    SDL_GetWindowSize(app.win, &ww, &hh);
    app.ren->setSize(wp, hp);

    float mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    if (ww > 0) mx *= (float)wp / ww; // points -> pixels (HiDPI)
    if (hh > 0) my *= (float)hp / hh;

    app.ren->beginFrame(app.ctx->displayedTheme().color("background", Color{17, 18, 23}));
    app.ctx->frame(*app.ren, dt, mx, my);
    SDL_GL_SwapWindow(app.win);
}

static bool SDLCALL eventWatch(void* userdata, SDL_Event* e) {
    if (e->type == SDL_EVENT_WINDOW_RESIZED || e->type == SDL_EVENT_WINDOW_EXPOSED)
        renderFrame(*static_cast<App*>(userdata));
    return true;
}

int main(int, char**) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* win = SDL_CreateWindow("Spry GL demo (#208)", 960, 640,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) return 1;
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    if (!gl) {
        SDL_Log("GL context failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(win, gl);
    SDL_GL_SetSwapInterval(1); // vsync

    GlRenderer ren; // loads GL entry points from the current context
    const char* fonts[] = {"C:/Windows/Fonts/CascadiaCode.ttf", "C:/Windows/Fonts/CascadiaMono.ttf",
                           "C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/segoeui.ttf",
                           "/System/Library/Fonts/Menlo.ttc",
                           "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"};
    for (const char* f : fonts)
        if (ren.loadFont(f)) break;

    Context ctx;
    ctx.setRoot(buildUI());

    const char* base = SDL_GetBasePath();
    std::string dir = base ? std::string(base) + "themes/" : "themes/";
    Theme midnight = Theme::builtinDark();
    Theme::loadFromFile(dir + "midnight.theme", midnight);
    Theme ember = Theme::builtinDark();
    Theme::loadFromFile(dir + "ember.theme", ember);
    ctx.setThemeImmediate(midnight);
    bool onEmber = false;

    App app{win, &ren, &ctx, SDL_GetTicks()};
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
                    ctx.setTheme(onEmber ? ember : midnight);
                }
            }
        }
        renderFrame(app);
    }

    SDL_RemoveEventWatch(eventWatch, &app);
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
