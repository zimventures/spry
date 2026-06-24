// Spry GL demo (#208/#216) — the retained UI rendered through the OpenGL backend,
// now interactive: buttons + a checkbox, click / Tab-focus / Enter-activate.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>

#include "spry/gl_renderer.h"
#include "spry/spry.h"

using namespace spry;

static std::unique_ptr<Widget> buildUI(std::function<void()> onMidnight, std::function<void()> onEmber) {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 16;

    root->emplace<Label>("Spry - input, focus & keyboard (#216)", 2.2f);
    auto* sub = root->emplace<Label>("click the buttons - Tab to move focus - Enter/Space to activate", 1.4f);
    sub->role = "textDim";

    auto* row = root->emplace<Box>();
    row->axis = Axis::Row;
    row->spacing = 14;
    row->prefH = 120;
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
    auto* cl = cbox->emplace<Label>("the buttons below are real focusable controls (spry::Button/Checkbox)", 1.5f);
    cl->role = "textDim";
    cbox->emplace<Label>("HarfBuzz shaping:  -> => != == >= <= |> <| :: www  (ligatures)", 1.6f);

    // Editable fields (#213): click to place the caret, drag or shift-arrow to
    // select, double-click a word, Ctrl/Cmd C/X/V for clipboard, Ctrl+Z/Y undo.
    auto* tip = cbox->emplace<Label>("editable text below - select, clipboard, undo/redo, IME composition", 1.4f);
    tip->role = "textDim";
    auto* fields = cbox->emplace<Box>();
    fields->axis = Axis::Row;
    fields->spacing = 12;
    fields->prefH = 40;
    auto* host = fields->emplace<TextField>(std::string("user@example.com"));
    host->grow = 2.0f;
    auto* port = fields->emplace<TextField>(std::string("22"));
    port->grow = 1.0f;
    auto* empty = cbox->emplace<TextField>();
    empty->placeholder = "type here - IME composition works (e.g. CJK input)";

    // Interactive row (#216): buttons swap the theme; a checkbox toggles.
    auto* irow = root->emplace<Box>();
    irow->axis = Axis::Row;
    irow->spacing = 12;
    irow->prefH = 46;
    irow->emplace<Button>("Midnight theme", std::move(onMidnight));
    irow->emplace<Button>("Ember theme", std::move(onEmber));
    irow->emplace<Checkbox>("a toggle", true);
    irow->emplace<Widget>()->grow = 1.0f;

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
    app.ren->endFrame();
    app.ren->blitToDefault(wp, hp);
    SDL_GL_SwapWindow(app.win);
}

static bool SDLCALL eventWatch(void* userdata, SDL_Event* e) {
    if (e->type == SDL_EVENT_WINDOW_RESIZED || e->type == SDL_EVENT_WINDOW_EXPOSED)
        renderFrame(*static_cast<App*>(userdata));
    return true;
}

static Key toKey(SDL_Keycode k) {
    switch (k) {
        case SDLK_TAB: return Key::Tab;
        case SDLK_RETURN: return Key::Enter;
        case SDLK_ESCAPE: return Key::Escape;
        case SDLK_SPACE: return Key::Space;
        case SDLK_BACKSPACE: return Key::Backspace;
        case SDLK_DELETE: return Key::Delete;
        case SDLK_LEFT: return Key::Left;
        case SDLK_RIGHT: return Key::Right;
        case SDLK_UP: return Key::Up;
        case SDLK_DOWN: return Key::Down;
        case SDLK_HOME: return Key::Home;
        case SDLK_END: return Key::End;
        // Letters bound to editing chords (select-all, clipboard, undo/redo).
        case SDLK_A: return Key::A;
        case SDLK_C: return Key::C;
        case SDLK_V: return Key::V;
        case SDLK_X: return Key::X;
        case SDLK_Y: return Key::Y;
        case SDLK_Z: return Key::Z;
        default: return Key::None;
    }
}

// Translate the current point->pixel scale for mouse coords (HiDPI).
static void mouseToSpry(SDL_Window* win, float px, float py, float& ox, float& oy) {
    int wp = 0, hp = 0, ww = 0, hh = 0;
    SDL_GetWindowSizeInPixels(win, &wp, &hp);
    SDL_GetWindowSize(win, &ww, &hh);
    ox = px * (ww > 0 ? (float)wp / ww : 1.0f);
    oy = py * (hh > 0 ? (float)hp / hh : 1.0f);
}

int main(int, char**) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0); // match Cleat's GL 3.0 core context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* win = SDL_CreateWindow("Spry GL demo (#216)", 960, 660,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) return 1;
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    if (!gl) {
        SDL_Log("GL context failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(win, gl);
    SDL_GL_SetSwapInterval(1);

    GlRenderer ren;
    const char* fonts[] = {"C:/Windows/Fonts/CascadiaCode.ttf", "C:/Windows/Fonts/CascadiaMono.ttf",
                           "C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/segoeui.ttf",
                           "/System/Library/Fonts/Menlo.ttc",
                           "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"};
    for (const char* f : fonts)
        if (ren.loadFont(f)) break;

    Context ctx;
    const char* base = SDL_GetBasePath();
    std::string dir = base ? std::string(base) + "themes/" : "themes/";
    Theme midnight = Theme::builtinDark();
    Theme::loadFromFile(dir + "midnight.theme", midnight);
    Theme ember = Theme::builtinDark();
    Theme::loadFromFile(dir + "ember.theme", ember);

    ctx.setRoot(buildUI([&] { ctx.setTheme(midnight); }, [&] { ctx.setTheme(ember); }));
    ctx.setThemeImmediate(midnight);

    // Wire platform clipboard + IME for the text fields (#213). The toolkit core
    // stays SDL-free; the host installs these.
    setClipboardHandlers(
        [] {
            char* t = SDL_GetClipboardText();
            std::string s = t ? t : "";
            SDL_free(t);
            return s;
        },
        [](const std::string& s) { SDL_SetClipboardText(s.c_str()); });

    ctx.setTextInputHandler([win](bool active, const Rect& caret) {
        if (!active) {
            if (SDL_TextInputActive(win)) SDL_StopTextInput(win);
            return;
        }
        if (!SDL_TextInputActive(win)) SDL_StartTextInput(win);
        // caret is in Spry (FBO pixel) space; convert to window points for SDL.
        int wp = 0, hp = 0, ww = 0, hh = 0;
        SDL_GetWindowSizeInPixels(win, &wp, &hp);
        SDL_GetWindowSize(win, &ww, &hh);
        float sx = wp > 0 ? (float)ww / wp : 1.0f, sy = hp > 0 ? (float)hh / hp : 1.0f;
        SDL_Rect r{(int)(caret.x * sx), (int)(caret.y * sy), (int)(caret.w * sx), (int)(caret.h * sy)};
        SDL_SetTextInputArea(win, &r, 0);
    });

    App app{win, &ren, &ctx, SDL_GetTicks()};
    SDL_AddEventWatch(eventWatch, &app);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN: {
                    if (e.key.key == SDLK_ESCAPE) running = false;
                    InputEvent ev;
                    ev.type = InputEvent::KeyDown;
                    ev.key = toKey(e.key.key);
                    ev.shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
                    // Treat Cmd (GUI) as Ctrl so the editing chords work on macOS.
                    ev.ctrl = (e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
                    ev.alt = (e.key.mod & SDL_KMOD_ALT) != 0;
                    ctx.handleEvent(ev);
                    break;
                }
                case SDL_EVENT_TEXT_INPUT: {
                    InputEvent ev;
                    ev.type = InputEvent::Text;
                    ev.text = e.text.text;
                    ctx.handleEvent(ev);
                    break;
                }
                case SDL_EVENT_TEXT_EDITING: { // IME composition (pre-edit)
                    InputEvent ev;
                    ev.type = InputEvent::TextEditing;
                    ev.text = e.edit.text;
                    ctx.handleEvent(ev);
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION: {
                    InputEvent ev;
                    ev.type = InputEvent::MouseMove;
                    mouseToSpry(win, e.motion.x, e.motion.y, ev.x, ev.y);
                    ctx.handleEvent(ev);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    InputEvent ev;
                    ev.type = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? InputEvent::MouseDown : InputEvent::MouseUp;
                    mouseToSpry(win, e.button.x, e.button.y, ev.x, ev.y);
                    ev.button = (int)e.button.button - 1; // SDL: 1 = left
                    ev.shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                    ctx.handleEvent(ev);
                    break;
                }
                default:
                    break;
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
