#pragma once
#include <algorithm>
#include <vector>

#include "color.h"
#include "layout.h"

// Backend-agnostic 2D renderer (#208). The SDL_Renderer backend ships now; a
// GL/SDL_gpu backend can implement the same interface later for shader-grade
// effects, without the widgets knowing the difference.
namespace spry {

struct Vertex {
    float x = 0, y = 0;
    Color color;
};

struct Mesh {
    std::vector<Vertex> verts;
    std::vector<int> indices;
};

// Text metrics (#212): a monospace cell model so layout can size text without a
// renderer. text() advances by textCellW(); Label::measure() uses the same, so
// rendering and layout agree. `scale` is a multiple of the base pixel size.
constexpr float kTextBasePx = 13.0f;
inline float textCellW(float scale) { return 0.60f * kTextBasePx * scale; }
inline float textLineH(float scale) { return kTextBasePx * scale + 7.0f; }

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void beginFrame(Color clear) = 0;
    virtual void endFrame() = 0;
    virtual void outputSize(int& w, int& h) = 0;

    virtual void fillMesh(const std::vector<Vertex>& verts, const std::vector<int>& indices) = 0;
    virtual void fillRoundedRect(float cx, float cy, float w, float h, float radius, Color top,
                                 Color bot) = 0;
    virtual void fillRect(float x, float y, float w, float h, Color c) = 0;
    virtual void text(float x, float y, float scale, Color c, const char* s) = 0;
    // Shaped text extent (#212) — lets layout size proportional text. Width is the
    // sum of shaped advances; height is the line box.
    virtual Size measureText(float scale, const char* s) = 0;

    // Clip stack (#213 — the seam noted in #208). Widgets that scroll content
    // (e.g. a text field) push a clip rect, draw, then pop. Pushes intersect with
    // the current clip; backends apply the top of the stack as a scissor. The
    // base maintains the stack so backends only implement applyClip().
    void pushClip(const Rect& r) {
        Rect c = clipStack_.empty() ? r : intersectRect(clipStack_.back(), r);
        clipStack_.push_back(c);
        applyClip(&clipStack_.back());
    }
    void popClip() {
        if (clipStack_.empty()) return;
        clipStack_.pop_back();
        applyClip(clipStack_.empty() ? nullptr : &clipStack_.back());
    }
    // Backends call this at frame start so a stale scissor never survives a frame.
    void resetClip() {
        clipStack_.clear();
        applyClip(nullptr);
    }

    // Opacity stack (#214). Overlays push a multiplier to fade a whole subtree in
    // or out; backends multiply opacity() into every emitted colour's alpha.
    // Pushes compound (a 0.5 inside a 0.5 yields 0.25).
    void pushOpacity(float o) { opacityStack_.push_back(opacity() * o); }
    void popOpacity() {
        if (!opacityStack_.empty()) opacityStack_.pop_back();
    }
    float opacity() const { return opacityStack_.empty() ? 1.0f : opacityStack_.back(); }
    void resetOpacity() { opacityStack_.clear(); }

protected:
    // Apply the active clip (nullptr => disable). Default no-op suits headless
    // renderers; SDL/GL backends set a scissor.
    virtual void applyClip(const Rect* /*r*/) {}

    // Fold the active opacity into a colour's alpha (backends call this on every
    // colour they emit).
    Color tint(Color c) const {
        float o = opacity();
        o = o < 0.0f ? 0.0f : (o > 1.0f ? 1.0f : o); // guard the uint8_t cast
        c.a = (uint8_t)(c.a * o);
        return c;
    }

    static Rect intersectRect(const Rect& a, const Rect& b) {
        float x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
        float x1 = std::min(a.x + a.w, b.x + b.w), y1 = std::min(a.y + a.h, b.y + b.h);
        return Rect{x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
    }

    std::vector<Rect> clipStack_;
    std::vector<float> opacityStack_;
};

// Shared geometry usable by any backend or widget: a rounded rect as a triangle
// fan, with optional gooey edge distortion (amp/phase). amp <= 0 → clean panel.
Mesh roundedBlob(float cx, float cy, float w, float h, float radius, float distortAmp, float phase,
                 Color top, Color bot, int seg = 8);

} // namespace spry
