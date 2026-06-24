#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "anim.h"
#include "input.h"
#include "overlay.h"
#include "renderer.h"
#include "theme.h"
#include "widget.h"

// Owns the root widget + the active theme, and drives a frame: layout -> hover
// hit-test -> update -> draw. Theme swaps are animated by interpolating tokens.
namespace spry {

class Context {
public:
    Context() {
        to_ = Theme::builtinDark();
        from_ = to_;
        displayed_ = to_;
    }

    void setRoot(std::unique_ptr<Widget> root) {
        root_ = std::move(root);
        hovered_ = nullptr;
    }
    Widget* root() const { return root_.get(); }

    // Swap with no transition (e.g. initial theme).
    void setThemeImmediate(Theme th) {
        to_ = std::move(th);
        from_ = to_;
        displayed_ = to_;
        trans_ = 1.0f;
    }
    // Swap with an animated crossfade.
    void setTheme(Theme th) {
        from_ = displayed_;
        to_ = std::move(th);
        trans_ = 0.0f;
    }
    const Theme& displayedTheme() const { return displayed_; }

    // ---- input, focus & keyboard (#216) ----
    Widget* focused() const { return focused_; }
    void setFocus(Widget* w) {
        if (w == focused_) return;
        if (focused_) {
            focused_->focused = false;
            focused_->onFocusChanged(false);
        }
        focused_ = w;
        if (focused_) {
            focused_->focused = true;
            focused_->onFocusChanged(true);
        }
    }
    // Host installs platform text input here (#213). Called with active=true and
    // the caret rect when a text-editing widget is focused (each frame, so the IME
    // candidate window tracks the caret), and active=false when focus leaves it.
    // Wire it to SDL_StartTextInput / SDL_SetTextInputArea / SDL_StopTextInput.
    void setTextInputHandler(std::function<void(bool active, const Rect& caret)> fn) {
        textInput_ = std::move(fn);
    }

    // ---- overlays (#214) ----
    // Push a transient layer (modal/menu/tooltip/toast). It draws above the root,
    // and — if interactive — captures input until dismissed. Context removes it
    // once its close animation finishes.
    Overlay* addOverlay(std::unique_ptr<Overlay> o) {
        Overlay* p = o.get();
        overlays_.push_back(std::move(o));
        return p;
    }
    bool hasInteractiveOverlay() const { return topInteractiveOverlay() != nullptr; }

    // Host feeds translated platform events here.
    void handleEvent(const InputEvent& e) {
        if (!root_) return;
        Overlay* ov = topInteractiveOverlay();
        Widget* ir = ov ? static_cast<Widget*>(ov) : root_.get();
        switch (e.type) {
            case InputEvent::MouseMove:
                // While a widget holds the press, route moves to it as a drag
                // (selection); otherwise it's just hover tracking.
                if (pressed_)
                    pressed_->onMouseDrag(e.x, e.y);
                else
                    updateHover(ir->hitTest(e.x, e.y));
                break;
            case InputEvent::MouseDown: {
                // A click outside the active overlay's content dismisses it and is
                // swallowed (doesn't reach widgets beneath).
                if (ov && !ov->contentRect().contains(e.x, e.y)) {
                    if (ov->dismissOnOutsideClick) ov->requestClose();
                    break;
                }
                Widget* hit = ir->hitTest(e.x, e.y);
                pressed_ = hit;
                setFocus(hit && hit->focusable ? hit : nullptr);
                if (hit) {
                    hit->pressed = true;
                    hit->onMouseDown(e.x, e.y, e.button, e.shift);
                }
                break;
            }
            case InputEvent::MouseUp:
                if (pressed_) {
                    pressed_->pressed = false;
                    pressed_->onMouseUp(e.x, e.y, e.button);
                    if (ir->hitTest(e.x, e.y) == pressed_) pressed_->onClick();
                    pressed_ = nullptr;
                }
                break;
            case InputEvent::KeyDown:
                if (ov && e.key == Key::Escape && ov->dismissOnEscape) {
                    ov->requestClose();
                } else if (e.key == Key::Tab) {
                    moveFocus(e.shift ? -1 : 1);
                } else if (focused_) {
                    bool consumed = focused_->onKey(e.key, e.shift, e.ctrl, e.alt);
                    if (!consumed && (e.key == Key::Enter || e.key == Key::Space)) focused_->onClick();
                }
                break;
            case InputEvent::Text:
                if (focused_ && e.text) focused_->onText(e.text);
                break;
            case InputEvent::TextEditing:
                if (focused_) focused_->onTextEditing(e.text ? e.text : "");
                break;
            case InputEvent::Wheel:
                break;
        }
    }
    // Walk the accessibility tree (role + label per non-None widget). The hook is
    // present so a platform a11y backend can be added later without rework.
    void visitAccessible(const std::function<void(Widget*, Role, const std::string&)>& fn) const {
        if (root_) visitA(root_.get(), fn);
    }

    void frame(Renderer& r, float dt, float mouseX, float mouseY) {
        if (!root_) return;
        trans_ = std::min(1.0f, trans_ + dt * 4.0f);
        displayed_ = trans_ >= 1.0f ? to_ : lerp(from_, to_, easeOutCubic(trans_));

        int w = 0, h = 0;
        r.outputSize(w, h);
        Rect full{0, 0, (float)w, (float)h};
        root_->arrange(r, full);
        for (auto& o : overlays_) o->arrange(r, full); // overlays fill the window

        Widget* ir = inputRoot();
        if (pressed_)
            pressed_->onMouseDrag(mouseX, mouseY); // sustain drag-select without per-move events
        else
            updateHover(ir->hitTest(mouseX, mouseY));

        root_->update(dt);
        for (auto& o : overlays_) o->update(dt);

        root_->draw(r, displayed_);
        for (auto& o : overlays_) o->draw(r, displayed_); // drawn on top, in push order
        pruneOverlays();

        // Drive platform text input from focus: caret rects are valid only after
        // draw (they depend on shaped-text measurement), so push the IME area here.
        if (textInput_) {
            bool active = focused_ && focused_->wantsTextInput();
            if (active)
                textInput_(true, focused_->caretRect());
            else if (textActive_)
                textInput_(false, Rect{});
            textActive_ = active;
        }
    }

private:
    void updateHover(Widget* hv) {
        if (hv == hovered_) return;
        if (hovered_) hovered_->hovered = false;
        hovered_ = hv;
        if (hovered_) hovered_->hovered = true;
    }
    Overlay* topInteractiveOverlay() const {
        for (auto it = overlays_.rbegin(); it != overlays_.rend(); ++it)
            if ((*it)->interactive && !(*it)->closed()) return it->get();
        return nullptr;
    }
    Widget* inputRoot() const {
        Overlay* o = topInteractiveOverlay();
        return o ? static_cast<Widget*>(o) : root_.get();
    }
    static bool subtreeContains(Widget* root, Widget* w) {
        if (!root || !w) return false;
        if (root == w) return true;
        for (auto& c : root->children())
            if (subtreeContains(c.get(), w)) return true;
        return false;
    }
    void pruneOverlays() {
        for (auto it = overlays_.begin(); it != overlays_.end();) {
            if ((*it)->closed()) {
                // Drop focus/press/hover if they pointed into the removed overlay.
                if (subtreeContains(it->get(), focused_)) setFocus(nullptr);
                if (subtreeContains(it->get(), pressed_)) pressed_ = nullptr;
                if (subtreeContains(it->get(), hovered_)) hovered_ = nullptr;
                it = overlays_.erase(it);
            } else {
                ++it;
            }
        }
    }
    void moveFocus(int dir) {
        std::vector<Widget*> f;
        inputRoot()->collectFocusable(f);
        if (f.empty()) return;
        int n = (int)f.size(), idx = -1;
        for (int i = 0; i < n; ++i)
            if (f[i] == focused_) {
                idx = i;
                break;
            }
        int next = (idx < 0) ? (dir > 0 ? 0 : n - 1) : (((idx + dir) % n) + n) % n;
        setFocus(f[next]);
    }
    static void visitA(Widget* w, const std::function<void(Widget*, Role, const std::string&)>& fn) {
        if (!w->visible) return;
        Role role = w->accessibleRole();
        if (role != Role::None) fn(w, role, w->accessibleLabel());
        for (auto& c : w->children()) visitA(c.get(), fn);
    }

    std::unique_ptr<Widget> root_;
    std::vector<std::unique_ptr<Overlay>> overlays_;
    Widget* hovered_ = nullptr;
    Widget* pressed_ = nullptr;
    Widget* focused_ = nullptr;
    Theme from_, to_, displayed_;
    float trans_ = 1.0f;

    std::function<void(bool, const Rect&)> textInput_;
    bool textActive_ = false;
};

} // namespace spry
