#include "spry/renderer.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace spry {

static void appendArc(std::vector<Vertex>& out, float ccx, float ccy, float rad, float start, int seg,
                      float yTop, float yBot, Color cTop, Color cBot) {
    for (int i = 0; i <= seg; ++i) {
        float a = start + (float)i / seg * (float)(M_PI / 2.0);
        Vertex v;
        v.x = ccx + std::cos(a) * rad;
        v.y = ccy + std::sin(a) * rad;
        float tt = std::clamp((v.y - yTop) / std::max(1.0f, yBot - yTop), 0.0f, 1.0f);
        v.color = lerp(cTop, cBot, tt);
        out.push_back(v);
    }
}

Mesh roundedBlob(float cx, float cy, float w, float h, float radius, float amp, float phase, Color top,
                 Color bot, int seg) {
    float hw = w * 0.5f, hh = h * 0.5f;
    radius = std::clamp(radius, 0.0f, std::min(hw, hh));
    float l = cx - hw, r = cx + hw, t = cy - hh, b = cy + hh;
    float yTop = cy - hh, yBot = cy + hh;

    Mesh m;
    appendArc(m.verts, l + radius, t + radius, radius, (float)M_PI, seg, yTop, yBot, top, bot);
    appendArc(m.verts, r - radius, t + radius, radius, (float)(M_PI * 1.5), seg, yTop, yBot, top, bot);
    appendArc(m.verts, r - radius, b - radius, radius, 0.0f, seg, yTop, yBot, top, bot);
    appendArc(m.verts, l + radius, b - radius, radius, (float)(M_PI * 0.5), seg, yTop, yBot, top, bot);

    if (amp > 0.01f) {
        for (auto& v : m.verts) {
            float th = std::atan2(v.y - cy, v.x - cx);
            float wob = amp * (std::sin(3.0f * th + phase) + 0.5f * std::sin(5.0f * th - phase * 1.3f));
            v.x += std::cos(th) * wob;
            v.y += std::sin(th) * wob;
        }
    }

    // center vertex (anchor, undistorted) + triangle-fan indices
    Vertex c;
    c.x = cx;
    c.y = cy;
    c.color = lerp(top, bot, 0.5f);
    m.verts.insert(m.verts.begin(), c);
    int n = (int)m.verts.size() - 1;
    for (int i = 0; i < n; ++i) {
        m.indices.push_back(0);
        m.indices.push_back(1 + i);
        m.indices.push_back(1 + (i + 1) % n);
    }
    return m;
}

} // namespace spry
