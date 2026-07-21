#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

#include "color.h"
#include "layout.h"

/// @file renderer.h
/// The backend-agnostic 2D `Renderer` interface (#208) widgets draw through, plus
/// its supporting value types. Concrete backends (`SdlRenderer`, `GlRenderer`)
/// implement it; the widgets never know the difference.

namespace spry {

/// @addtogroup renderer
/// @{

/// A single 2D vertex: position + color (for `Renderer::fillMesh`).
struct Vertex {
    float x = 0, y = 0;  ///< Position, in logical pixels.
    Color color;         ///< Vertex color.
};

/// A triangle mesh: a vertex list plus triangle indices.
struct Mesh {
    std::vector<Vertex> verts;   ///< Vertices.
    std::vector<int> indices;    ///< Triangle indices into @ref verts.
};

/// An opaque handle to a GPU-resident image, returned by `Renderer::loadImage`
/// and passed to `Renderer::drawImage`. Wide enough to hold either a GL texture id
/// or a backend pointer; 0 means "none / upload failed". Each backend interprets
/// the bits its own way.
using ImageHandle = std::uintptr_t;

/// Base text pixel size; `scale` arguments are a multiple of this. @ingroup renderer
constexpr float kTextBasePx = 13.0f;
/// Monospace cell width at `scale`, so layout can size text without a renderer.
inline float textCellW(float scale) { return 0.60f * kTextBasePx * scale; }
/// Line height (line box) at `scale`.
inline float textLineH(float scale) { return kTextBasePx * scale + 7.0f; }

/// The abstract 2D drawing interface the widgets emit to. A consumer normally
/// **picks** a shipped backend (`SdlRenderer` / `GlRenderer`) rather than
/// implementing this. The base class provides the clip and opacity stacks; a
/// backend implements the pure-virtual primitives (and optionally @ref applyClip).
class Renderer {
public:
    virtual ~Renderer() = default;

    /// Begin a frame, clearing to `clear`.
    virtual void beginFrame(Color clear) = 0;
    /// End the frame and present.
    virtual void endFrame() = 0;
    /// Report the current drawable size in pixels via `w`/`h`.
    virtual void outputSize(int& w, int& h) = 0;

    /// Fill a triangle mesh (vertices + indices).
    virtual void fillMesh(const std::vector<Vertex>& verts, const std::vector<int>& indices) = 0;
    /// Fill a rounded rectangle centered at (`cx`,`cy`) with a vertical `top`→`bot` gradient.
    virtual void fillRoundedRect(float cx, float cy, float w, float h, float radius, Color top,
                                 Color bot) = 0;
    /// Fill an axis-aligned rectangle with a solid color.
    virtual void fillRect(float x, float y, float w, float h, Color c) = 0;
    /// Draw UTF-8 text `s` at (`x`,`y`) baseline, at `scale`, in color `c`.
    virtual void text(float x, float y, float scale, Color c, const char* s) = 0;
    /// Measure shaped text extent (#212) so layout can size proportional text.
    /// Width is the sum of shaped advances; height is the line box.
    virtual Size measureText(float scale, const char* s) = 0;

    /// Upload tightly-packed RGBA8 pixels (`w*h*4` bytes, straight alpha) to a GPU
    /// texture and return an opaque handle; draw it later with @ref drawImage.
    /// Backends own the texture and free it at teardown (no per-image free — hold
    /// and reuse the handle). Returns 0 on bad input or an unsupported (e.g.
    /// headless) backend, in which case @ref drawImage is a no-op.
    virtual ImageHandle loadImage(const unsigned char* /*rgba*/, int /*w*/, int /*h*/) { return 0; }
    /// Draw a previously-loaded image into `dst`. `mod` modulates it (white =
    /// as-is); the active @ref opacity folds into its alpha.
    virtual void drawImage(ImageHandle /*img*/, const Rect& /*dst*/, Color /*mod*/) {}

    /// Push a clip rectangle (intersected with the current clip). Widgets that
    /// scroll content push a clip, draw, then @ref popClip. Backends apply the top
    /// of the stack as a scissor.
    void pushClip(const Rect& r) {
        Rect c = clipStack_.empty() ? r : intersectRect(clipStack_.back(), r);
        clipStack_.push_back(c);
        applyClip(&clipStack_.back());
    }
    /// Pop the top clip rectangle.
    void popClip() {
        if (clipStack_.empty()) return;
        clipStack_.pop_back();
        applyClip(clipStack_.empty() ? nullptr : &clipStack_.back());
    }
    /// Clear the clip stack — backends call this at frame start so a stale scissor
    /// never survives a frame.
    void resetClip() {
        clipStack_.clear();
        applyClip(nullptr);
    }

    /// Push an opacity multiplier (compounds: 0.5 inside 0.5 → 0.25). Overlays use
    /// this to fade a whole subtree; backends multiply @ref opacity into alpha.
    void pushOpacity(float o) { opacityStack_.push_back(opacity() * o); }
    /// Pop the top opacity multiplier.
    void popOpacity() {
        if (!opacityStack_.empty()) opacityStack_.pop_back();
    }
    /// The current compounded opacity (1.0 when the stack is empty).
    float opacity() const { return opacityStack_.empty() ? 1.0f : opacityStack_.back(); }
    /// Clear the opacity stack.
    void resetOpacity() { opacityStack_.clear(); }

protected:
    /// Apply the active clip (`nullptr` ⇒ disable). Default no-op suits headless
    /// renderers; the SDL/GL backends set a scissor.
    virtual void applyClip(const Rect* /*r*/) {}

    /// Fold the active @ref opacity into a color's alpha — backends call this on
    /// every color they emit.
    Color tint(Color c) const {
        float o = opacity();
        o = o < 0.0f ? 0.0f : (o > 1.0f ? 1.0f : o); // guard the uint8_t cast
        c.a = (uint8_t)(c.a * o);
        return c;
    }

    /// Intersection of two rectangles (empty if they don't overlap).
    static Rect intersectRect(const Rect& a, const Rect& b) {
        float x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
        float x1 = std::min(a.x + a.w, b.x + b.w), y1 = std::min(a.y + a.h, b.y + b.h);
        return Rect{x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
    }

    std::vector<Rect> clipStack_;       ///< Active clip stack (top = current scissor).
    std::vector<float> opacityStack_;   ///< Active opacity stack (top = current opacity).
};

/// Shared geometry usable by any backend or widget: a rounded rect as a triangle
/// fan, with optional gooey edge distortion (`distortAmp`/`phase`; `amp <= 0` → a
/// clean panel). Filled with a vertical `top`→`bot` gradient.
Mesh roundedBlob(float cx, float cy, float w, float h, float radius, float distortAmp, float phase,
                 Color top, Color bot, int seg = 8);

/// @}

} // namespace spry
