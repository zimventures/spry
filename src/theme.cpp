#include "spry/theme.h"

#include <fstream>
#include <sstream>

namespace spry {

Theme Theme::builtinDark() {
    Theme t;
    t.colors = {
        {"background", {17, 18, 23}}, {"surface", {40, 43, 62}},    {"surfaceAlt", {32, 34, 48}},
        {"text", {224, 227, 238}},    {"textDim", {140, 144, 160}}, {"accent", {96, 126, 205}},
        {"accentText", {235, 238, 248}},
    };
    t.metrics = {{"radius", 12.0f}, {"pad", 24.0f}};
    return t;
}

bool Theme::loadFromFile(const std::string& path, Theme& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        std::istringstream ss(line);
        std::string kind, key;
        if (!(ss >> kind >> key)) continue;
        if (kind == "color") {
            int r, g, b, a = 255;
            if (ss >> r >> g >> b) {
                ss >> a; // optional alpha
                out.colors[key] = Color{(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
            }
        } else if (kind == "metric") {
            float v;
            if (ss >> v) out.metrics[key] = v;
        }
    }
    return true;
}

Theme lerp(const Theme& a, const Theme& b, float t) {
    Theme r = a;
    for (auto& [k, ca] : a.colors) r.colors[k] = lerp(ca, b.color(k, ca), t);
    for (auto& [k, ma] : a.metrics) r.metrics[k] = lerpf(ma, b.metric(k, ma), t);
    return r;
}

} // namespace spry
