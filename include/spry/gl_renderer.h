#pragma once
#include "renderer.h"

// OpenGL 3.3 backend (#208 second backend, the path to #217). Renders Spry into
// a host-owned GL context — the integration path for Cleat (SDL3 + OpenGL +
// ImGui), where SDL_Renderer can't compose. A current GL context must exist
// before construction; the host provides the drawable size via setSize() (the
// backend doesn't own the window).
namespace spry {

class GlRenderer : public Renderer {
public:
    GlRenderer();
    ~GlRenderer() override;

    bool loadFont(const char* path);
    void setSize(int w, int h); // drawable size in pixels; call on resize
    // Content scale (HiDPI): widgets lay out in logical units and the renderer
    // scales them to the device-pixel FBO, rasterizing text at device px for
    // crispness. Default 1 (no scaling). Set to the framebuffer/DPI scale.
    void setContentScale(float s);

    // Spry renders to an offscreen FBO so it composes with a host that owns the
    // default framebuffer (e.g. Cleat's ImGui frame, which clears late).
    unsigned int targetTexture() const;          // GL texture of the rendered UI (for ImGui::Image)
    void blitToDefault(int dstW, int dstH);       // copy the FBO to the default framebuffer

    void beginFrame(Color clear) override;
    void endFrame() override;
    void outputSize(int& w, int& h) override;
    void fillMesh(const std::vector<Vertex>& verts, const std::vector<int>& indices) override;
    void fillRoundedRect(float cx, float cy, float w, float h, float radius, Color top, Color bot) override;
    void fillRect(float x, float y, float w, float h, Color c) override;
    void text(float x, float y, float scale, Color c, const char* s) override;
    Size measureText(float scale, const char* s) override;

protected:
    void applyClip(const Rect* r) override;

private:
    struct Impl;
    Impl* d_ = nullptr;
};

} // namespace spry
