#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "input.h"
#include "layout.h"
#include "renderer.h"
#include "theme.h"

// Retained widget tree (#209). Widgets are persistent objects with a computed
// `rect`, layout hints, and lifecycle hooks. Containers arrange children; leaves
// override paint()/measure(). The base Widget is a simple stack container.
namespace spry {

// Accessibility role (#216) — exposed via the a11y tree; no platform backend yet.
enum class Role { None, Group, Label, Button, Checkbox, TextField, Panel };

class Widget {
public:
    virtual ~Widget() = default;

    // Layout hints (set by the caller).
    float prefW = -1.0f, prefH = -1.0f; // -1 => auto / content-sized
    float grow = 0.0f;                  // flex weight along the parent's main axis
    Edges margin;
    bool visible = true;

    // Computed by layout each frame.
    Rect rect{};

    // Interaction state (#216). hovered/pressed/focused are set by Context as it
    // routes input; focusable opts a widget into click-focus + Tab navigation.
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool focusable = false;

    // Tree.
    Widget* add(std::unique_ptr<Widget> child);
    template <class T, class... Args>
    T* emplace(Args&&... args) {
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        add(std::move(owned));
        return raw;
    }
    const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }
    Widget* parent() const { return parent_; }

    // Layout: measure desired content size, then arrange into a final rect. The
    // renderer is threaded through so widgets can measure shaped text (#212).
    virtual Size measure(Renderer& r, float availW, float availH);
    virtual void arrange(Renderer& r, Rect rect);

    // Per-frame.
    virtual void update(float dt);
    virtual void draw(Renderer& r, const Theme& theme);
    virtual void paint(Renderer&, const Theme&) {} // self-paint hook for leaves

    Widget* hitTest(float x, float y);
    void collectFocusable(std::vector<Widget*>& out); // tree-order, for Tab nav

    // Input hooks (#216). Return true to consume the event.
    virtual bool onMouseDown(float /*x*/, float /*y*/, int /*button*/, bool /*shift*/) { return false; }
    virtual bool onMouseUp(float /*x*/, float /*y*/, int /*button*/) { return false; }
    // Mouse moved while this widget holds the press capture (drag select, #213).
    virtual void onMouseDrag(float /*x*/, float /*y*/) {}
    virtual void onClick() {}
    virtual bool onKey(Key /*key*/, bool /*shift*/, bool /*ctrl*/, bool /*alt*/) { return false; }
    virtual void onText(const char* /*utf8*/) {}
    // IME pre-edit (composition) text, not yet committed (#213).
    virtual void onTextEditing(const char* /*utf8*/) {}
    virtual void onFocusChanged(bool /*focused*/) {}

    // Text-input / IME hooks (#213). A widget that edits text returns true so the
    // host can start platform text input and place the IME candidate window at the
    // caret; caretRect() reports that caret in Spry coordinates.
    virtual bool wantsTextInput() const { return false; }
    virtual Rect caretRect() const { return rect; }

    // Accessibility hooks (#216): expose roles + labels for an a11y tree.
    virtual Role accessibleRole() const { return Role::None; }
    virtual std::string accessibleLabel() const { return {}; }

protected:
    Widget* parent_ = nullptr;
    std::vector<std::unique_ptr<Widget>> children_;
};

} // namespace spry
