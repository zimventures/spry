#pragma once
#include <SDL3/SDL.h>

#include <vector>

#include "renderer.h"

// The SDL_Renderer backend (#208). Construct it with an SDL_Renderer* the host
// already owns. Real text uses FreeType (#212) via a private glyph-texture cache;
// FreeType is kept out of this header (pimpl) so consumers don't pull it in.
namespace spry {

class SdlRenderer : public Renderer {
public:
    explicit SdlRenderer(SDL_Renderer* r);
    ~SdlRenderer() override;

    // Load a TTF for real text. Returns false if it can't be loaded; text() then
    // falls back to SDL's debug font.
    bool loadFont(const char* path);

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
    SDL_Renderer* r_;
    struct Text; // FreeType font + glyph-texture cache (pimpl)
    Text* text_ = nullptr;
    std::vector<SDL_Texture*> images_; // uploaded image textures, freed in the dtor
};

} // namespace spry
