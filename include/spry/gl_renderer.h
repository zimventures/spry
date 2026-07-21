#pragma once
#include "renderer.h"

/// @file gl_renderer.h
/// The `GlRenderer` backend — embed Spry in a host that owns a GL context.

namespace spry {

/// @addtogroup renderer
/// @{

/// OpenGL 3.3 backend (#208; the path to #217): renders Spry into a host-owned GL
/// context, for embedding in an app that already owns the default framebuffer.
/// Renders to its own FBO and composites. `gl_renderer.h` is pimpl-clean (no SDL).
/// @note A current GL context must exist before construction, and the host must
/// provide the drawable size via @ref setSize (the backend doesn't own the window).
class GlRenderer : public Renderer {
public:
    GlRenderer();
    ~GlRenderer() override;

    /// Load a TTF for real text (see `SdlRenderer::loadFont`).
    bool loadFont(const char* path);
    /// Set the drawable size in pixels; call again on resize.
    void setSize(int w, int h);
    /// HiDPI content scale: widgets lay out in logical units and the renderer
    /// scales them to the device-pixel FBO, rasterizing text at device px for
    /// crispness. Default 1 (no scaling); set to the framebuffer/DPI scale.
    void setContentScale(float s);

    /// GL texture id of the rendered UI (e.g. for `ImGui::Image`).
    /// @note Cleat-shaped but generic — built for a specific host's compositing.
    unsigned int targetTexture() const;
    /// Copy the FBO to the default framebuffer (overwrite).
    /// @note Cleat-shaped but generic.
    void blitToDefault(int dstW, int dstH);
    /// Composite the FBO onto the default framebuffer with alpha blending — for
    /// presenting a transparent overlay over a host frame without an `ImGui::Image`
    /// bridge (#222). @note Cleat-shaped but generic.
    void presentBlended();

    void beginFrame(Color clear) override;
    void endFrame() override;
    void outputSize(int& w, int& h) override;
    void fillMesh(const std::vector<Vertex>& verts, const std::vector<int>& indices) override;
    void fillRoundedRect(float cx, float cy, float w, float h, float radius, Color top, Color bot) override;
    void fillRect(float x, float y, float w, float h, Color c) override;
    void text(float x, float y, float scale, Color c, const char* s) override;
    Size measureText(float scale, const char* s) override;
    ImageHandle loadImage(const unsigned char* rgba, int w, int h) override;
    void drawImage(ImageHandle img, const Rect& dst, Color mod) override;

protected:
    void applyClip(const Rect* r) override;

private:
    struct Impl;
    Impl* d_ = nullptr;
};

/// @}

} // namespace spry
