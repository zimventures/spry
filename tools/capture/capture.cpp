// Headless screenshot capture for the Spry docs (#11).
//
// Renders a fixed set of scenes × themes to PNG through an OFFSCREEN SDL software
// renderer — no window or display is needed — so `spry_capture` regenerates every
// site screenshot in one run (see docs/assets/README.md). It links only the public
// Spry API + SDL3, exactly like a consumer would.
//
// Usage:  spry_capture [output_dir]      (default: ./docs/assets)
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <spry/spry.h>
#include <spry/sdl_renderer.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../../examples/web/scenes.h" // the WASM demo scenes — captured as fallbacks

using namespace spry;

static bool loadAnyFont(SdlRenderer& ren, bool mono = false) {
    // The WASM demos embed JetBrainsMono-Regular.ttf and load it for every scene,
    // so their fallback stills (docs/assets/wasm/scene-*.png) must render in the
    // same font to match the live browser demos exactly — including the programming
    // ligatures the text scene showcases. For a mono job JetBrainsMono is therefore
    // the *only* acceptable font: if it can't be loaded, fail rather than silently
    // falling back to a host font (which would defeat the flag). Non-WASM stills use
    // the host UI font.
    if (mono) {
        if (ren.loadFont("examples/web/JetBrainsMono-Regular.ttf"))
            return true;
        std::fprintf(stderr, "  loadAnyFont: mono job but examples/web/JetBrainsMono-Regular.ttf "
                             "could not be loaded (run from the repo root)\n");
        return false;
    }
    const char* fonts[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };
    for (const char* f : fonts)
        if (ren.loadFont(f)) return true;
    return false;
}

// ---------------------------------------------------------------------------
// A scene populates a Context (root tree + optional overlays). The capture driver
// sets the theme and runs a few frames so springs/overlays settle. The only scenes
// now are the WASM demo fallback stills, built from examples/web/scenes.h below.
// ---------------------------------------------------------------------------
using SceneFn = std::function<void(Context&)>;

// ---------------------------------------------------------------------------
// Capture driver.
// ---------------------------------------------------------------------------
static bool capture(const std::string& path, int w, int h, const Theme& theme, const SceneFn& scene,
                    bool mono = false) {
    SDL_Surface* surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        std::fprintf(stderr, "  SDL_CreateSurface: %s\n", SDL_GetError());
        return false;
    }
    SDL_Renderer* sr = SDL_CreateSoftwareRenderer(surf);
    if (!sr) {
        std::fprintf(stderr, "  SDL_CreateSoftwareRenderer: %s\n", SDL_GetError());
        SDL_DestroySurface(surf);
        return false;
    }
    {
        SdlRenderer ren(sr);
        if (!loadAnyFont(ren, mono)) {
            std::printf("  %-40s %s\n", path.c_str(), "FAILED (no font)");
            SDL_DestroyRenderer(sr);
            SDL_DestroySurface(surf);
            return false;
        }
        Context ctx;
        scene(ctx);
        ctx.setThemeImmediate(theme);
        // Run enough frames for springs / overlay open animations to settle.
        const float dt = 1.0f / 60.0f;
        for (int i = 0; i < 40; ++i) {
            ren.beginFrame(ctx.displayedTheme().color(tokens::Background));
            ctx.frame(ren, dt, -1.0f, -1.0f); // off-screen mouse: no hover highlight
            ren.endFrame();
        }
    }
    bool ok = SDL_LockSurface(surf);
    if (ok) {
        ok = stbi_write_png(path.c_str(), w, h, 4, surf->pixels, surf->pitch) != 0;
        SDL_UnlockSurface(surf);
    }
    std::printf("  %-40s %s\n", path.c_str(), ok ? "ok" : "FAILED");
    SDL_DestroyRenderer(sr);
    SDL_DestroySurface(surf);
    return ok;
}

int main(int argc, char** argv) {
    SDL_SetMainReady();
    // Initialize video where it's available (some platforms require it before
    // creating surfaces/renderers). On a truly headless box the init may fail —
    // the offscreen software renderer can still work, so warn and continue.
    if (!SDL_Init(SDL_INIT_VIDEO))
        std::fprintf(stderr, "SDL_Init(VIDEO) failed (%s); continuing headless\n", SDL_GetError());

    std::string dir = argc > 1 ? argv[1] : "docs/assets";
    if (!dir.empty() && dir.back() != '/') dir += '/';

    const Theme dark = Theme::builtinDark();

    struct Job {
        const char* name;
        int w, h;
        const Theme* theme;
        SceneFn scene;
        bool mono = false; // render in JetBrainsMono (the font the WASM demos embed)
    };
    std::vector<Job> jobs = {
        // WASM demo fallback stills (#40): rendered from the same scenes.h builders
        // the live demos use, in the same JetBrainsMono font (mono=true), so the
        // no-JS/no-WASM fallback matches the live view — ligatures and all.
        {"wasm/scene-hello", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildHello()); }, true},
        {"wasm/scene-theming", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildTheming()); }, true},
        {"wasm/scene-controls", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildControls()); }, true},
        {"wasm/scene-layout", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildLayout()); }, true},
        {"wasm/scene-animation", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildAnimation()); }, true},
        {"wasm/scene-text", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildText()); }, true},
        {"wasm/scene-data", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildData()); }, true},
        // The overlays scene spawns its overlays on click; the live still would
        // otherwise show only the trigger buttons. Open a representative context
        // menu (via the same shared builder the button uses) so the fallback still
        // depicts the scene's signature interaction rather than a bare toolbar.
        {"wasm/scene-overlays", 900, 560, &dark,
         [](Context& c) {
             c.setRoot(demos::buildOverlays());
             c.addOverlay(demos::buildDemoMenu(150.0f, 150.0f));
         },
         true},
        {"wasm/scene-textinput", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildTextInput()); }, true},
        {"wasm/scene-gallery", 900, 560, &dark,
         [](Context& c) { c.setRoot(demos::buildGallery()); }, true},
    };

    int failures = 0;
    std::printf("Capturing %zu screenshots into %s\n", jobs.size(), dir.c_str());
    SDL_CreateDirectory((dir + "wasm").c_str()); // some jobs write into a wasm/ subdir
    for (const auto& j : jobs)
        if (!capture(dir + j.name + ".png", j.w, j.h, *j.theme, j.scene, j.mono)) ++failures;

    std::printf("Done: %zu ok, %d failed\n", jobs.size() - failures, failures);
    SDL_Quit();
    return failures ? 1 : 0;
}
