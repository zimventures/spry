#pragma once
#include <SDL3/SDL.h>

#include <string>

#include "clipboard.h"
#include "context.h"
#include "input.h"

// Optional SDL host glue (#320). Spry's core is platform-agnostic: it consumes already-
// translated spry::InputEvent and installs host-provided clipboard / text-input handlers. This
// header packages the standard SDL3 wiring every SDL host would otherwise copy by hand —
// keycode translation, the event pump (with HiDPI point->pixel scaling and Cmd->Ctrl), and the
// clipboard + IME handlers. It is opt-in and NOT part of <spry/spry.h>: including it pulls in
// SDL3, and a non-SDL host translates events itself. Keep this thin — it's convenience, not a
// new layer.
namespace spry {

// Translate an SDL keycode to the spry::Key subset the toolkit acts on. Printable text arrives
// as SDL_EVENT_TEXT_INPUT (a Text event), not through here.
inline Key toKey(SDL_Keycode k) {
    switch (k) {
        case SDLK_TAB: return Key::Tab;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return Key::Enter;
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

// Scale window-point coordinates (what SDL mouse events carry) to Spry's pixel space, so
// hit-testing lines up on HiDPI displays. On a non-HiDPI window pixels == points (scale 1).
inline void mouseToSpry(SDL_Window* win, float px, float py, float& ox, float& oy) {
    int wp = 0, hp = 0, ww = 0, hh = 0;
    SDL_GetWindowSizeInPixels(win, &wp, &hp);
    SDL_GetWindowSize(win, &ww, &hh);
    ox = px * (ww > 0 ? (float)wp / ww : 1.0f);
    oy = py * (hh > 0 ? (float)hp / hh : 1.0f);
}

// Install SDL clipboard handlers on the toolkit (global; call once). Text fields then reach the
// system clipboard for cut/copy/paste.
inline void installSdlClipboard() {
    setClipboardHandlers(
        [] {
            char* t = SDL_GetClipboardText();
            std::string s = t ? t : "";
            SDL_free(t);
            return s;
        },
        [](const std::string& s) { SDL_SetClipboardText(s.c_str()); });
}

// Install SDL text-input / IME handling on a Context for `win` (call once). Starts/stops
// platform text input as focus enters/leaves a text widget and tracks the caret so the IME
// candidate window follows it (converting the caret from Spry pixels back to window points).
inline void installSdlTextInput(Context& ctx, SDL_Window* win) {
    ctx.setTextInputHandler([win](bool active, const Rect& caret) {
        if (!active) {
            if (SDL_TextInputActive(win)) SDL_StopTextInput(win);
            return;
        }
        if (!SDL_TextInputActive(win)) SDL_StartTextInput(win);
        int wp = 0, hp = 0, ww = 0, hh = 0;
        SDL_GetWindowSizeInPixels(win, &wp, &hp);
        SDL_GetWindowSize(win, &ww, &hh);
        float sx = wp > 0 ? (float)ww / wp : 1.0f, sy = hp > 0 ? (float)hh / hp : 1.0f;
        SDL_Rect r{(int)(caret.x * sx), (int)(caret.y * sy), (int)(caret.w * sx), (int)(caret.h * sy)};
        SDL_SetTextInputArea(win, &r, 0);
    });
}

// Install both the clipboard and text-input handlers.
inline void installSdlHost(Context& ctx, SDL_Window* win) {
    installSdlClipboard();
    installSdlTextInput(ctx, win);
}

// Translate one SDL event into a spry::InputEvent and dispatch it to `ctx`. Returns true if the
// event was a Spry input event (translated + dispatched), false otherwise — the host still
// handles SDL_EVENT_QUIT, window resize, etc. itself. Mouse coordinates are scaled to Spry
// pixels via `win`; Cmd is folded into Ctrl so editing chords work on macOS.
inline bool pumpEvent(Context& ctx, const SDL_Event& e, SDL_Window* win) {
    InputEvent ev;
    switch (e.type) {
        case SDL_EVENT_KEY_DOWN:
            ev.type = InputEvent::KeyDown;
            ev.key = toKey(e.key.key);
            ev.shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
            ev.ctrl = (e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0; // Cmd == Ctrl on macOS
            ev.alt = (e.key.mod & SDL_KMOD_ALT) != 0;
            break;
        case SDL_EVENT_TEXT_INPUT:
            ev.type = InputEvent::Text;
            ev.text = e.text.text;
            break;
        case SDL_EVENT_TEXT_EDITING: // IME composition (pre-edit)
            ev.type = InputEvent::TextEditing;
            ev.text = e.edit.text;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            ev.type = InputEvent::MouseMove;
            mouseToSpry(win, e.motion.x, e.motion.y, ev.x, ev.y);
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            ev.type = InputEvent::Wheel;
            ev.wheel = e.wheel.y;
            float px = 0, py = 0;
            SDL_GetMouseState(&px, &py); // wheel events carry no position; use the cursor
            mouseToSpry(win, px, py, ev.x, ev.y);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            // InputEvent buttons: 0 = left, 1 = right, 2 = middle. Extra buttons (X1/X2) have no
            // Spry mapping — return false so they aren't misread as left-clicks; the host can
            // handle them itself.
            int button = e.button.button == SDL_BUTTON_LEFT     ? 0
                         : e.button.button == SDL_BUTTON_RIGHT  ? 1
                         : e.button.button == SDL_BUTTON_MIDDLE ? 2
                                                                : -1;
            if (button < 0)
                return false;
            ev.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? InputEvent::MouseDown : InputEvent::MouseUp;
            mouseToSpry(win, e.button.x, e.button.y, ev.x, ev.y);
            ev.button = button;
            SDL_Keymod mod = SDL_GetModState();
            ev.shift = (mod & SDL_KMOD_SHIFT) != 0;
            ev.ctrl = (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
            break;
        }
        default:
            return false; // not a Spry input event — the host deals with it
    }
    ctx.handleEvent(ev);
    return true;
}

} // namespace spry
