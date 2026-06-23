#pragma once
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
};

// Shared geometry usable by any backend or widget: a rounded rect as a triangle
// fan, with optional gooey edge distortion (amp/phase). amp <= 0 → clean panel.
Mesh roundedBlob(float cx, float cy, float w, float h, float radius, float distortAmp, float phase,
                 Color top, Color bot, int seg = 8);

} // namespace spry
