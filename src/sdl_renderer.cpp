#include "spry/sdl_renderer.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

namespace spry {

// ---- FreeType face + HarfBuzz shaper + cached glyph textures (pimpl) -------
struct Glyph {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0, bx = 0, by = 0; // size + left/top bearing
};

struct Shaped {
    uint32_t gid = 0; // glyph index (post-shaping)
    float xadv = 0, xoff = 0, yoff = 0;
};

struct SdlRenderer::Text {
    SDL_Renderer* r = nullptr;
    FT_Library lib = nullptr;
    FT_Face face = nullptr;
    hb_font_t* hb = nullptr;
    bool ready = false;
    int curPx = 0;
    std::unordered_map<uint64_t, Glyph> cache; // key = (pixelSize << 24) | glyphIndex

    ~Text() {
        for (auto& [k, g] : cache)
            if (g.tex) SDL_DestroyTexture(g.tex);
        if (hb) hb_font_destroy(hb);
        if (face) FT_Done_Face(face);
        if (lib) FT_Done_FreeType(lib);
    }

    bool load(const char* path) {
        if (FT_Init_FreeType(&lib)) return false;
        if (FT_New_Face(lib, path, 0, &face)) {
            FT_Done_FreeType(lib);
            lib = nullptr;
            return false;
        }
        FT_Set_Pixel_Sizes(face, 0, 16);
        curPx = 16;
        hb = hb_ft_font_create_referenced(face);
        ready = true;
        return true;
    }

    void setPx(int px) {
        if (px == curPx) return;
        curPx = px;
        FT_Set_Pixel_Sizes(face, 0, px);
        hb_ft_font_changed(hb);
    }

    void shape(const char* s, int px, std::vector<Shaped>& out) {
        setPx(px);
        hb_buffer_t* buf = hb_buffer_create();
        hb_buffer_add_utf8(buf, s, -1, 0, -1);
        hb_buffer_guess_segment_properties(buf);
        hb_shape(hb, buf, nullptr, 0);
        unsigned n = 0;
        hb_glyph_info_t* gi = hb_buffer_get_glyph_infos(buf, &n);
        hb_glyph_position_t* gp = hb_buffer_get_glyph_positions(buf, &n);
        out.clear();
        out.reserve(n);
        for (unsigned i = 0; i < n; ++i)
            out.push_back({gi[i].codepoint, gp[i].x_advance / 64.0f, gp[i].x_offset / 64.0f,
                           gp[i].y_offset / 64.0f});
        hb_buffer_destroy(buf);
    }

    Glyph& glyphByIndex(uint32_t gid, int px) {
        uint64_t key = ((uint64_t)px << 24) | gid;
        if (auto it = cache.find(key); it != cache.end()) return it->second;
        Glyph g{};
        if (FT_Load_Glyph(face, gid, FT_LOAD_RENDER) == 0) {
            FT_GlyphSlot s = face->glyph;
            int w = (int)s->bitmap.width, h = (int)s->bitmap.rows;
            g.bx = s->bitmap_left;
            g.by = s->bitmap_top;
            g.w = w;
            g.h = h;
            if (w > 0 && h > 0) {
                std::vector<uint8_t> rgba((size_t)w * h * 4);
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x) {
                        uint8_t a = s->bitmap.buffer[y * s->bitmap.pitch + x];
                        size_t o = ((size_t)y * w + x) * 4;
                        rgba[o] = 255;
                        rgba[o + 1] = 255;
                        rgba[o + 2] = 255;
                        rgba[o + 3] = a;
                    }
                SDL_Surface* surf = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, rgba.data(), w * 4);
                if (surf) {
                    g.tex = SDL_CreateTextureFromSurface(r, surf);
                    SDL_DestroySurface(surf);
                    if (g.tex) SDL_SetTextureBlendMode(g.tex, SDL_BLENDMODE_BLEND);
                }
            }
        }
        return cache.emplace(key, g).first->second;
    }
};

// ---- SdlRenderer ----------------------------------------------------------
SdlRenderer::SdlRenderer(SDL_Renderer* r) : r_(r) {
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    text_ = new Text();
    text_->r = r_;
}

SdlRenderer::~SdlRenderer() {
    for (SDL_Texture* t : images_)
        if (t) SDL_DestroyTexture(t);
    delete text_;
}

bool SdlRenderer::loadFont(const char* path) { return text_ && text_->load(path); }

static SDL_FColor toF(Color c) {
    return SDL_FColor{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

void SdlRenderer::beginFrame(Color clear) {
    resetClip(); // drop any scissor left from the previous frame before clearing
    resetOpacity();
    SDL_SetRenderDrawColor(r_, clear.r, clear.g, clear.b, clear.a);
    SDL_RenderClear(r_);
}

void SdlRenderer::applyClip(const Rect* r) {
    if (!r) {
        SDL_SetRenderClipRect(r_, nullptr);
        return;
    }
    SDL_Rect c{(int)std::lround(r->x), (int)std::lround(r->y), (int)std::lround(r->w),
               (int)std::lround(r->h)};
    SDL_SetRenderClipRect(r_, &c);
}

void SdlRenderer::endFrame() { SDL_RenderPresent(r_); }

void SdlRenderer::outputSize(int& w, int& h) { SDL_GetCurrentRenderOutputSize(r_, &w, &h); }

void SdlRenderer::fillMesh(const std::vector<Vertex>& vs, const std::vector<int>& idx) {
    if (vs.size() < 3 || idx.empty()) return;
    std::vector<SDL_Vertex> sv;
    sv.reserve(vs.size());
    for (const auto& v : vs) {
        SDL_Vertex s{};
        s.position = SDL_FPoint{v.x, v.y};
        s.color = toF(tint(v.color));
        sv.push_back(s);
    }
    SDL_RenderGeometry(r_, nullptr, sv.data(), (int)sv.size(), idx.data(), (int)idx.size());
}

void SdlRenderer::fillRoundedRect(float cx, float cy, float w, float h, float radius, Color top, Color bot) {
    Mesh m = roundedBlob(cx, cy, w, h, radius, 0.0f, 0.0f, top, bot);
    fillMesh(m.verts, m.indices);
}

void SdlRenderer::fillRect(float x, float y, float w, float h, Color c) {
    c = tint(c);
    SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
    SDL_FRect rr{x, y, w, h};
    SDL_RenderFillRect(r_, &rr);
}

void SdlRenderer::text(float x, float y, float scale, Color c, const char* s) {
    c = tint(c);
    if (!text_ || !text_->ready) {
        SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
        SDL_SetRenderScale(r_, scale, scale);
        SDL_RenderDebugText(r_, x / scale, y / scale, s);
        SDL_SetRenderScale(r_, 1.0f, 1.0f);
        return;
    }
    int px = (int)std::lround(kTextBasePx * scale);
    if (px < 6) px = 6;
    std::vector<Shaped> glyphs;
    text_->shape(s, px, glyphs);
    float ascent = (float)(text_->face->size->metrics.ascender >> 6);
    float pen = x, baseline = y + ascent;
    for (const auto& sg : glyphs) {
        Glyph& g = text_->glyphByIndex(sg.gid, px);
        if (g.tex) {
            SDL_SetTextureColorMod(g.tex, c.r, c.g, c.b);
            SDL_SetTextureAlphaMod(g.tex, c.a);
            SDL_FRect d{pen + sg.xoff + (float)g.bx, baseline - sg.yoff - (float)g.by, (float)g.w,
                        (float)g.h};
            SDL_RenderTexture(r_, g.tex, nullptr, &d);
        }
        pen += sg.xadv;
    }
}

ImageHandle SdlRenderer::loadImage(const unsigned char* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return 0;
    // SDL_PIXELFORMAT_RGBA32 is byte-order R,G,B,A — matching stb_image's 4-channel
    // output — regardless of endianness, so the upload is a straight copy.
    SDL_Surface* surf =
        SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, const_cast<unsigned char*>(rgba), w * 4);
    if (!surf) return 0;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r_, surf);
    SDL_DestroySurface(surf);
    if (!tex) return 0;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    images_.push_back(tex);
    return (ImageHandle)(std::uintptr_t)tex;
}

void SdlRenderer::drawImage(ImageHandle img, const Rect& dst, Color mod) {
    if (!img) return;
    SDL_Texture* tex = (SDL_Texture*)(std::uintptr_t)img;
    Color c = tint(mod); // fold in the active opacity
    SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(tex, c.a);
    SDL_FRect d{dst.x, dst.y, dst.w, dst.h};
    SDL_RenderTexture(r_, tex, nullptr, &d);
}

Size SdlRenderer::measureText(float scale, const char* s) {
    float lineH = kTextBasePx * scale + 7.0f;
    if (!text_ || !text_->ready) {
        return Size{(float)std::strlen(s) * 0.60f * kTextBasePx * scale, lineH};
    }
    int px = (int)std::lround(kTextBasePx * scale);
    if (px < 6) px = 6;
    std::vector<Shaped> glyphs;
    text_->shape(s, px, glyphs);
    float w = 0;
    for (const auto& g : glyphs) w += g.xadv;
    return Size{w, lineH};
}

} // namespace spry
