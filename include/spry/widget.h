#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "layout.h"
#include "renderer.h"
#include "theme.h"

// Retained widget tree (#209). Widgets are persistent objects with a computed
// `rect`, layout hints, and lifecycle hooks. Containers arrange children; leaves
// override paint()/measure(). The base Widget is a simple stack container.
namespace spry {

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

    // Interaction state (set by Context::frame from hit-testing).
    bool hovered = false;

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

protected:
    Widget* parent_ = nullptr;
    std::vector<std::unique_ptr<Widget>> children_;
};

} // namespace spry
