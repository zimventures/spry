#include "spry/theme.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace spry;

TEST_CASE("token constants match their string names", "[theme][tokens]") {
    CHECK(std::string(tokens::Background) == "background");
    CHECK(std::string(tokens::Surface) == "surface");
    CHECK(std::string(tokens::SurfaceAlt) == "surfaceAlt");
    CHECK(std::string(tokens::Accent) == "accent");
    CHECK(std::string(tokens::AccentText) == "accentText");
    CHECK(std::string(tokens::Text) == "text");
    CHECK(std::string(tokens::TextDim) == "textDim");
    CHECK(std::string(tokens::Scrim) == "scrim");
    CHECK(std::string(tokens::Radius) == "radius");
}

TEST_CASE("builtinDark defines the full core token vocabulary", "[theme][tokens]") {
    CHECK(Theme::builtinDark().missingCoreTokens().empty());
}

TEST_CASE("missingCoreTokens reports every core token for an empty theme", "[theme][tokens]") {
    Theme empty;
    auto missing = empty.missingCoreTokens();
    // 8 core colors + 1 core metric.
    CHECK(missing.size() == kCoreColorTokens.size() + kCoreMetricTokens.size());
    auto has = [&](const char* t) { return std::find(missing.begin(), missing.end(), t) != missing.end(); };
    CHECK(has(tokens::Accent));
    CHECK(has(tokens::Scrim));
    CHECK(has(tokens::Radius));
}

TEST_CASE("missingCoreTokens only flags the absent tokens", "[theme][tokens]") {
    Theme t;
    t.colors[tokens::Accent] = Color{1, 2, 3};
    auto missing = t.missingCoreTokens();
    auto has = [&](const char* tok) { return std::find(missing.begin(), missing.end(), tok) != missing.end(); };
    CHECK_FALSE(has(tokens::Accent)); // defined → not missing
    CHECK(has(tokens::Surface));      // still absent
}

TEST_CASE("color()/metric() read tokens with fallback", "[theme][tokens]") {
    Theme t = Theme::builtinDark();
    CHECK(t.color(tokens::Accent).b > 0);              // defined in builtinDark
    CHECK(t.color("no-such-token", Color{9, 9, 9}).r == 9); // unknown → fallback
    CHECK(t.metric(tokens::Radius, 0.0f) > 0.0f);
}
