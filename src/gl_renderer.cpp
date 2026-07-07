#include "spry/gl_renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

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

// ---- modern GL function pointers (1.1 is linked directly) ------------------
#define SPRY_GL_FUNCS(X)                                                                                \
    X(PFNGLCREATESHADERPROC, glCreateShader)                                                            \
    X(PFNGLSHADERSOURCEPROC, glShaderSource)                                                            \
    X(PFNGLCOMPILESHADERPROC, glCompileShader)                                                          \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv)                                                              \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                                                    \
    X(PFNGLDELETESHADERPROC, glDeleteShader)                                                            \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                                                          \
    X(PFNGLATTACHSHADERPROC, glAttachShader)                                                            \
    X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation)                                                \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram)                                                              \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                                                            \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                                                  \
    X(PFNGLUSEPROGRAMPROC, glUseProgram)                                                                \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)                                                \
    X(PFNGLUNIFORM2FPROC, glUniform2f)                                                                  \
    X(PFNGLUNIFORM1FPROC, glUniform1f)                                                                  \
    X(PFNGLUNIFORM1IPROC, glUniform1i)                                                                  \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                                                      \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                                                      \
    X(PFNGLGENBUFFERSPROC, glGenBuffers)                                                                \
    X(PFNGLBINDBUFFERPROC, glBindBuffer)                                                                \
    X(PFNGLBUFFERDATAPROC, glBufferData)                                                                \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)                                              \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)                                      \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture)                                                          \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                                                      \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                                                      \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)                                            \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)

#define SPRY_GL_DECL(type, name) static type name = nullptr;
SPRY_GL_FUNCS(SPRY_GL_DECL)
#undef SPRY_GL_DECL

static bool loadGlFns() {
#define SPRY_GL_LOAD(type, name)                                                                        \
    name = (type)SDL_GL_GetProcAddress(#name);                                                          \
    if (!name) return false;
    SPRY_GL_FUNCS(SPRY_GL_LOAD)
#undef SPRY_GL_LOAD
    return true;
}

// Shader bodies are GLSL 130/150 compatible (in/out, texture(), custom frag out).
// The #version line is chosen at runtime from the context: 130 for GL 3.0 on
// Win/Linux, 150 for GL 3.2+ core on macOS (which rejects <150 in core profile).
// Attribute locations are bound explicitly since layout(location=) is a 330 feature.
static const char* kVert = R"(
in vec2 aPos;
in vec4 aColor;
in vec2 aUV;
uniform vec2 uViewport;
uniform float uScale; // logical units -> device px (HiDPI content scale)
out vec4 vColor; out vec2 vUV;
void main(){
    vColor = aColor; vUV = aUV;
    float x = (aPos.x * uScale) / uViewport.x * 2.0 - 1.0;
    float y = 1.0 - (aPos.y * uScale) / uViewport.y * 2.0; // screen y-down -> NDC y-up
    gl_Position = vec4(x, y, 0.0, 1.0);
})";

static const char* kFrag = R"(
in vec4 vColor; in vec2 vUV;
uniform sampler2D uTex;
uniform int uImage; // 0 = coverage (glyphs/fills, .r); 1 = full RGBA image, vColor is the tint
out vec4 FragColor;
void main(){
    if (uImage == 1) {
        FragColor = texture(uTex, vUV) * vColor; // straight-alpha RGBA modulated by the tint
    } else {
        float a = texture(uTex, vUV).r; // coverage; white 1x1 tex => 1 for solid fills
        FragColor = vec4(vColor.rgb, vColor.a * a);
    }
})";

static GLuint compile(GLenum type, const char* version, const char* body) {
    GLuint s = glCreateShader(type);
    const char* srcs[2] = {version, body};
    glShaderSource(s, 2, srcs, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        SDL_Log("spry GL shader compile failed: %s", log);
    }
    return s;
}

struct GlGlyph {
    GLuint tex = 0;
    int w = 0, h = 0, bx = 0, by = 0;
};
struct Shaped {
    uint32_t gid = 0;
    float xadv = 0, xoff = 0, yoff = 0;
};

struct GlRenderer::Impl {
    int vpW = 1, vpH = 1;
    GLuint prog = 0, vao = 0, vbo = 0, ebo = 0, white = 0;
    GLuint fbo = 0, fboTex = 0;
    int fboW = 0, fboH = 0;
    GLint uViewport = -1, uTex = -1, uScale = -1, uImage = -1;
    float contentScale = 1.0f;  // logical -> device px (HiDPI)
    std::vector<GLuint> images; // uploaded image textures, freed at teardown

    FT_Library lib = nullptr;
    FT_Face face = nullptr;
    hb_font_t* hb = nullptr;
    bool fontReady = false;
    int curPx = 0;
    std::unordered_map<uint64_t, GlGlyph> glyphs;

    void initGL() {
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        // macOS core profile requires GLSL >= 150; GL 3.0 (Win/Linux) caps at 130.
        const char* glsl =
            ((major > 3) || (major == 3 && minor >= 2)) ? "#version 150\n" : "#version 130\n";
        GLuint vs = compile(GL_VERTEX_SHADER, glsl, kVert);
        GLuint fs = compile(GL_FRAGMENT_SHADER, glsl, kFrag);
        prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glBindAttribLocation(prog, 0, "aPos");
        glBindAttribLocation(prog, 1, "aColor");
        glBindAttribLocation(prog, 2, "aUV");
        glLinkProgram(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        uViewport = glGetUniformLocation(prog, "uViewport");
        uTex = glGetUniformLocation(prog, "uTex");
        uScale = glGetUniformLocation(prog, "uScale");
        uImage = glGetUniformLocation(prog, "uImage");

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        const GLsizei stride = 8 * sizeof(float);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glGenTextures(1, &white);
        glBindTexture(GL_TEXTURE_2D, white);
        unsigned char w = 255;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &w);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void resizeFbo(int w, int h) {
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        if (fbo && w == fboW && h == fboH) return;
        fboW = w;
        fboH = h;
        if (!fbo) glGenFramebuffers(1, &fbo);
        if (!fboTex) glGenTextures(1, &fboTex);
        glBindTexture(GL_TEXTURE_2D, fboTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void draw(const std::vector<float>& verts, const std::vector<int>& idx, GLuint tex) {
        if (verts.empty() || idx.empty()) return;
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(int), idx.data(), GL_STREAM_DRAW);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, nullptr);
    }

    bool loadFont(const char* path) {
        if (FT_Init_FreeType(&lib)) return false;
        if (FT_New_Face(lib, path, 0, &face)) {
            FT_Done_FreeType(lib);
            lib = nullptr;
            return false;
        }
        FT_Set_Pixel_Sizes(face, 0, 16);
        curPx = 16;
        hb = hb_ft_font_create_referenced(face);
        fontReady = true;
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
    GlGlyph& glyph(uint32_t gid, int px) {
        uint64_t key = ((uint64_t)px << 24) | gid;
        if (auto it = glyphs.find(key); it != glyphs.end()) return it->second;
        GlGlyph g{};
        if (FT_Load_Glyph(face, gid, FT_LOAD_RENDER) == 0) {
            FT_GlyphSlot sl = face->glyph;
            int w = (int)sl->bitmap.width, h = (int)sl->bitmap.rows;
            g.w = w;
            g.h = h;
            g.bx = sl->bitmap_left;
            g.by = sl->bitmap_top;
            if (w > 0 && h > 0) {
                glGenTextures(1, &g.tex);
                glBindTexture(GL_TEXTURE_2D, g.tex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE,
                             sl->bitmap.buffer);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
        }
        return glyphs.emplace(key, g).first->second;
    }
};

GlRenderer::GlRenderer() : d_(new Impl()) {
    loadGlFns();
    d_->initGL();
}
GlRenderer::~GlRenderer() {
    for (auto& [k, g] : d_->glyphs)
        if (g.tex) glDeleteTextures(1, &g.tex);
    for (GLuint t : d_->images)
        if (t) glDeleteTextures(1, &t);
    if (d_->hb) hb_font_destroy(d_->hb);
    if (d_->face) FT_Done_Face(d_->face);
    if (d_->lib) FT_Done_FreeType(d_->lib);
    delete d_;
}

bool GlRenderer::loadFont(const char* path) { return d_->loadFont(path); }
void GlRenderer::setSize(int w, int h) {
    d_->vpW = w > 0 ? w : 1;
    d_->vpH = h > 0 ? h : 1;
    d_->resizeFbo(d_->vpW, d_->vpH);
}
void GlRenderer::setContentScale(float s) { d_->contentScale = s > 0.0f ? s : 1.0f; }
void GlRenderer::outputSize(int& w, int& h) {
    // Report logical size: widgets lay out in logical units and the shader scales
    // them up to the device-pixel FBO. Round (not truncate) to avoid a 1px drift.
    w = (int)std::lround((float)d_->vpW / d_->contentScale);
    h = (int)std::lround((float)d_->vpH / d_->contentScale);
}
unsigned int GlRenderer::targetTexture() const { return d_->fboTex; }

void GlRenderer::blitToDefault(int dstW, int dstH) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, d_->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, d_->fboW, d_->fboH, 0, 0, dstW, dstH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlRenderer::beginFrame(Color clear) {
    glBindFramebuffer(GL_FRAMEBUFFER, d_->fbo); // render into our FBO, not the host's screen
    glViewport(0, 0, d_->vpW, d_->vpH);
    glClearColor(clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, clear.a / 255.0f);
    glDisable(GL_SCISSOR_TEST); // clear the whole FBO, unclipped
    glClear(GL_COLOR_BUFFER_BIT);
    resetClip();
    resetOpacity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(d_->prog);
    glUniform2f(d_->uViewport, (float)d_->vpW, (float)d_->vpH);
    glUniform1f(d_->uScale, d_->contentScale);
    glUniform1i(d_->uTex, 0);
    glUniform1i(d_->uImage, 0); // default to the coverage path; drawImage flips it per-draw
}
void GlRenderer::endFrame() {
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlRenderer::applyClip(const Rect* r) {
    if (!r) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    glEnable(GL_SCISSOR_TEST);
    // Clip rects are in logical units; scale to device px for the scissor box.
    float cs = d_->contentScale;
    int x = (int)std::lround(r->x * cs), y = (int)std::lround(r->y * cs);
    int w = (int)std::lround(r->w * cs), h = (int)std::lround(r->h * cs);
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    // Our coordinate space is y-down from the top; GL scissor is y-up from the
    // bottom of the FBO, so flip the origin.
    glScissor(x, d_->vpH - (y + h), w, h);
}

static void pushVert(std::vector<float>& v, float x, float y, Color c, float u, float w) {
    v.insert(v.end(), {x, y, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f, u, w});
}

void GlRenderer::fillMesh(const std::vector<Vertex>& vs, const std::vector<int>& idx) {
    if (vs.size() < 3) return;
    std::vector<float> v;
    v.reserve(vs.size() * 8);
    for (const auto& p : vs) pushVert(v, p.x, p.y, tint(p.color), 0.0f, 0.0f);
    d_->draw(v, idx, d_->white);
}

void GlRenderer::fillRoundedRect(float cx, float cy, float w, float h, float radius, Color top, Color bot) {
    Mesh m = roundedBlob(cx, cy, w, h, radius, 0.0f, 0.0f, top, bot);
    fillMesh(m.verts, m.indices);
}

void GlRenderer::fillRect(float x, float y, float w, float h, Color c) {
    c = tint(c);
    std::vector<float> v;
    pushVert(v, x, y, c, 0, 0);
    pushVert(v, x + w, y, c, 0, 0);
    pushVert(v, x + w, y + h, c, 0, 0);
    pushVert(v, x, y + h, c, 0, 0);
    std::vector<int> idx{0, 1, 2, 0, 2, 3};
    d_->draw(v, idx, d_->white);
}

void GlRenderer::text(float x, float y, float scale, Color c, const char* s) {
    if (!d_->fontReady) return;
    c = tint(c);
    // Rasterize at device px for crisp HiDPI text, but lay out in logical units
    // (glyph metrics / advances are at the device size, so divide by the scale).
    float cs = d_->contentScale;
    float inv = 1.0f / cs;
    int px = (int)std::lround(kTextBasePx * scale * cs);
    if (px < 6) px = 6;
    std::vector<Shaped> glyphs;
    d_->shape(s, px, glyphs);
    float ascent = (float)(d_->face->size->metrics.ascender >> 6) * inv;
    float pen = x, baseline = y + ascent;
    for (const auto& sg : glyphs) {
        GlGlyph& g = d_->glyph(sg.gid, px);
        if (g.tex) {
            float gx = pen + (sg.xoff + (float)g.bx) * inv;
            float gy = baseline - (sg.yoff + (float)g.by) * inv;
            float gw = (float)g.w * inv, gh = (float)g.h * inv;
            std::vector<float> v;
            v.reserve(32);
            pushVert(v, gx, gy, c, 0.0f, 0.0f);
            pushVert(v, gx + gw, gy, c, 1.0f, 0.0f);
            pushVert(v, gx + gw, gy + gh, c, 1.0f, 1.0f);
            pushVert(v, gx, gy + gh, c, 0.0f, 1.0f);
            std::vector<int> idx{0, 1, 2, 0, 2, 3};
            d_->draw(v, idx, g.tex);
        }
        pen += sg.xadv * inv;
    }
}

ImageHandle GlRenderer::loadImage(const unsigned char* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    d_->images.push_back(tex);
    return (ImageHandle)tex; // the GL texture id doubles as the opaque handle
}

void GlRenderer::drawImage(ImageHandle img, const Rect& dst, Color mod) {
    if (!img) return;
    Color c = tint(mod); // fold in the active opacity, same as every other primitive
    std::vector<float> v;
    pushVert(v, dst.x, dst.y, c, 0.0f, 0.0f);
    pushVert(v, dst.x + dst.w, dst.y, c, 1.0f, 0.0f);
    pushVert(v, dst.x + dst.w, dst.y + dst.h, c, 1.0f, 1.0f);
    pushVert(v, dst.x, dst.y + dst.h, c, 0.0f, 1.0f);
    std::vector<int> idx{0, 1, 2, 0, 2, 3};
    glUniform1i(d_->uImage, 1); // sample full RGBA for this draw...
    d_->draw(v, idx, (GLuint)img);
    glUniform1i(d_->uImage, 0); // ...then restore the coverage path for fills/text
}

Size GlRenderer::measureText(float scale, const char* s) {
    float lineH = kTextBasePx * scale + 7.0f;
    if (!d_->fontReady) return Size{(float)std::strlen(s) * 0.60f * kTextBasePx * scale, lineH};
    float cs = d_->contentScale;
    int px = (int)std::lround(kTextBasePx * scale * cs);
    if (px < 6) px = 6;
    std::vector<Shaped> glyphs;
    d_->shape(s, px, glyphs);
    float w = 0;
    for (const auto& g : glyphs) w += g.xadv;
    return Size{w / cs, lineH}; // device advances -> logical width
}

} // namespace spry
