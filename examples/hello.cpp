// "Hello, Spry" — the minimal one-screen example for the public API (#207).
//
// It shows the whole consumer contract in ~90 lines: create an SDL window + the
// SdlRenderer backend, build a retained Widget tree, drive it from a Context, and
// translate platform events into spry::InputEvent so a Button actually clicks.
//
// Build (standalone):  cmake -B build -G Ninja && cmake --build build --target spry_hello
// Run:                 ./build/spry_hello
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <memory>
#include <string>

#include <spry/sdl_host.h>     // SDL event pump (opt-in; not in the umbrella header)
#include <spry/sdl_renderer.h> // concrete backend (opt-in; not in the umbrella header)
#include <spry/spry.h>         // umbrella: Context, Widget, Box, widgets, Theme, ...

using namespace spry;

int main(int, char**) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 1;
    SDL_Window* win = SDL_CreateWindow("Hello, Spry", 640, 400, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* sdl = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
    if (!win || !sdl) {
        if (win)
            SDL_DestroyWindow(win); // clean up on the failure path, even though we're exiting
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(sdl, 1);

    // The renderer is a backend the host owns; the widgets never see SDL.
    SdlRenderer ren(sdl);
    const char* fonts[] = {"/System/Library/Fonts/Menlo.ttc",
                           "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
                           "C:/Windows/Fonts/consola.ttf"};
    for (const char* f : fonts)
        if (ren.loadFont(f))
            break; // else falls back to SDL's debug font

    // Build the retained tree: a column with a heading, a panel, and a button that
    // mutates a label. emplace<T>() constructs a child in place and returns it.
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 16;
    root->emplace<Label>("Hello, Spry", 2.4f);

    auto* panel = root->emplace<Panel>();
    panel->grow = 1.0f;
    auto* pbox = panel->emplace<Box>();
    pbox->axis = Axis::Column;
    pbox->padding = Edges(18);
    pbox->spacing = 12;
    Label* counter = pbox->emplace<Label>("Clicked 0 times", 1.6f);
    int clicks = 0;
    auto* button = pbox->emplace<Button>("Click me", [counter, &clicks] {
        ++clicks;
        counter->text = "Clicked " + std::to_string(clicks) + (clicks == 1 ? " time" : " times");
    });
    button->prefW = 140;

    Context ctx;
    ctx.setRoot(std::move(root));
    ctx.setThemeImmediate(Theme::builtinDark());

    bool running = true;
    Uint64 last = SDL_GetTicks();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            // spry/sdl_host.h's pumpEvent translates + dispatches the input events; the host
            // still owns app lifecycle (quit).
            if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE))
                running = false;
            pumpEvent(ctx, e, win);
        }

        Uint64 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        // frame()'s mouse must be in the same Spry pixel space as pumpEvent's, or hover/drag
        // drift apart on HiDPI. mouseToSpry scales window points -> pixels (a no-op at 1x).
        float mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);
        mouseToSpry(win, mx, my, mx, my);

        ren.beginFrame(ctx.displayedTheme().color("background", Color{17, 18, 23}));
        ctx.frame(ren, dt, mx, my); // layout -> hover -> update -> draw
        ren.endFrame();             // presents
    }

    SDL_DestroyRenderer(sdl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
