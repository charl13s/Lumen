// LampAnim.cpp - the visitor's lamp sweeping across a sculpture. Owner: Timothy.
//
// Seen from the side. The lamp's direction is a vector turned by a rotation
// matrix; the edges of its cone are that vector turned a little further either
// way. Where the cone meets the floor gives the pool of light. The room's own
// light fades up and back down between keyframes.

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Anim.h"
#include "PageKit.h"
#include "Transform.h"

namespace lumen::demo {

void animLamp(Canvas& c, const Plan* /*plan*/, float t) {
    const float floorY = 340.0f;
    const xf::PointF lamp{150.0f, 175.0f};

    // The room light, set at keyframes: dark, up, held, down.
    const anim::Track roomLight({{0.0f, 0.0f}, {1.5f, 0.0f}, {2.5f, 1.0f}, {4.5f, 1.0f}, {5.5f, 0.0f}, {6.0f, 0.0f}});
    const float glow = roomLight.at(std::fmod(t, 6.0f));
    c.clear(anim::lerp(Colour{14, 12, 10, 255}, Colour{70, 62, 50, 255}, glow));
    raster::fillPolygonScanline(c, {{0, 340}, {c.width(), 340}, {c.width(), c.height()}, {0, c.height()}},
                                anim::lerp(Colour{24, 21, 17, 255}, Colour{96, 86, 68, 255}, glow));

    // The lamp swings: its direction is (1, 0) turned by the current angle.
    const float angle = 0.62f + 0.32f * std::sin(2.0f * kPi * t / 6.0f);
    const float spread = 0.2f;
    auto hitFloor = [&](float a) {
        const xf::PointF d = xf::rotation(a).apply(xf::PointF{1.0f, 0.0f});
        const float s = (floorY - lamp.y) / d.y;
        return xf::PointF{lamp.x + d.x * s, floorY};
    };
    const xf::PointF centre = hitFloor(angle), nearEdge = hitFloor(angle + spread), farEdge = hitFloor(angle - spread);

    // How much light reaches the plinth: most when the beam is centred on it.
    const float plinthX = 400.0f;
    const float lit = anim::clamp01(1.0f - std::fabs(centre.x - plinthX) / 130.0f);
    const Colour stone = anim::lerp(Colour{80, 76, 70, 255}, Colour{232, 226, 214, 255}, std::max(lit, glow * 0.6f));

    // The pool of light on the floor, behind everything else.
    const int poolX = static_cast<int>(std::lround((nearEdge.x + farEdge.x) * 0.5f));
    const int poolR = static_cast<int>(std::lround(std::fabs(farEdge.x - nearEdge.x) * 0.5f));
    raster::ellipseFilled(c, poolX, static_cast<int>(floorY), poolR, 10, Colour{150, 128, 84, 255});
    raster::ellipseMidpoint(c, poolX, static_cast<int>(floorY), poolR, 10, kGold);

    // The plinth and the bust on it, and the shadow they throw along the floor.
    if (lit > 0.2f) {
        const float sx = lamp.x + (plinthX + 50.0f - lamp.x) * (floorY - lamp.y) / (265.0f - lamp.y);
        raster::lineThick(c, static_cast<int>(plinthX + 50), static_cast<int>(floorY),
                          static_cast<int>(std::min(sx, static_cast<float>(c.width() - 1))),
                          static_cast<int>(floorY), 5, Colour{8, 7, 6, 255});
    }
    raster::fillPolygonScanline(c, {{350, 265}, {450, 265}, {450, 340}, {350, 340}}, stone);
    raster::fillPolygonScanline(c, {{368, 200}, {432, 200}, {432, 265}, {368, 265}}, stone);
    raster::ellipseFilled(c, 400, 200, 32, 7, stone);
    raster::ellipseMidpoint(c, 400, 200, 32, 7, Colour{40, 36, 30, 255});

    // The cone, from the lamp to the floor.
    raster::lineBresenham(c, static_cast<int>(lamp.x), static_cast<int>(lamp.y),
                          static_cast<int>(nearEdge.x), static_cast<int>(floorY), kGoldDim);
    raster::lineBresenham(c, static_cast<int>(lamp.x), static_cast<int>(lamp.y),
                          static_cast<int>(farEdge.x), static_cast<int>(floorY), kGoldDim);

    // The visitor holding the lamp.
    raster::circleMidpoint(c, 95, 150, 14, kInk);                 // head
    raster::lineThick(c, 95, 164, 95, 260, 3, kInk);              // body
    raster::lineThick(c, 95, 260, 75, 338, 3, kInk);              // legs
    raster::lineThick(c, 95, 260, 115, 338, 3, kInk);
    raster::lineThick(c, 95, 190, static_cast<int>(lamp.x), static_cast<int>(lamp.y), 3, kInk);   // arm
    raster::circleFilled(c, static_cast<int>(lamp.x), static_cast<int>(lamp.y), 7, kGold);

    font::drawText(c, 20, 14, "THE LAMP", kGold, 2);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "LAMP ANGLE %3.0f DEGREES   ROOM LIGHT %3.0f%%", angle * 180.0f / kPi, glow * 100.0f);
    font::drawText(c, 20, 40, buf, kDim, 1);
    font::drawText(c, 20, c.height() - 22, "BEAM: ROTATION MATRIX.  ROOM LIGHT: EASED KEYFRAMES.", kDim, 1);
}

} // namespace lumen::demo
