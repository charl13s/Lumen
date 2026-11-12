// TransformPage.cpp - 2D transformations, as a page and as an animation.
// Owner: Priscillah.
//
// The shape is a small framed painting: the frame, the canvas and a mountain
// painted on it, each a polygon. Every transformation is one 3x3 matrix applied
// to all of their vertices.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Anim.h"
#include "PageKit.h"
#include "Transform.h"

namespace lumen::demo {
namespace {

using xf::Mat3;
using xf::PointF;
using xf::Polygon;

// The painting, centred on the origin so rotating and scaling it is simple.
struct Shape {
    Polygon frame{{-50, -38}, {50, -38}, {50, 38}, {-50, 38}};
    Polygon canvas{{-40, -28}, {40, -28}, {40, 28}, {-40, 28}};
    Polygon mountain{{-34, 22}, {-8, -14}, {6, 4}, {16, -8}, {36, 22}};
};

const Colour kFrameGold{214, 176, 88, 255};
const Colour kCanvasDark{40, 36, 48, 255};

void drawShape(Canvas& c, const Shape& s, const Mat3& m, bool ghost) {
    const Polygon f = xf::transform(s.frame, m), cv = xf::transform(s.canvas, m),
                  mt = xf::transform(s.mountain, m);
    if (ghost) {
        const Colour g{90, 80, 62, 255};
        raster::polyline(c, poly::toPixels(f), 1, g, true);
        raster::polyline(c, poly::toPixels(cv), 1, g, true);
        return;
    }
    raster::fillPolygonScanline(c, poly::toPixels(f), kFrameGold);
    raster::fillPolygonScanline(c, poly::toPixels(cv), kCanvasDark);
    raster::fillPolygonScanline(c, poly::toPixels(mt), kTeal);
    raster::polyline(c, poly::toPixels(f), 1, kGoldDim, true);
    // A dot on the top left corner shows which way round the shape is.
    const PointF corner = m.apply(s.frame[0]);
    raster::circleFilled(c, static_cast<int>(std::lround(corner.x)), static_cast<int>(std::lround(corner.y)), 3, kCoral);
}

void drawMatrix(Canvas& c, int x, int y, const Mat3& m) {
    char row[64];
    auto tidy = [](float v) { return std::fabs(v) < 0.005f ? 0.0f : v; };   // no "-0.00"
    for (int i = 0; i < 3; ++i) {
        std::snprintf(row, sizeof(row), "| %6.2f %6.2f %7.1f |", tidy(m.m[i][0]), tidy(m.m[i][1]), tidy(m.m[i][2]));
        font::drawText(c, x, y + i * 12, row, kDim, 1);
    }
}

} // namespace

// ---- the page ----------------------------------------------------------------

void pageTransforms(Canvas& c, float /*time*/) {
    const Shape s;
    struct Example {
        const char* title;
        const char* note;
        Mat3 local;   // applied about the panel's centre
    };
    const float pi = kPi;
    const Example ex[] = {
        {"TRANSLATE", "MOVE BY (90, -40)", xf::translation(90, -40)},
        {"ROTATE ABOUT A POINT", "40 DEGREES ABOUT THE CORAL CORNER",
         xf::rotationAbout(40.0f * pi / 180.0f, PointF{-50, -38})},
        {"SCALE ABOUT A POINT", "1.6 WIDE, 1.3 TALL, ABOUT THE CENTRE", xf::scaling(1.6f, 1.3f)},
        {"REFLECT IN A LINE", "MIRRORED IN THE SLOPING LINE", xf::reflectionAbout(PointF{0, 0}, -0.35f)},
        {"SHEAR", "X MOVES BY 0.6 TIMES Y", xf::shearing(0.6f, 0.0f)},
        {"COMPOSITE", "ROTATE, SCALE, THEN MOVE: ONE MATRIX",
         xf::translation(60, 10) * xf::rotation(-0.5f) * xf::scaling(0.8f, 0.8f)},
    };

    for (int i = 0; i < 6; ++i) {
        const int col = i % 3, row = i / 3;
        const int px = 30 + col * 410, py = 60 + row * 280;
        panel(c, px, py, 390, 265, ex[i].title);
        const Mat3 centre = xf::translation(static_cast<float>(px + 160), static_cast<float>(py + 112));
        drawShape(c, s, centre, true);                      // where it started
        drawShape(c, s, centre * ex[i].local, false);       // where the matrix sends it
        if (i == 3) {
            // Draw the mirror line.
            const PointF a = (centre * xf::rotation(-0.35f)).apply(PointF{-150, 0});
            const PointF b = (centre * xf::rotation(-0.35f)).apply(PointF{150, 0});
            float x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
            if (raster::clipCohenSutherland(x0, y0, x1, y1, static_cast<float>(px + 2), static_cast<float>(py + 20),
                                            static_cast<float>(px + 388), static_cast<float>(py + 262)))
                raster::lineDDA(c, static_cast<int>(x0), static_cast<int>(y0), static_cast<int>(x1),
                                static_cast<int>(y1), kCoral);
        }
        caption(c, px + 12, py + 212, ex[i].note);
        drawMatrix(c, px + 12, py + 228, ex[i].local);
    }
}

// ---- the animation -----------------------------------------------------------

// Six steps, each eased: move in, rotate, scale, reflect, shear, and then the
// inverse of the whole composite matrix takes the painting back to the start.
void animTransforms(Canvas& c, const Plan* /*plan*/, float t) {
    c.clear(kBg);
    const Shape s;
    const float step = 8.0f / 6.0f;
    const int stage = std::min(5, static_cast<int>(t / step));
    const float k = anim::easeInOut((t - stage * step) / step);

    // Each value at the end of stages 0 to 4; during a stage it eases from the
    // previous stage's value to this one.
    const float moveX[] = {1, 1, 1, 1, 1}, turn[] = {0, 1, 1, 1, 1}, grow[] = {0, 0, 1, 1, 1},
                flip[] = {0, 0, 0, 1, 1}, lean[] = {0, 0, 0, 0, 1};
    auto value = [&](const float* ends) {
        if (stage == 5) return 1.0f - k;   // undo everything together
        const float before = stage == 0 ? 0.0f : ends[stage - 1];
        return anim::lerp(before, ends[stage], k);
    };

    const float mx = value(moveX), rt = value(turn), sc = value(grow), fl = value(flip), sh = value(lean);

    const Mat3 m = xf::translation(170.0f + 300.0f * mx, 200.0f) *
                   xf::shearing(0.6f * sh, 0.0f) *
                   xf::rotation(rt * kPi * 0.5f) *
                   xf::scaling((1.0f + 0.6f * sc) * (1.0f - 2.0f * fl), 1.0f + 0.6f * sc);

    drawShape(c, s, xf::translation(170, 200), true);
    drawShape(c, s, m, false);

    const char* names[] = {"TRANSLATE", "ROTATE 90 DEGREES", "SCALE BY 1.6", "REFLECT (SCALE X BY -1)",
                           "SHEAR X BY 0.6 Y", "THE INVERSE MATRIX UNDOES IT ALL"};
    font::drawText(c, 20, 18, "2D TRANSFORMATIONS", kGold, 2);
    font::drawText(c, 20, 44, names[stage], kInk, 1);
    font::drawText(c, 20, 330, "THE WHOLE CHAIN IS ONE 3X3 MATRIX:", kDim, 1);
    drawMatrix(c, 20, 346, m);
}

} // namespace lumen::demo
