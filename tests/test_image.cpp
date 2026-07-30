// Headless tests for the Image widget's upload caching and the freeImage() release
// path. A stub renderer stands in for a GPU backend: it hands out fake handles and
// tracks which are still live, so we can assert on upload/free bookkeeping without
// a real texture.
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "spry/spry.h"

using namespace spry;

namespace {

struct ImageStubRenderer : Renderer {
    int uploads = 0;              // loadImage() calls that were handed real pixels
    int draws = 0;                // drawImage() calls with a non-zero handle
    Rect lastDst{};               // destination rect of the most recent draw
    std::set<ImageHandle> live;   // handles uploaded and not yet freed
    bool failUploads = false;     // simulate an unsupported/headless backend

    void beginFrame(Color) override {}
    void endFrame() override {}
    void outputSize(int& w, int& h) override {
        w = 800;
        h = 600;
    }
    void fillMesh(const std::vector<Vertex>&, const std::vector<int>&) override {}
    void fillRoundedRect(float, float, float, float, float, Color, Color) override {}
    void fillRect(float, float, float, float, Color) override {}
    void text(float, float, float, Color, const char*) override {}
    Size measureText(float scale, const char* s) override {
        return Size{(float)std::strlen(s) * 7.0f * scale, 13.0f * scale + 7.0f};
    }

    ImageHandle loadImage(const unsigned char* rgba, int w, int h) override {
        if (!rgba || w <= 0 || h <= 0) return 0;
        ++uploads;
        if (failUploads) return 0;
        ImageHandle h2 = (ImageHandle)(uploads + 1000); // any non-zero, unique id
        live.insert(h2);
        return h2;
    }
    void drawImage(ImageHandle img, const Rect& dst, Color) override {
        if (!img) return;
        ++draws;
        lastDst = dst;
    }
    void freeImage(ImageHandle img) override { live.erase(img); }
};

// A 2x2 RGBA block — the widget only forwards the pointer, so the values are moot.
const std::vector<unsigned char> kPixels(2 * 2 * 4, 0xFF);

// Widget owns its children by unique_ptr, so Image isn't copyable/movable — fill
// one in place rather than returning it by value.
void initImage(Image& img, ImageHandle* handle, float w = 64.0f, float h = 32.0f) {
    img.pixels = kPixels.data();
    img.srcW = 2;
    img.srcH = 2;
    img.drawW = w;
    img.drawH = h;
    img.handle = handle;
}

} // namespace

TEST_CASE("Image measures to its requested draw size") {
    ImageStubRenderer r;
    ImageHandle handle = 0;
    Image img;
    initImage(img, &handle, 120.0f, 90.0f);
    Size s = img.measure(r, 10000.0f, 10000.0f);
    REQUIRE(s.w == 120.0f);
    REQUIRE(s.h == 90.0f);
}

TEST_CASE("Image uploads once and reuses the cached handle across rebuilds") {
    ImageStubRenderer r;
    Theme th = Theme::builtinDark();
    ImageHandle handle = 0;

    {
        Image img;
        initImage(img, &handle);
        img.rect = Rect{0, 0, 64, 32};
        img.paint(r, th);
        img.paint(r, th); // same instance, repainted
    }
    REQUIRE(r.uploads == 1);
    REQUIRE(handle != 0);
    REQUIRE(r.draws == 2);

    // A fresh widget instance (a scene rebuild) must reuse the cached handle.
    Image rebuilt;
    initImage(rebuilt, &handle);
    rebuilt.rect = Rect{0, 0, 64, 32};
    rebuilt.paint(r, th);
    REQUIRE(r.uploads == 1);
    REQUIRE(r.draws == 3);
}

TEST_CASE("Image draws into its laid-out rect") {
    ImageStubRenderer r;
    Theme th = Theme::builtinDark();
    ImageHandle handle = 0;
    Image img;
    initImage(img, &handle);
    img.rect = Rect{12, 34, 200, 100};
    img.paint(r, th);
    REQUIRE(r.lastDst.x == 12.0f);
    REQUIRE(r.lastDst.y == 34.0f);
    REQUIRE(r.lastDst.w == 200.0f);
    REQUIRE(r.lastDst.h == 100.0f);
}

TEST_CASE("Image does not retry an upload that failed") {
    ImageStubRenderer r;
    r.failUploads = true;
    Theme th = Theme::builtinDark();
    ImageHandle handle = 0;
    Image img;
    initImage(img, &handle);
    img.rect = Rect{0, 0, 64, 32};
    for (int i = 0; i < 5; ++i) img.paint(r, th);
    REQUIRE(r.uploads == 1); // one attempt, then the tried_ guard holds
    REQUIRE(handle == 0);
    REQUIRE(r.draws == 0); // nothing to draw
}

TEST_CASE("freeImage releases the texture; clearing the handle re-uploads") {
    ImageStubRenderer r;
    Theme th = Theme::builtinDark();
    ImageHandle handle = 0;

    Image img;
    initImage(img, &handle);
    img.rect = Rect{0, 0, 64, 32};
    img.paint(r, th);
    REQUIRE(r.live.size() == 1);

    // The owner of the handle releases it and resets the cache slot — the pattern
    // for images with a bounded lifetime (a preview pane closing, say).
    r.freeImage(handle);
    handle = 0;
    REQUIRE(r.live.empty());

    Image next;
    initImage(next, &handle);
    next.rect = Rect{0, 0, 64, 32};
    next.paint(r, th);
    REQUIRE(r.uploads == 2); // the cleared slot forces a fresh upload
    REQUIRE(r.live.size() == 1);
}

TEST_CASE("Image with no pixels or no handle slot never uploads") {
    ImageStubRenderer r;
    Theme th = Theme::builtinDark();

    ImageHandle handle = 0;
    Image noPixels;
    noPixels.srcW = 2;
    noPixels.srcH = 2;
    noPixels.handle = &handle;
    noPixels.paint(r, th);

    Image noSlot;
    initImage(noSlot, nullptr);
    noSlot.paint(r, th);

    REQUIRE(r.uploads == 0);
    REQUIRE(r.draws == 0);
}

TEST_CASE("Renderer base class ignores image calls") {
    // The default Renderer has no image support: uploads fail and draw/free are
    // no-ops, so a scene with images still runs on a backend that lacks them.
    struct Bare : Renderer {
        void beginFrame(Color) override {}
        void endFrame() override {}
        void outputSize(int& w, int& h) override { w = h = 0; }
        void fillMesh(const std::vector<Vertex>&, const std::vector<int>&) override {}
        void fillRoundedRect(float, float, float, float, float, Color, Color) override {}
        void fillRect(float, float, float, float, Color) override {}
        void text(float, float, float, Color, const char*) override {}
        Size measureText(float, const char*) override { return Size{0, 0}; }
    } bare;

    REQUIRE(bare.loadImage(kPixels.data(), 2, 2) == 0);
    bare.drawImage(0, Rect{0, 0, 1, 1}, Color{255, 255, 255, 255}); // no crash
    bare.freeImage(0);                                              // no crash
}
