#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "input.h"
#include "layout.h"
#include "renderer.h"
#include "theme.h"

/// @file widget.h
/// The retained widget tree (#209): the `Widget` base plus the `Role`/`Cursor`
/// enums. Widgets are persistent objects with a computed `rect`, layout hints, and
/// lifecycle hooks. Containers arrange children; leaves override
/// `paint()`/`measure()`.

namespace spry {

/// @addtogroup widgets
/// @{

/// Accessibility role (#216) — exposed via the a11y tree.
/// @note Experimental: the tree is walked but there is no platform a11y backend yet.
enum class Role { None, Group, Label, Button, Checkbox, Radio, TextField, Panel };

/// Desired mouse cursor while a widget is the hover/press target (#222). The host
/// maps these to platform cursors; widgets that don't override keep the arrow.
enum class Cursor { Default, ResizeH, ResizeV };

/// The retained base class for everything in the tree. Compose trees with
/// @ref emplace / @ref add; drive them via `Context`. Custom widgets override the
/// lifecycle (`measure`/`arrange`/`update`/`draw`/`paint`) and input hooks.
class Widget {
public:
    virtual ~Widget() = default;

    float prefW = -1.0f;   ///< Preferred width  (-1 ⇒ auto / content-sized).
    float prefH = -1.0f;   ///< Preferred height (-1 ⇒ auto / content-sized).
    float grow = 0.0f;     ///< Flex weight along the parent's main axis.
    Edges margin;          ///< Outer margin around this widget.
    bool visible = true;   ///< When false, the widget is skipped by layout and draw.
    std::string tooltip;   ///< Hover tooltip text (empty = none); shown by `Context` (#214).

    Rect rect{};           ///< Final rectangle, computed by layout each frame.

    bool hovered = false;    ///< Set by `Context`: mouse is over this widget.
    bool pressed = false;    ///< Set by `Context`: this widget holds the press capture.
    bool focused = false;    ///< Set by `Context`: this widget has keyboard focus.
    bool focusable = false;  ///< Opt in to click-focus + Tab navigation.

    /// Adopt `child` as the last child; returns the raw pointer.
    Widget* add(std::unique_ptr<Widget> child);
    /// Construct a `T` child in place (forwarding `args`) and return it.
    template <class T, class... Args>
    T* emplace(Args&&... args) {
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        add(std::move(owned));
        return raw;
    }
    /// This widget's children (owning, in tree order).
    const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }
    /// This widget's parent, or `nullptr` for the root.
    Widget* parent() const { return parent_; }
    /// Remove all children (e.g. to repopulate a dynamic list).
    /// @warning Callers that route input must first clear any focus/press/hover
    /// pointing into the removed subtree — see `Context::dropInteractionWithin()`.
    void clearChildren() { children_.clear(); }

    /// Measure desired content size given the available space. The renderer is
    /// threaded through so widgets can measure shaped text (#212).
    virtual Size measure(Renderer& r, float availW, float availH);
    /// Place this widget (and its children) into the final `rect`.
    virtual void arrange(Renderer& r, Rect rect);

    /// Advance per-frame animation/state by `dt` seconds.
    virtual void update(float dt);
    /// Draw this widget and its subtree.
    virtual void draw(Renderer& r, const Theme& theme);
    /// Self-paint hook for leaf widgets (default no-op).
    virtual void paint(Renderer&, const Theme&) {}

    /// Deepest visible widget at (`x`,`y`), or `nullptr`.
    Widget* hitTest(float x, float y);
    /// Append focusable descendants in tree order (for Tab navigation).
    void collectFocusable(std::vector<Widget*>& out);

    /// @name Input hooks (#216)
    /// Override to react to input; return `true` to consume the event.
    /// @{
    virtual bool onMouseDown(float /*x*/, float /*y*/, int /*button*/, bool /*shift*/, bool /*ctrl*/) {
        return false;
    }
    virtual bool onMouseUp(float /*x*/, float /*y*/, int /*button*/) { return false; }
    /// Mouse moved while this widget holds the press capture (drag select, #213).
    virtual void onMouseDrag(float /*x*/, float /*y*/) {}
    /// Fired when a press and release land on this same widget.
    virtual void onClick() {}
    /// Scroll wheel over this widget; `dy` is in notches (+ = up). Return `true` to
    /// consume — `Context` bubbles up the parent chain until something does (#215).
    virtual bool onWheel(float /*dx*/, float /*dy*/) { return false; }
    virtual bool onKey(Key /*key*/, bool /*shift*/, bool /*ctrl*/, bool /*alt*/) { return false; }
    virtual void onText(const char* /*utf8*/) {}
    /// IME pre-edit (composition) text, not yet committed (#213).
    virtual void onTextEditing(const char* /*utf8*/) {}
    virtual void onFocusChanged(bool /*focused*/) {}
    /// @}

    /// Mouse cursor to show while this widget is the hover/press target (#222).
    virtual Cursor cursor() const { return Cursor::Default; }

    /// Return `true` if this widget edits text, so the host starts platform text
    /// input and places the IME candidate window at @ref caretRect.
    virtual bool wantsTextInput() const { return false; }
    /// The caret rectangle, in Spry coordinates (for IME placement).
    virtual Rect caretRect() const { return rect; }

    /// Accessibility role for the a11y tree (#216). @note Experimental.
    virtual Role accessibleRole() const { return Role::None; }
    /// Accessibility label for the a11y tree (#216). @note Experimental.
    virtual std::string accessibleLabel() const { return {}; }

protected:
    Widget* parent_ = nullptr;                       ///< Owning parent (`nullptr` at the root).
    std::vector<std::unique_ptr<Widget>> children_;  ///< Owned children, in tree order.
};

/// @}

} // namespace spry
