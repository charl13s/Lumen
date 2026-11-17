// tests/test_core.cpp - tests for the algorithms, transformations and animation. Owner: Priscillah.
// Run lumen_tests before you push.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/Anim.h"
#include "core/Animations.h"
#include "core/Builders.h"
#include "core/Collision.h"
#include "core/Config.h"
#include "core/Curves.h"
#include "core/Demo.h"
#include "core/Gif.h"
#include "core/Image.h"
#include "core/Intro.h"
#include "core/Json.h"
#include "core/Lighting.h"
#include "core/Model.h"
#include "core/Plan.h"
#include "core/Polygon.h"
#include "core/PopArt.h"
#include "core/Raster.h"
#include "core/Font.h"
#include "core/Label.h"
#include "core/Sculpture.h"
#include "core/Skeleton.h"
#include "core/Transform.h"
#include "core/Transform3D.h"

namespace {

constexpr float kTestPi = 3.14159265358979f;

int g_failures = 0;
int g_checks   = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("  FAIL  %s\n", what.c_str());
    }
}

void checkNear(float got, float want, float tol, const std::string& what) {
    ++g_checks;
    if (std::fabs(got - want) > tol) {
        ++g_failures;
        std::printf("  FAIL  %s  (got %.4f, wanted %.4f)\n", what.c_str(), got, want);
    }
}

using namespace lumen;
using namespace lumen::raster;
namespace demo = lumen::demo;

const Colour kInk{255, 255, 255, 255};
const Colour kBg {0, 0, 0, 255};

int countSet(const Canvas& c, Colour col) {
    int n = 0;
    for (int y = 0; y < c.height(); ++y)
        for (int x = 0; x < c.width(); ++x)
            if (c.get(x, y) == col) ++n;
    return n;
}

// ---- Bresenham --------------------------------------------------------

void testBresenham() {
    std::printf("Bresenham\n");

    {   // a horizontal run touches exactly the pixels it spans
        Canvas c(32, 32);
        c.clear(kBg);
        lineBresenham(c, 2, 10, 20, 10, kInk);
        check(countSet(c, kInk) == 19, "horizontal line sets width+1 pixels");
        check(c.get(2, 10) == kInk && c.get(20, 10) == kInk, "endpoints are drawn");
        check(c.get(1, 10) != kInk && c.get(21, 10) != kInk, "does not overshoot");
    }
    {   // a 45 degree line is one pixel per step, no gaps and no doubling
        Canvas c(32, 32);
        c.clear(kBg);
        lineBresenham(c, 0, 0, 15, 15, kInk);
        check(countSet(c, kInk) == 16, "diagonal sets one pixel per step");
        bool onDiagonal = true;
        for (int i = 0; i <= 15; ++i)
            if (!(c.get(i, i) == kInk)) onDiagonal = false;
        check(onDiagonal, "diagonal pixels are on the diagonal");
    }
    {   // all eight octants terminate and hit both endpoints
        const int dirs[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
        bool allOk = true;
        for (auto& d : dirs) {
            Canvas c(41, 41);
            c.clear(kBg);
            const int x1 = 20 + d[0] * 17;
            const int y1 = 20 + d[1] * 11;
            lineBresenham(c, 20, 20, x1, y1, kInk);
            if (!(c.get(20, 20) == kInk) || !(c.get(x1, y1) == kInk)) allOk = false;
        }
        check(allOk, "all eight octants reach both endpoints");
    }
    {   // a steep line must not be gappy: every scanline it crosses is touched
        Canvas c(32, 64);
        c.clear(kBg);
        lineBresenham(c, 4, 2, 10, 50, kInk);
        bool everyRow = true;
        for (int y = 2; y <= 50; ++y) {
            bool rowHit = false;
            for (int x = 0; x < 32; ++x)
                if (c.get(x, y) == kInk) rowHit = true;
            if (!rowHit) everyRow = false;
        }
        check(everyRow, "steep line has no gaps");
    }
    {   // degenerate input is a single pixel, not a hang
        Canvas c(8, 8);
        c.clear(kBg);
        lineBresenham(c, 3, 3, 3, 3, kInk);
        check(countSet(c, kInk) == 1, "zero-length line draws one pixel");
    }
    {   // DDA and Bresenham should agree on an exact 45 degree case
        Canvas a(32, 32), b(32, 32);
        a.clear(kBg);
        b.clear(kBg);
        lineDDA(a, 1, 1, 20, 20, kInk);
        lineBresenham(b, 1, 1, 20, 20, kInk);
        bool same = true;
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
                if (!(a.get(x, y) == b.get(x, y))) same = false;
        check(same, "DDA and Bresenham agree on a 45 degree line");
    }
}

// ---- midpoint circle -------------------------------------------------------

void testCircle() {
    std::printf("Midpoint circle\n");

    {   // every drawn pixel is within half a pixel of the true radius
        Canvas c(64, 64);
        c.clear(kBg);
        const int cx = 32, cy = 32, r = 20;
        circleMidpoint(c, cx, cy, r, kInk);

        bool allOnRing = true;
        int  drawn = 0;
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                if (!(c.get(x, y) == kInk)) continue;
                ++drawn;
                const float d = std::sqrt(static_cast<float>((x - cx) * (x - cx) +
                                                             (y - cy) * (y - cy)));
                if (std::fabs(d - r) > 1.0f) allOnRing = false;
            }
        }
        check(drawn > 0, "circle draws something");
        check(allOnRing, "every circle pixel lies within 1px of the radius");
    }
    {   // eight way symmetry
        Canvas c(64, 64);
        c.clear(kBg);
        circleMidpoint(c, 32, 32, 15, kInk);
        bool symmetric = true;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
                if (c.get(x, y) == kInk) {
                    const int dx = x - 32, dy = y - 32;
                    if (!(c.get(32 - dx, 32 + dy) == kInk)) symmetric = false;
                    if (!(c.get(32 + dy, 32 + dx) == kInk)) symmetric = false;
                }
        check(symmetric, "circle is symmetric in all eight octants");
    }
    {   Canvas c(16, 16);
        c.clear(kBg);
        circleMidpoint(c, 8, 8, 0, kInk);
        check(countSet(c, kInk) == 1, "radius 0 draws one pixel");
    }
    {   // the filled form covers roughly pi r^2
        Canvas c(64, 64);
        c.clear(kBg);
        const int r = 18;
        circleFilled(c, 32, 32, r, kInk);
        const float area = static_cast<float>(countSet(c, kInk));
        const float want = 3.14159f * r * r;
        check(std::fabs(area - want) / want < 0.08f, "filled circle area is about pi r^2");
    }
}

// ---- midpoint ellipse ------------------------------------------------------

void testEllipse() {
    std::printf("Midpoint ellipse\n");

    {   // every pixel satisfies the ellipse equation to within a pixel
        Canvas c(128, 96);
        c.clear(kBg);
        const int cx = 64, cy = 48, rx = 50, ry = 30;
        ellipseMidpoint(c, cx, cy, rx, ry, kInk);

        int drawn = 0;
        bool onCurve = true;
        for (int y = 0; y < 96; ++y) {
            for (int x = 0; x < 128; ++x) {
                if (!(c.get(x, y) == kInk)) continue;
                ++drawn;
                const double nx = static_cast<double>(x - cx) / rx;
                const double ny = static_cast<double>(y - cy) / ry;
                const double v = nx * nx + ny * ny;
                if (v < 0.80 || v > 1.20) onCurve = false;
            }
        }
        check(drawn > 0, "ellipse draws something");
        check(onCurve, "every ellipse pixel lies on the curve");
    }
    {   // the extreme points are hit exactly
        Canvas c(128, 96);
        c.clear(kBg);
        ellipseMidpoint(c, 64, 48, 50, 30, kInk);
        check(c.get(64 + 50, 48) == kInk, "right extreme is drawn");
        check(c.get(64 - 50, 48) == kInk, "left extreme is drawn");
        check(c.get(64, 48 + 30) == kInk, "bottom extreme is drawn");
        check(c.get(64, 48 - 30) == kInk, "top extreme is drawn");
    }
    {   // no gaps, which is why there are two regions
        Canvas c(128, 96);
        c.clear(kBg);
        ellipseMidpoint(c, 64, 48, 50, 30, kInk);
        bool everyColumn = true;
        for (int x = 64 - 50; x <= 64 + 50; ++x) {
            bool hit = false;
            for (int y = 0; y < 96; ++y)
                if (c.get(x, y) == kInk) hit = true;
            if (!hit) everyColumn = false;
        }
        bool everyRow = true;
        for (int y = 48 - 30; y <= 48 + 30; ++y) {
            bool hit = false;
            for (int x = 0; x < 128; ++x)
                if (c.get(x, y) == kInk) hit = true;
            if (!hit) everyRow = false;
        }
        check(everyColumn, "ellipse has no vertical gaps");
        check(everyRow, "ellipse has no horizontal gaps (region 2 works)");
    }
    {   // four way symmetry
        Canvas c(128, 128);
        c.clear(kBg);
        ellipseMidpoint(c, 64, 64, 40, 25, kInk);
        bool symmetric = true;
        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 128; ++x)
                if (c.get(x, y) == kInk) {
                    const int dx = x - 64, dy = y - 64;
                    if (!(c.get(64 - dx, 64 + dy) == kInk)) symmetric = false;
                    if (!(c.get(64 + dx, 64 - dy) == kInk)) symmetric = false;
                }
        check(symmetric, "ellipse is symmetric in all four quadrants");
    }
    {   // equal radii degenerate to a circle
        Canvas a(64, 64), b(64, 64);
        a.clear(kBg);
        b.clear(kBg);
        ellipseMidpoint(a, 32, 32, 20, 20, kInk);
        circleMidpoint(b, 32, 32, 20, kInk);
        int differing = 0;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
                if (!(a.get(x, y) == b.get(x, y))) ++differing;
        check(differing < 20, "equal radii give approximately a circle");
    }
    {   // filled form covers about pi * rx * ry
        Canvas c(128, 96);
        c.clear(kBg);
        ellipseFilled(c, 64, 48, 40, 24, kInk);
        const float area = static_cast<float>(countSet(c, kInk));
        const float want = 3.14159f * 40.0f * 24.0f;
        check(std::fabs(area - want) / want < 0.05f,
              "filled ellipse area is about pi*rx*ry");
    }
}

// ---- curves ----------------------------------------------------------------

void testCurves() {
    std::printf("Bezier and splines\n");
    using namespace lumen::curves;

    const P2 p0{0.0f, 0.0f}, p1{50.0f, 100.0f}, p2{100.0f, 0.0f};

    {   // endpoint interpolation: a Bezier always starts at P0 and ends at Pn
        const P2 a = bezierQuadratic(p0, p1, p2, 0.0f);
        const P2 b = bezierQuadratic(p0, p1, p2, 1.0f);
        checkNear(a.x, p0.x, 1e-4f, "quadratic Bezier starts at P0");
        checkNear(b.x, p2.x, 1e-4f, "quadratic Bezier ends at P2");
        checkNear(b.y, p2.y, 1e-4f, "quadratic Bezier ends at P2 (y)");
    }
    {   // midpoint of a symmetric quadratic sits at half the control height
        const P2 m = bezierQuadratic(p0, p1, p2, 0.5f);
        checkNear(m.x, 50.0f, 1e-3f, "symmetric quadratic midpoint x");
        checkNear(m.y, 50.0f, 1e-3f, "symmetric quadratic midpoint is half the peak");
    }
    {   // de Casteljau must agree with the Bernstein form
        bool agree = true;
        for (int i = 0; i <= 20; ++i) {
            const float t = static_cast<float>(i) / 20.0f;
            const P2 a = bezierQuadratic(p0, p1, p2, t);
            const P2 b = bezierDeCasteljau({p0, p1, p2}, t);
            if (std::fabs(a.x - b.x) > 1e-3f || std::fabs(a.y - b.y) > 1e-3f)
                agree = false;
        }
        check(agree, "de Casteljau agrees with the Bernstein polynomial form");
    }
    {   // and for the cubic too
        const P2 c0{0, 0}, c1{10, 80}, c2{90, 80}, c3{100, 0};
        bool agree = true;
        for (int i = 0; i <= 20; ++i) {
            const float t = static_cast<float>(i) / 20.0f;
            const P2 a = bezierCubic(c0, c1, c2, c3, t);
            const P2 b = bezierDeCasteljau({c0, c1, c2, c3}, t);
            if (std::fabs(a.x - b.x) > 1e-3f || std::fabs(a.y - b.y) > 1e-3f)
                agree = false;
        }
        check(agree, "de Casteljau agrees for the cubic too");
    }
    {   // the curve stays inside its control points' box
        const std::vector<P2> ctrl{{0, 0}, {10, 80}, {90, 80}, {100, 0}};
        bool inside = true;
        for (int i = 0; i <= 50; ++i) {
            const P2 q = bezierDeCasteljau(ctrl, static_cast<float>(i) / 50.0f);
            if (q.x < -1e-3f || q.x > 100.0f + 1e-3f) inside = false;
            if (q.y < -1e-3f || q.y > 80.0f + 1e-3f) inside = false;
        }
        check(inside, "Bezier stays within the control polygon's bounding box");
    }
    {   const std::vector<P2> pts = sampleBezier({p0, p1, p2}, 16);
        check(pts.size() == 17, "sampling n segments gives n+1 points");
    }

    {   // Catmull-Rom passes through its points
        const P2 a{0, 0}, b{10, 20}, c{30, 25}, d{40, 0};
        const P2 start = catmullRom(a, b, c, d, 0.0f);
        const P2 end   = catmullRom(a, b, c, d, 1.0f);
        checkNear(start.x, b.x, 1e-2f, "Catmull-Rom passes through p1 (x)");
        checkNear(start.y, b.y, 1e-2f, "Catmull-Rom passes through p1 (y)");
        checkNear(end.x, c.x, 1e-2f, "Catmull-Rom passes through p2 (x)");
        checkNear(end.y, c.y, 1e-2f, "Catmull-Rom passes through p2 (y)");
    }
    {   // a closed loop through a square returns to where it began
        const std::vector<P2> square{{0, 0}, {100, 0}, {100, 100}, {0, 100}};
        const std::vector<P2> loop = sampleCatmullRomLoop(square, 12);
        check(loop.size() == 48, "closed loop samples every span");
        checkNear(loop.front().x, 0.0f, 1e-2f, "loop starts at the first point");
        checkNear(loop.front().y, 0.0f, 1e-2f, "loop starts at the first point (y)");
    }
    {   // an open path keeps both endpoints
        const std::vector<P2> path{{0, 0}, {10, 10}, {20, 0}, {30, 10}};
        const std::vector<P2> s = sampleCatmullRomOpen(path, 8);
        checkNear(s.front().x, 0.0f, 1e-3f, "open spline starts at the first point");
        checkNear(s.back().x, 30.0f, 1e-3f, "open spline ends at the last point");
    }
    {   // arc length: equal distances, equal spacing
        const std::vector<P2> line{{0, 0}, {10, 0}, {20, 0}, {30, 0}};
        const std::vector<float> len = arcLengths(line);
        checkNear(len.back(), 30.0f, 1e-3f, "arc length of a straight run");

        const P2 q = pointAtDistance(line, len, 15.0f);
        checkNear(q.x, 15.0f, 1e-2f, "point at distance lands halfway");

        // and it wraps around
        const P2 w = pointAtDistance(line, len, 35.0f);
        checkNear(w.x, 5.0f, 1e-2f, "distance wraps around the loop");
    }
    {   // centripetal parameterisation should not overshoot where uniform does
        const std::vector<P2> uneven{{0, 0}, {1, 0}, {100, 0}, {101, 0}};
        bool monotonic = true;
        float prev = -1e9f;
        for (int i = 0; i <= 30; ++i) {
            const P2 q = catmullRom(uneven[0], uneven[1], uneven[2], uneven[3],
                                    static_cast<float>(i) / 30.0f, 0.5f);
            if (q.x < prev - 1e-3f) monotonic = false;
            prev = q.x;
        }
        check(monotonic, "centripetal spline does not double back on uneven spacing");
    }
}

// ---- Cohen-Sutherland ------------------------------------------------------

void testClipping() {
    std::printf("Cohen-Sutherland clipping\n");

    {   float x0 = 2, y0 = 2, x1 = 8, y1 = 8;
        check(clipCohenSutherland(x0, y0, x1, y1, 0, 0, 10, 10), "fully inside accepted");
        checkNear(x0, 2, 1e-4f, "inside segment start unchanged");
        checkNear(y1, 8, 1e-4f, "inside segment end unchanged");
    }
    {   float x0 = -5, y0 = 5, x1 = 15, y1 = 5;
        check(clipCohenSutherland(x0, y0, x1, y1, 0, 0, 10, 10), "crossing segment accepted");
        checkNear(x0, 0, 1e-4f, "clipped to the left edge");
        checkNear(x1, 10, 1e-4f, "clipped to the right edge");
    }
    {   float x0 = -5, y0 = -5, x1 = -1, y1 = -1;
        check(!clipCohenSutherland(x0, y0, x1, y1, 0, 0, 10, 10), "wholly outside rejected");
    }
    {   float x0 = 20, y0 = 5, x1 = 30, y1 = 5;
        check(!clipCohenSutherland(x0, y0, x1, y1, 0, 0, 10, 10), "past the right edge rejected");
    }
    {   // a diagonal past a corner must not be wrongly rejected
        float x0 = -2, y0 = 5, x1 = 5, y1 = 12;
        check(clipCohenSutherland(x0, y0, x1, y1, 0, 0, 10, 10), "corner crossing accepted");
        check(x0 >= -1e-3f && y1 <= 10 + 1e-3f, "corner crossing lands on the boundary");
    }
    check(outcode(-1, 5, 0, 0, 10, 10) == kLeft, "outcode left");
    check(outcode(5, 11, 0, 0, 10, 10) == kTop, "outcode top");
    check(outcode(-1, -1, 0, 0, 10, 10) == (kLeft | kBottom), "outcode corner");
}

// ---- fills -----------------------------------------------------------------

void testFills() {
    std::printf("Area fills\n");

    {   // a 10x10 square footprint fills 100 pixels, not 99 or 121
        Canvas c(32, 32);
        c.clear(kBg);
        std::vector<Point> square{{5, 5}, {15, 5}, {15, 15}, {5, 15}};
        fillPolygonScanline(c, square, kInk);
        check(countSet(c, kInk) == 100, "scanline fill of a 10x10 square covers 100 px");
        check(c.get(10, 10) == kInk, "interior is filled");
        check(!(c.get(4, 10) == kInk), "outside the left edge is untouched");
    }
    {   // concave outline: the parity rule must leave the notch empty
        Canvas c(32, 32);
        c.clear(kBg);
        std::vector<Point> ell{{4, 4}, {20, 4}, {20, 10}, {10, 10}, {10, 20}, {4, 20}};
        fillPolygonScanline(c, ell, kInk);
        check(c.get(6, 18) == kInk, "concave shape fills its lower arm");
        check(!(c.get(16, 18) == kInk), "concave notch stays empty");
    }
    {   // boundary fill stops at the boundary colour and does not leak
        Canvas c(32, 32);
        c.clear(kBg);
        const Colour edge{200, 0, 0, 255};
        for (int x = 5; x <= 15; ++x) {
            c.set(x, 5, edge);
            c.set(x, 15, edge);
        }
        for (int y = 5; y <= 15; ++y) {
            c.set(5, y, edge);
            c.set(15, y, edge);
        }
        fillBoundary(c, 10, 10, kInk, edge);
        check(countSet(c, kInk) == 81, "boundary fill covers the 9x9 interior");
        check(!(c.get(2, 2) == kInk), "boundary fill did not leak outside");
    }
    {   // must terminate rather than recurse forever when fill == boundary
        Canvas c(16, 16);
        c.clear(kBg);
        fillBoundary(c, 8, 8, kInk, kInk);
        check(true, "fill colour equal to boundary colour terminates");
    }
}

// ---- thick lines -----------------------------------------------------------

void testLineThick() {
    std::printf("Thick lines\n");

    {   // a horizontal thick line covers its length times its thickness
        Canvas c(40, 40);
        c.clear(kBg);
        lineThick(c, 5, 20, 30, 20, 3, kInk);
        check(countSet(c, kInk) == 26 * 3, "3 px thick horizontal line covers 26 x 3 pixels");
        check(c.get(5, 19) == kInk && c.get(5, 21) == kInk, "thickness is spread both sides");
        check(c.get(5, 18) != kInk && c.get(5, 22) != kInk, "and no wider than asked");
    }
    {   // a vertical one is the same, turned
        Canvas c(40, 40);
        c.clear(kBg);
        lineThick(c, 20, 5, 20, 30, 3, kInk);
        check(countSet(c, kInk) == 26 * 3, "3 px thick vertical line covers 26 x 3 pixels");
    }
    {   // thickness 1 is exactly Bresenham
        Canvas a(40, 40), b(40, 40);
        a.clear(kBg);
        b.clear(kBg);
        lineThick(a, 3, 4, 33, 17, 1, kInk);
        lineBresenham(b, 3, 4, 33, 17, kInk);
        bool same = true;
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 40; ++x)
                if (a.get(x, y) != b.get(x, y)) same = false;
        check(same, "thickness 1 matches Bresenham exactly");
    }
    {   // a diagonal thick line is several pixels thick all along its length
        Canvas c(60, 60);
        c.clear(kBg);
        lineThick(c, 5, 5, 50, 40, 4, kInk);
        bool thick = true;
        for (int x = 10; x < 46; ++x) {
            int n = 0;
            for (int y = 0; y < 60; ++y) if (c.get(x, y) == kInk) ++n;
            if (n < 3) thick = false;
        }
        check(thick, "diagonal thick line is at least 3 px thick in every column");
    }
    {   // a zero length line is a single dot, not a crash
        Canvas c(10, 10);
        c.clear(kBg);
        lineThick(c, 4, 4, 4, 4, 5, kInk);
        check(countSet(c, kInk) == 1, "zero length thick line is one pixel");
    }
}

// ---- polygons --------------------------------------------------------------

void testPolygons() {
    std::printf("Polygons\n");
    using namespace lumen::poly;

    const Polygon square = rectangle(0, 0, 4, 4);

    {   checkNear(area(square), 16.0f, 1e-4f, "shoelace area of a 4 x 4 square");
        check(signedArea(square) > 0.0f, "anticlockwise vertices give positive area");
        const PointF c = centroid(square);
        checkNear(c.x, 2.0f, 1e-4f, "centroid x");
        checkNear(c.y, 2.0f, 1e-4f, "centroid y");
    }
    {   check(contains(square, {2, 2}), "centre is inside");
        check(!contains(square, {5, 2}), "point to the right is outside");
        const Polygon s = star({0, 0}, 10, 4, 5);
        check(contains(s, {0, 0}), "star centre is inside");
        check(!contains(s, {0, 9}), "point between star arms is outside (even-odd)");
        check(isConvex(square), "a square is convex");
        check(!isConvex(s), "a star is not convex");
    }
    {   // an ellipse polygon has close to pi rx ry area
        const Polygon e = ellipse({0, 0}, 10, 5, 200);
        checkNear(area(e), 3.14159265f * 50.0f, 1.0f, "ellipse polygon area is about pi rx ry");
    }
}

void testPolygonClipping() {
    std::printf("Sutherland-Hodgman polygon clipping\n");
    using namespace lumen::poly;

    {   // fully inside: unchanged
        const Polygon in = rectangle(2, 2, 4, 4);
        const Polygon out = clipSutherlandHodgman(in, 0, 0, 10, 10);
        check(out.size() == 4, "inside polygon keeps its 4 vertices");
        checkNear(area(out), 4.0f, 1e-4f, "inside polygon keeps its area");
    }
    {   // fully outside: nothing left
        const Polygon out = clipSutherlandHodgman(rectangle(20, 20, 30, 30), 0, 0, 10, 10);
        check(out.empty(), "outside polygon is removed");
    }
    {   // overlapping a corner: only the overlap is left
        const Polygon out = clipSutherlandHodgman(rectangle(5, 5, 15, 15), 0, 0, 10, 10);
        checkNear(area(out), 25.0f, 1e-3f, "corner overlap is 5 x 5");
        bool allIn = true;
        for (const PointF& p : out)
            if (p.x < -1e-4f || p.x > 10.0001f || p.y < -1e-4f || p.y > 10.0001f) allIn = false;
        check(allIn, "every clipped vertex is inside the window");
    }
    {   // bigger than the window on every side: becomes the window
        const Polygon out = clipSutherlandHodgman(rectangle(-5, -5, 15, 15), 0, 0, 10, 10);
        checkNear(area(out), 100.0f, 1e-3f, "a covering polygon clips to the whole window");
    }
    {   // a triangle crossing one edge gains a vertex
        const Polygon tri{{5, 2}, {14, 5}, {5, 8}};
        const Polygon out = clipSutherlandHodgman(tri, 0, 0, 10, 10);
        check(out.size() == 4, "a triangle cut by one edge becomes a quad");
    }
    {   // concave star: clipped area is never more than either shape
        const Polygon s = star({10, 5}, 8, 3, 6);
        const Polygon out = clipSutherlandHodgman(s, 0, 0, 10, 10);
        check(area(out) <= area(s) + 1e-3f && area(out) <= 100.0f + 1e-3f,
              "clipped area is no larger than the polygon or the window");
        check(area(out) > 0.0f, "a star across the edge leaves something");
    }
}

// ---- the plan file ---------------------------------------------------------

std::string writeTemp(const std::string& name, const std::string& text) {
    const std::string path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream(path) << text;
    return path;
}

bool loadFails(const std::string& path, const std::string& expectInMessage) {
    try {
        Plan::load(path);
    } catch (const std::exception& e) {
        return std::string(e.what()).find(expectInMessage) != std::string::npos;
    }
    return false;
}

void testPlanLoad() {
    std::printf("Plan loading\n");

    {   const Plan p = Plan::load(std::string(LUMEN_ASSET_DIR) + "plan.json");
        check(p.rooms.size() == 5, "the real plan has five rooms");
        check(p.walls.size() == 61, "the real plan has 61 wall segments");
        check(p.nicheOpenings.size() == 4, "four niches open onto the corridor");
        check(p.doorways.size() == 7, "seven doorways: five rooms, the atrium and the vestibule");
        bool sevenEach = true;
        for (const RoomPlan& r : p.rooms) if (r.hangs.size() != 7) sevenEach = false;
        check(sevenEach, "every room has seven hang points");
        check(p.route.size() >= 4, "the tour route is loaded");
    }
    {   check(loadFails("no/such/folder/plan.json", "Could not open"),
              "a missing file gives a clear message");
    }
    {   const std::string bad = writeTemp("lumen_bad_syntax.json", "{ \"rooms\": [ 1, 2, ");
        check(loadFails(bad, "not valid JSON"), "broken JSON gives a clear message");
        std::remove(bad.c_str());
    }
    {   const std::string bad = writeTemp("lumen_missing_field.json",
                                          "{ \"rooms\": [], \"walls\": [] }");
        check(loadFails(bad, "missing a field"), "a missing field names the problem");
        check(loadFails(bad, "lumen_missing_field.json"), "and names the file");
        std::remove(bad.c_str());
    }
    {   // every doorway really is a gap: no wall runs through its middle
        const Plan p = Plan::load(std::string(LUMEN_ASSET_DIR) + "plan.json");
        Collider col(p.walls, p.wallThickness);
        bool open = true;
        for (const Segment& d : p.doorways) {
            const Vec2 mid{(d.a.x + d.b.x) * 0.5f, (d.a.y + d.b.y) * 0.5f};
            const float nx = d.horizontal() ? 0.0f : 1.0f;
            const float ny = d.horizontal() ? 1.0f : 0.0f;
            if (col.segmentBlocked(Vec2{mid.x - nx, mid.y - ny}, Vec2{mid.x + nx, mid.y + ny}))
                open = false;
        }
        check(open, "every doorway is an opening in the walls");
    }
}

// ---- image export ----------------------------------------------------------

void testImage() {
    std::printf("Image export\n");

    Canvas c(5, 3);   // odd width, so rows need padding
    c.clear(Colour{10, 20, 30, 255});
    c.set(0, 0, Colour{255, 0, 0, 255});   // top left red

    const std::string path = (std::filesystem::temp_directory_path() / "lumen_test.bmp").string();
    check(image::writeBmp(c, path), "BMP is written");

    std::ifstream in(path, std::ios::binary);
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(in)), {});
    check(b.size() == 54 + 16 * 3, "file size is header plus padded rows (16 bytes each)");
    check(b.size() > 2 && b[0] == 'B' && b[1] == 'M', "starts with BM");
    // Rows are stored bottom up, so the top left pixel is in the last row.
    const std::size_t topLeft = 54 + 2 * 16;
    check(b.size() > topLeft + 2 && b[topLeft] == 0 && b[topLeft + 1] == 0 && b[topLeft + 2] == 255,
          "top left pixel is red, stored as blue green red in the last row");
    in.close();
    std::remove(path.c_str());
}

// ---- the JSON reader -------------------------------------------------------

void testJson() {
    std::printf("JSON reader\n");
    using lumen::json::Value;

    const Value v = Value::parse(R"({"a": 1.5, "b": [1, 2, 3], "c": {"d": "text"}, "e": true})");
    checkNear(static_cast<float>(v.at("a").number()), 1.5f, 1e-6f, "reads a number");
    check(v.at("b").size() == 3, "reads a list");
    check(v.at("c").at("d").string() == "text", "reads a nested string");
    check(v.contains("e") && !v.contains("z"), "knows which keys exist");
    checkNear(static_cast<float>(v.numberOr("z", 7.0)), 7.0f, 1e-6f, "falls back for a missing key");

    bool threw = false;
    try { Value::parse("{\"a\": [1, 2"); } catch (const std::exception&) { threw = true; }
    check(threw, "unfinished JSON is rejected");

    threw = false;
    try { v.at("missing"); } catch (const std::exception& e) {
        threw = std::string(e.what()).find("missing") != std::string::npos;
    }
    check(threw, "a missing key names itself in the error");
}

// ---- the pages -------------------------------------------------------------

void testPages() {
    std::printf("Pages\n");

    const Plan plan = Plan::load(std::string(LUMEN_ASSET_DIR) + "plan.json");
    Canvas c(1280, 720);
    bool allDrawn = true;
    for (int i = 0; i < demo::kPageCount; ++i) {
        demo::renderPage(c, static_cast<demo::Page>(i), &plan);
        // Count pixels that are not the background colour.
        const Colour bg = c.get(2, 2);
        int marks = 0;
        for (int y = 0; y < c.height(); y += 2)
            for (int x = 0; x < c.width(); x += 2)
                if (c.get(x, y) != bg) ++marks;
        if (marks < 2000) allDrawn = false;
    }
    check(allDrawn, "every page draws something");
    check(demo::kPageCount == 12, "twelve pages");

    // The generated artwork comes from fixed seeds, so it is the same every time.
    Canvas first(1280, 720), second(1280, 720);
    demo::renderPage(first, demo::Page::Artwork, &plan);
    demo::renderPage(second, demo::Page::Artwork, &plan);
    bool same = true;
    for (int y = 0; y < first.height() && same; ++y)
        for (int x = 0; x < first.width(); ++x)
            if (first.get(x, y) != second.get(x, y)) { same = false; break; }
    check(same, "the same seeds give the same artwork");
}

// ---- 2D transformations ----------------------------------------------------

bool near(xf::PointF p, float x, float y, float tol = 1e-3f) {
    return std::fabs(p.x - x) < tol && std::fabs(p.y - y) < tol;
}

void testTransforms() {
    std::printf("2D transformations\n");
    const xf::PointF p{3, 4};

    check(near(xf::translation(5, -2).apply(p), 8, 2), "translate moves by (5, -2)");
    check(near(xf::rotation(kTestPi / 2).apply(xf::PointF{1, 0}), 0, 1), "rotate (1, 0) by 90 degrees gives (0, 1)");
    check(near(xf::scaling(2, 3).apply(p), 6, 12), "scale multiplies x and y");
    check(near(xf::reflectionX().apply(p), 3, -4), "reflect in the x axis flips y");
    check(near(xf::reflectionY().apply(p), -3, 4), "reflect in the y axis flips x");
    check(near(xf::shearing(0.5f, 0).apply(p), 5, 4), "shear x moves x by half of y");

    {   // a composite matrix is the same as applying each in turn
        const xf::Mat3 m = xf::translation(3, -2) * xf::rotation(0.4f) * xf::scaling(1.5f, 0.5f);
        const xf::PointF once = m.apply(xf::PointF{2, 7});
        const xf::PointF steps = xf::translation(3, -2).apply(
            xf::rotation(0.4f).apply(xf::scaling(1.5f, 0.5f).apply(xf::PointF{2, 7})));
        check(near(once, steps.x, steps.y), "composite matrix matches step by step");
        check(!near(xf::rotation(0.4f).apply(xf::translation(3, 0).apply(p)),
                    xf::translation(3, 0).apply(xf::rotation(0.4f).apply(p)).x,
                    xf::translation(3, 0).apply(xf::rotation(0.4f).apply(p)).y),
              "order matters: rotate then move is not move then rotate");
    }
    {   // about a point: the point itself does not move
        const xf::PointF pivot{10, 20};
        check(near(xf::rotationAbout(1.1f, pivot).apply(pivot), 10, 20), "rotating about a point leaves it fixed");
        check(near(xf::scalingAbout(3, 2, pivot).apply(pivot), 10, 20), "scaling about a point leaves it fixed");
        const xf::Mat3 mirror = xf::reflectionAbout(xf::PointF{0, 5}, 0.0f);   // the line y = 5
        check(near(mirror.apply(xf::PointF{2, 8}), 2, 2), "reflect in the line y = 5");
        const xf::PointF twice = mirror.apply(mirror.apply(xf::PointF{7, -3}));
        check(near(twice, 7, -3), "reflecting twice gives the point back");
    }
    {   // the inverse undoes a matrix
        const xf::Mat3 m = xf::translation(4, 9) * xf::rotation(0.7f) * xf::scaling(2, 0.5f) * xf::shearing(0.3f, 0);
        const xf::PointF back = xf::inverse(m).apply(m.apply(p));
        check(near(back, 3, 4), "inverse undoes the composite");
    }
    {   // what transformations do to area
        const poly::Polygon sq = poly::rectangle(0, 0, 4, 4);
        checkNear(poly::area(xf::transform(sq, xf::rotationAbout(0.9f, {2, 2}))), 16.0f, 1e-3f, "rotation keeps area");
        checkNear(poly::area(xf::transform(sq, xf::shearing(0.7f, 0))), 16.0f, 1e-3f, "shear keeps area");
        checkNear(poly::area(xf::transform(sq, xf::scaling(2, 3))), 96.0f, 1e-3f, "scale by 2 and 3 multiplies area by 6");
        check(poly::signedArea(xf::transform(sq, xf::reflectionX())) < 0.0f &&
              poly::signedArea(sq) > 0.0f, "reflection reverses the vertex order");
    }
}

// ---- 3D transformations ----------------------------------------------------

void testTransforms3D() {
    std::printf("3D transformations\n");
    auto near3 = [](xf3::Vec3 v, float x, float y, float z) {
        return std::fabs(v.x - x) < 1e-3f && std::fabs(v.y - y) < 1e-3f && std::fabs(v.z - z) < 1e-3f;
    };
    check(near3(xf3::translation(1, 2, 3).apply({1, 1, 1}), 2, 3, 4), "3D translate");
    check(near3(xf3::rotationY(kTestPi / 2).apply({1, 0, 0}), 0, 0, -1), "turning (1,0,0) 90 degrees about y gives (0,0,-1)");
    check(near3(xf3::rotationX(kTestPi / 2).apply({0, 1, 0}), 0, 0, 1), "turning (0,1,0) 90 degrees about x gives (0,0,1)");
    check(near3(xf3::rotationZ(kTestPi / 2).apply({1, 0, 0}), 0, 1, 0), "turning (1,0,0) 90 degrees about z gives (0,1,0)");

    const xf3::Mat4 view = xf3::lookAt({0, 0, 10}, {0, 0, 0});
    check(near3(view.apply({0, 0, 10}), 0, 0, 0), "the camera sits at the origin of its own view");
    check(near3(view.apply({0, 0, 0}), 0, 0, -10), "the target is straight ahead, down -z");

    const xf3::Mat4 vp = xf3::perspective(1.0f, 1.0f, 0.1f, 100.0f) * view;
    float sx = 0, sy = 0;
    check(xf3::project(vp, {0, 0, 0}, 200, 200, sx, sy) && std::fabs(sx - 100) < 1e-2f && std::fabs(sy - 100) < 1e-2f,
          "a point straight ahead lands in the middle of the screen");
    float nearX = 0, farX = 0, unused = 0;
    xf3::project(vp, {1, 0, 5}, 200, 200, nearX, unused);
    xf3::project(vp, {1, 0, -20}, 200, 200, farX, unused);
    check(nearX - 100 > farX - 100 && farX > 100, "further away looks smaller (perspective)");
    check(!xf3::project(vp, {0, 0, 20}, 200, 200, sx, sy), "a point behind the camera is not drawn");

    const Plan plan = Plan::load(std::string(LUMEN_ASSET_DIR) + "plan.json");
    const xf3::Wire w = xf3::roomWire(plan.rooms[1], plan.wallHeight, plan.vaultApex);
    bool inside = true;
    for (const xf3::Vec3& p : w.points)
        if (p.y < -1e-3f || p.y > plan.vaultApex + 1e-3f) inside = false;
    check(!w.edges.empty() && inside, "the room wireframe sits between the floor and the top of the vault");
}

// ---- animation -------------------------------------------------------------

void testAnimation() {
    std::printf("Animation\n");
    checkNear(anim::lerp(2.0f, 6.0f, 0.25f), 3.0f, 1e-6f, "lerp a quarter of the way");
    checkNear(anim::easeInOut(0.0f), 0.0f, 1e-6f, "ease starts at 0");
    checkNear(anim::easeInOut(1.0f), 1.0f, 1e-6f, "ease ends at 1");
    checkNear(anim::easeInOut(0.5f), 0.5f, 1e-6f, "ease is half way at half time");
    check(anim::easeInOut(0.1f) < 0.1f, "ease starts slower than a straight line");
    bool rising = true;
    for (int i = 0; i < 100; ++i)
        if (anim::easeInOut((i + 1) / 100.0f) < anim::easeInOut(i / 100.0f)) rising = false;
    check(rising, "ease never goes backwards");

    const anim::Track track({{0.0f, 10.0f}, {2.0f, 20.0f}, {4.0f, 0.0f}});
    checkNear(track.at(2.0f), 20.0f, 1e-5f, "a track hits its keyframe");
    checkNear(track.at(1.0f), 15.0f, 1e-5f, "half way between keys is half way between values");
    checkNear(track.at(-5.0f), 10.0f, 1e-5f, "before the first key it holds the first value");
    checkNear(track.at(9.0f), 0.0f, 1e-5f, "after the last key it holds the last value");
    checkNear(anim::loop(13.0f, 4.0f), 0.25f, 1e-5f, "loop wraps time into 0 to 1");

    const Colour mid = anim::lerp(Colour{0, 100, 200, 255}, Colour{100, 200, 0, 255}, 0.5f);
    check(mid.r == 50 && mid.g == 150 && mid.b == 100, "colours blend half way");
}

// ---- GIF export ------------------------------------------------------------

// A small LZW decoder, to check that what the encoder writes decodes back.
std::vector<unsigned char> lzwDecode(const std::vector<unsigned char>& data) {
    std::vector<std::vector<unsigned char>> dict;
    auto reset = [&] {
        dict.clear();
        for (int i = 0; i < 258; ++i) dict.push_back({static_cast<unsigned char>(i)});
    };
    reset();
    std::vector<unsigned char> out;
    unsigned size = 9, pos = 0;
    int prev = -1;
    auto read = [&](unsigned bits) {
        unsigned v = 0;
        for (unsigned b = 0; b < bits; ++b, ++pos)
            if (pos / 8 < data.size() && (data[pos / 8] >> (pos % 8)) & 1) v |= 1u << b;
        return v;
    };
    while (pos + size <= data.size() * 8) {
        const unsigned code = read(size);
        if (code == 256) { reset(); size = 9; prev = -1; continue; }
        if (code == 257) break;
        std::vector<unsigned char> entry;
        if (code < dict.size()) entry = dict[code];
        else if (prev >= 0) { entry = dict[prev]; entry.push_back(dict[prev][0]); }
        else break;
        out.insert(out.end(), entry.begin(), entry.end());
        if (prev >= 0 && dict.size() < 4096) {
            std::vector<unsigned char> add = dict[prev];
            add.push_back(entry[0]);
            dict.push_back(add);
        }
        if (dict.size() == (std::size_t{1} << size) && size < 12) ++size;
        prev = static_cast<int>(code);
    }
    return out;
}

void testGif() {
    std::printf("GIF export\n");
    {   // LZW: long, repetitive and random input all come back exactly
        std::vector<unsigned char> in;
        unsigned seed = 7;
        for (int i = 0; i < 60000; ++i) {
            seed = seed * 1103515245u + 12345u;
            in.push_back(static_cast<unsigned char>(i < 20000 ? (i / 50) % 7 : (seed >> 16) % 256));
        }
        check(lzwDecode(gif::lzwEncode(in)) == in, "LZW output decodes back to the same pixels");
    }
    {   // few colours are kept exactly
        Canvas c(40, 30);
        c.clear(Colour{10, 20, 30, 255});
        circleFilled(c, 20, 15, 8, Colour{250, 200, 100, 255});
        const gif::Indexed img = gif::quantise(c);
        const Colour got = img.palette[img.pixels[15 * 40 + 20]];
        check(got == Colour{250, 200, 100, 255}, "a frame with few colours keeps them exactly");
    }
    {   // the file has the right shape
        const std::string path = (std::filesystem::temp_directory_path() / "lumen_test.gif").string();
        gif::Writer w;
        check(w.open(path, 32, 16), "GIF opens");
        Canvas c(32, 16);
        for (int f = 0; f < 3; ++f) {
            c.clear(Colour{static_cast<std::uint8_t>(f * 80), 0, 0, 255});
            w.addFrame(c, 10);
        }
        check(w.frames() == 3 && w.close(), "three frames written and the file closed");
        std::ifstream in(path, std::ios::binary);
        std::vector<unsigned char> b((std::istreambuf_iterator<char>(in)), {});
        check(b.size() > 13 && std::string(b.begin(), b.begin() + 6) == "GIF89a", "starts with GIF89a");
        check(b.size() > 13 && b[6] == 32 && b[8] == 16, "width and height in the header");
        check(!b.empty() && b.back() == 0x3B, "ends with the GIF trailer");
        in.close();
        std::remove(path.c_str());
    }
}

// ---- the animations --------------------------------------------------------

void testAnimations() {
    std::printf("Animations\n");
    const Plan plan = Plan::load(std::string(LUMEN_ASSET_DIR) + "plan.json");
    bool allMove = true, allDraw = true;
    for (const demo::Animation& a : demo::animations()) {
        Canvas first(a.width, a.height), later(a.width, a.height);
        a.draw(first, &plan, 0.0f);
        a.draw(later, &plan, a.seconds * 0.4f);
        int differ = 0, marks = 0;
        const Colour bg = first.get(1, 1);
        for (int y = 0; y < a.height; y += 2)
            for (int x = 0; x < a.width; x += 2) {
                if (first.get(x, y) != later.get(x, y)) ++differ;
                if (first.get(x, y) != bg) ++marks;
            }
        if (differ < 200) allMove = false;
        if (marks < 500) allDraw = false;
    }
    check(allDraw, "every animation draws something");
    check(allMove, "every animation changes over time");
    check(demo::animations().size() == 5, "five animations");
}

// ============================================================================
// The gallery (Checkpoint 3)
// ============================================================================

// ---- geometry winding ------------------------------------------------------
// Every face must point out of its solid. A backwards face is culled and lit black.

void testWinding() {
    std::printf("Geometry winding\n");

    // Two parallel walls 4 m apart, with the visitor between them.
    Plan plan;
    plan.wallThickness = 0.45f;
    plan.wallHeight    = 4.2f;
    plan.walls = {
        Segment{{0.0f, 0.0f}, {10.0f, 0.0f}},
        Segment{{0.0f, 4.0f}, {10.0f, 4.0f}},
    };

    const gfx::MeshData mesh = build::extrudeWalls(plan);
    check(!mesh.vertices.empty(), "wall extrusion produces geometry");
    check(mesh.triangleCount() == 20, "each wall segment makes ten triangles");

    // The visitor stands between the walls, at head height.
    const glm::vec3 eye(5.0f, 1.65f, -2.0f);   // plan (5, 2): world z is -plan y

    // Any triangle facing the visitor must have its normal pointing at them.
    int facing = 0, backward = 0;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const glm::vec3 a = mesh.vertices[mesh.indices[i + 0]].position;
        const glm::vec3 b = mesh.vertices[mesh.indices[i + 1]].position;
        const glm::vec3 c = mesh.vertices[mesh.indices[i + 2]].position;

        const glm::vec3 geometric = glm::normalize(glm::cross(b - a, c - a));
        const glm::vec3 stored    = mesh.vertices[mesh.indices[i]].normal;

        // The stored normal must match the winding.
        if (glm::dot(geometric, stored) < 0.9f) ++backward;

        const glm::vec3 toEye = glm::normalize(eye - a);
        if (glm::dot(geometric, toEye) > 0.25f) ++facing;
    }
    check(backward == 0, "stored normals agree with the winding on every triangle");
    check(facing > 0, "some wall faces are turned toward a visitor between them");

    // The two inner faces the visitor sees: the y=0 wall faces north (world -Z),
    // the y=4 wall faces south (+Z).
    bool northFace = false, southFace = false;
    for (const gfx::Vertex& v : mesh.vertices) {
        // Inner face of the south wall sits at plan y = +0.225, world z = -0.225
        if (std::fabs(v.position.z + 0.225f) < 1e-3f && v.normal.z < -0.9f)
            northFace = true;
        // Inner face of the north wall sits at plan y = 3.775, world z = -3.775
        if (std::fabs(v.position.z + 3.775f) < 1e-3f && v.normal.z > 0.9f)
            southFace = true;
    }
    check(northFace, "the south wall's inner face points into the corridor");
    check(southFace, "the north wall's inner face points into the corridor");

    // A box: every face points away from its centre.
    {
        const glm::vec3 c(1.0f, 0.5f, -2.0f);
        const gfx::MeshData box = build::buildBox(c, glm::vec3(1.0f, 1.0f, 1.0f));
        check(box.triangleCount() == 12, "a box is twelve triangles");

        bool allOutward = true;
        for (std::size_t i = 0; i + 2 < box.indices.size(); i += 3) {
            const glm::vec3 a = box.vertices[box.indices[i + 0]].position;
            const glm::vec3 b = box.vertices[box.indices[i + 1]].position;
            const glm::vec3 d = box.vertices[box.indices[i + 2]].position;
            const glm::vec3 n = glm::normalize(glm::cross(b - a, d - a));
            const glm::vec3 outward = glm::normalize(((a + b + d) / 3.0f) - c);
            if (glm::dot(n, outward) < 0.5f) allOutward = false;
        }
        check(allOutward, "every box face points away from its centre");
    }

    // A cylinder: sides face away from the axis, the cap faces up.
    {
        const glm::vec3 base(2.0f, 1.0f, -3.0f);
        const gfx::MeshData cyl = build::buildCylinder(base, 0.5f, 1.0f, 16);
        bool outward = true;
        for (std::size_t i = 0; i + 2 < cyl.indices.size(); i += 3) {
            const glm::vec3 a = cyl.vertices[cyl.indices[i + 0]].position;
            const glm::vec3 b = cyl.vertices[cyl.indices[i + 1]].position;
            const glm::vec3 d = cyl.vertices[cyl.indices[i + 2]].position;
            const glm::vec3 n = glm::normalize(glm::cross(b - a, d - a));
            const glm::vec3 mid = (a + b + d) / 3.0f;
            glm::vec3 away = mid - glm::vec3(base.x, mid.y, base.z);   // from the axis
            if (std::fabs(mid.y - (base.y + 1.0f)) < 1e-4f) away = glm::vec3(0, 1, 0);   // the cap
            if (glm::dot(n, glm::normalize(away)) < 0.5f) outward = false;
            if (glm::dot(n, cyl.vertices[cyl.indices[i]].normal) < 0.5f) outward = false;
        }
        check(outward, "every cylinder face points outward, matching its stored normal");
    }

    // A vault must be visible from below.
    {
        RoomPlan room;
        room.id = "RT";
        room.rect = Rect{0.0f, 0.0f, 10.0f, 9.0f};
        room.vaults = {Segment{{5.0f, 0.9f}, {5.0f, 8.1f}}};
        const gfx::MeshData v = build::buildRoomVaults(room, 4.2f, 6.0f);
        check(!v.vertices.empty(), "a vault produces geometry");

        const glm::vec3 below(5.0f, 1.65f, -4.5f);   // standing under it
        int visible = 0;
        for (std::size_t i = 0; i + 2 < v.indices.size(); i += 3) {
            const glm::vec3 a = v.vertices[v.indices[i + 0]].position;
            const glm::vec3 b = v.vertices[v.indices[i + 1]].position;
            const glm::vec3 d = v.vertices[v.indices[i + 2]].position;
            const glm::vec3 n = glm::normalize(glm::cross(b - a, d - a));
            if (glm::dot(n, glm::normalize(below - a)) > 0.1f) ++visible;
        }
        check(visible > 0, "the vault is visible from under it");
    }

    // Floors face up.
    Plan fp = plan;
    fp.bounds = Rect{0, 0, 10, 10};
    fp.atriumRect = Rect{2, 2, 8, 8};
    fp.vestibuleRect = Rect{0, 0, 1, 1};
    fp.corridorOuter = Rect{0, 0, 10, 10};
    fp.corridorInner = Rect{2, 2, 8, 8};
    const gfx::MeshData floors = build::buildFloors(fp);
    bool allUp = true;
    for (const gfx::Vertex& v : floors.vertices)
        if (v.normal.y < 0.9f) allUp = false;
    check(allUp, "every floor face points up");
}

// ---- collision -------------------------------------------------------------

void testCollision() {
    std::printf("Collision\n");

    // A 10x10 room: walls on all four sides.
    std::vector<Segment> walls{
        {{0, 0}, {10, 0}}, {{10, 0}, {10, 10}},
        {{10, 10}, {0, 10}}, {{0, 10}, {0, 0}},
    };
    Collider col(walls, 0.45f);
    const float r = 0.32f;
    const float clear = r + 0.225f;   // radius + half thickness

    {   Vec2 p = col.resolve(Vec2{5, 5}, r);
        checkNear(p.x, 5, 1e-3f, "a point in open floor is not moved (x)");
        checkNear(p.y, 5, 1e-3f, "a point in open floor is not moved (y)");
    }
    {   // walking into the south wall: pushed back to exactly the clearance
        Vec2 p = col.resolve(Vec2{5, 0.1f}, r);
        checkNear(p.y, clear, 1e-3f, "pushed out of the south wall to clearance");
        checkNear(p.x, 5, 1e-3f, "push is perpendicular, x unchanged");
    }
    {   // a corner has to satisfy both walls at once
        Vec2 p = col.resolve(Vec2{0.05f, 0.05f}, r);
        check(p.x >= clear - 1e-2f && p.y >= clear - 1e-2f,
              "corner resolves against both walls");
    }
    {   // and must not tunnel straight through a wall
        Vec2 p = col.resolve(Vec2{-1.0f, 5.0f}, r);
        check(p.x < 0.0f || p.x >= clear - 1e-2f, "does not end up embedded in a wall");
    }
    {   check(col.segmentBlocked(Vec2{5, 5}, Vec2{5, 20}), "line of sight blocked by a wall");
        check(!col.segmentBlocked(Vec2{2, 2}, Vec2{8, 8}), "line of sight clear inside the room");
    }
}

// ---- the reveal ------------------------------------------------------------

Plan makeTinyPlan() {
    Plan p;
    p.wallThickness = 0.45f;
    RoomPlan r;
    r.id   = "R1";
    r.name = "Test Room";
    r.rect = Rect{0, 0, 10, 10};
    p.rooms.push_back(r);
    return p;
}

void testReveal() {
    std::printf("Reveal state machine\n");

    Plan p = makeTinyPlan();
    LightingState ls(p);

    {   // outside the room, nothing is lit
        ls.update(Vec2{20, 20}, 0.1f);
        checkNear(ls.levelFor("R1"), 0.0f, 1e-4f, "corridor leaves the room dark");
        check(ls.currentRoomId().empty(), "no current room when outside");
    }
    {   // step inside and the vault fades up over kRevealFadeIn seconds
        for (int i = 0; i < 100; ++i) ls.update(Vec2{5, 5}, cfg::kRevealFadeIn / 100.0f);
        checkNear(ls.levelFor("R1"), cfg::kRevealLevel, 1e-3f, "reveal reaches the cap");
        check(ls.currentRoomId() == "R1", "current room is tracked");
    }
    {   // and it stops at the reveal level, not full brightness
        check(cfg::kRevealLevel < 1.0f, "the reveal never reaches full brightness");
        check(ls.levelFor("R1") <= cfg::kRevealLevel + 1e-4f, "level never exceeds the cap");
    }
    {   // halfway through the fade in it is about half
        LightingState fresh(p);
        for (int i = 0; i < 50; ++i) fresh.update(Vec2{5, 5}, cfg::kRevealFadeIn / 100.0f);
        const float half = fresh.levelFor("R1");
        check(half > 0.05f * cfg::kRevealLevel && half < 0.95f * cfg::kRevealLevel,
              "the reveal is a fade, not a switch");
    }
    {   // leaving fades it back down to nothing
        for (int i = 0; i < 100; ++i) ls.update(Vec2{20, 20}, cfg::kRevealFadeOut / 100.0f);
        checkNear(ls.levelFor("R1"), 0.0f, 1e-3f, "leaving fades the room back out");
    }
    {   // the reveal level can be changed at runtime
        LightingState ex(p);
        ex.setRevealLevel(1.0f);
        for (int i = 0; i < 200; ++i) ex.update(Vec2{5, 5}, 0.05f);
        checkNear(ex.levelFor("R1"), 1.0f, 1e-3f, "exhibition mode reaches full");

        ex.setRevealLevel(0.0f);
        for (int i = 0; i < 200; ++i) ex.update(Vec2{5, 5}, 0.05f);
        checkNear(ex.levelFor("R1"), 0.0f, 1e-3f, "lamp only mode fades to nothing");

        ex.setRevealLevel(5.0f);
        check(ex.revealLevel() <= 1.0f, "reveal level is clamped to 1");
        ex.setRevealLevel(-2.0f);
        check(ex.revealLevel() >= 0.0f, "reveal level is clamped to 0");
    }
    {   // standing in the doorway must not flicker
        LightingState edge(p);
        for (int i = 0; i < 20; ++i)
            edge.update(Vec2{5, cfg::kRevealMargin * 0.5f}, 0.016f);
        checkNear(edge.levelFor("R1"), 0.0f, 1e-3f, "the threshold margin prevents flicker");
    }
}

// ---- lamp ------------------------------------------------------------------

void testLamp() {
    std::printf("Lamp\n");

    Lamp lamp;
    // Looking down -Z (north), lamp starts aligned.
    lamp.follow(0, 1.65f, 0,  0, 0, -1,  1, 0, 0,  0.016f);
    checkNear(lamp.posY, 1.65f - cfg::kLampOffsetDown, 1e-4f, "lamp hangs below the eye");
    checkNear(lamp.posX, cfg::kLampOffsetRight, 1e-4f, "lamp sits to the right of the eye");

    // Snap the view 90 degrees; the lamp should lag, not teleport.
    lamp.follow(0, 1.65f, 0,  1, 0, 0,  0, 0, 1,  0.016f);
    check(lamp.dirZ < -0.5f, "lamp lags behind a sudden turn");

    // Given enough time it catches up.
    for (int i = 0; i < 200; ++i)
        lamp.follow(0, 1.65f, 0,  1, 0, 0,  0, 0, 1,  0.016f);
    checkNear(lamp.dirX, 1.0f, 1e-2f, "lamp settles on the view direction");

    const float len = std::sqrt(lamp.dirX * lamp.dirX + lamp.dirY * lamp.dirY +
                                lamp.dirZ * lamp.dirZ);
    checkNear(len, 1.0f, 1e-4f, "lamp direction stays normalised");

    check(lamp.innerCos > lamp.outerCos,
          "inner cone is tighter than the outer cone");
    check(cfg::kLampOuterDeg < 90.0f,
          "the lamp is a cone, not a bulb (one shadow pass, not six)");

    // The shadow map camera: straight ahead lands in the middle of the map,
    // and the edge of the cone still fits inside it.
    {
        const glm::mat4 vp = lampProjection(lamp) * lampView(lamp);
        const glm::vec3 pos(lamp.posX, lamp.posY, lamp.posZ);
        const glm::vec3 dir(lamp.dirX, lamp.dirY, lamp.dirZ);

        glm::vec4 c = vp * glm::vec4(pos + dir * 5.0f, 1.0f);
        c /= c.w;
        checkNear(c.x, 0.0f, 1e-3f, "a point straight ahead of the lamp is centred (x)");
        checkNear(c.y, 0.0f, 1e-3f, "a point straight ahead of the lamp is centred (y)");
        check(c.z > -1.0f && c.z < 1.0f, "and is between the near and far planes");

        const float edge = glm::radians(cfg::kLampOuterDeg);
        const glm::vec3 side = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
        const glm::vec3 rim = pos + (dir * std::cos(edge) + side * std::sin(edge)) * 5.0f;
        glm::vec4 r = vp * glm::vec4(rim, 1.0f);
        r /= r.w;
        check(std::fabs(r.x) < 1.0f && std::fabs(r.y) < 1.0f,
              "the edge of the light cone fits inside the shadow map");

        glm::vec4 b = vp * glm::vec4(pos - dir * 5.0f, 1.0f);
        check(b.w < 0.0f, "a point behind the lamp is not in its view");
    }
}

// ---- walking the real building ---------------------------------------------

// Walks in a straight line toward a target at walking speed, letting the
// collider push back each step. Returns where the visitor ends up.
Vec2 walk(const Collider& col, Vec2 from, Vec2 to, float radius) {
    Vec2 p = col.resolve(from, radius);
    const float step = cfg::kWalkSpeed / 60.0f;
    for (int i = 0; i < 2000; ++i) {
        const float dx = to.x - p.x, dy = to.y - p.y;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d < step) return col.resolve(to, radius);
        p = col.resolve(Vec2{p.x + dx / d * step, p.y + dy / d * step}, radius);
    }
    return p;
}

float dist(Vec2 a, Vec2 b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

void testWalking() {
    std::printf("Walking the gallery\n");

    const Plan plan = Plan::load(std::string(LUMEN_ASSET_DIR) + "plan.json");
    const Collider col(plan.walls, plan.wallThickness);
    const float r = cfg::kPlayerRadius;

    {   // through every doorway, both ways
        int passed = 0;
        for (const Segment& d : plan.doorways) {
            const Vec2 mid{(d.a.x + d.b.x) * 0.5f, (d.a.y + d.b.y) * 0.5f};
            const float nx = d.horizontal() ? 0.0f : 1.0f;
            const float ny = d.horizontal() ? 1.0f : 0.0f;
            const Vec2 sideA{mid.x - nx * 1.5f, mid.y - ny * 1.5f};
            const Vec2 sideB{mid.x + nx * 1.5f, mid.y + ny * 1.5f};
            if (dist(walk(col, sideA, sideB, r), sideB) < 0.05f &&
                dist(walk(col, sideB, sideA, r), sideA) < 0.05f)
                ++passed;
        }
        check(passed == static_cast<int>(plan.doorways.size()),
              "every doorway can be walked through in both directions");
    }
    {   // but not through the wall beside a doorway
        int blocked = 0;
        for (const RoomPlan& room : plan.rooms) {
            const Segment d = room.door.segment(room.rect);
            const float nx = d.horizontal() ? 0.0f : 1.0f;
            const float ny = d.horizontal() ? 1.0f : 0.0f;
            // 1 m past the end of the opening, along the wall
            const float ex = d.horizontal() ? 1.0f : 0.0f;
            const float ey = d.horizontal() ? 0.0f : 1.0f;
            const Vec2 beside{d.b.x + ex, d.b.y + ey};
            const Vec2 from{beside.x - nx * 1.2f, beside.y - ny * 1.2f};
            const Vec2 to{beside.x + nx * 1.2f, beside.y + ny * 1.2f};
            const Vec2 end = walk(col, from, to, r);
            // still on the starting side of the wall line
            const float side = (end.x - beside.x) * nx + (end.y - beside.y) * ny;
            if (side < 0.0f) ++blocked;
        }
        check(blocked == static_cast<int>(plan.rooms.size()),
              "the wall beside each doorway stops the visitor");
    }
    {   // all the way round the corridor, corners included
        bool lap = true;
        Vec2 at = plan.route.front();
        for (std::size_t i = 1; i <= plan.route.size(); ++i) {
            const Vec2 next = plan.route[i % plan.route.size()];
            at = walk(col, at, next, r);
            if (dist(at, next) > 0.05f) lap = false;
        }
        check(lap, "a full lap of the corridor never gets stuck on a corner");
    }
    {   // an opening narrower than the visitor cannot be squeezed through
        auto gapWalls = [](float gap) {
            return std::vector<Segment>{{{0, 0}, {5.0f - gap * 0.5f, 0}},
                                        {{5.0f + gap * 0.5f, 0}, {10, 0}}};
        };
        const float needed = 2.0f * (r + 0.225f);
        const Collider wide(gapWalls(needed + 0.3f), 0.45f);
        const Collider narrow(gapWalls(needed - 0.3f), 0.45f);
        check(walk(wide, {5, -2}, {5, 2}, r).y > 1.9f, "a gap wider than the visitor lets them through");
        check(walk(narrow, {5, -2}, {5, 2}, r).y < 0.0f, "a gap narrower than the visitor does not");
    }
}

// ---- lintels, vault ends and frames ----------------------------------------

void testBuildingDetail() {
    std::printf("Lintels, vault ends and frames\n");

    const Plan plan = Plan::load(std::string(LUMEN_ASSET_DIR) + "plan.json");

    {   // a lintel over every doorway, with an underside facing down at door height
        Plan doorsOnly = plan;
        doorsOnly.walls.clear();
        const gfx::MeshData m = build::extrudeWalls(doorsOnly);
        int undersides = 0;
        for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
            const glm::vec3 a = m.vertices[m.indices[i]].position;
            const glm::vec3 b = m.vertices[m.indices[i + 1]].position;
            const glm::vec3 c = m.vertices[m.indices[i + 2]].position;
            const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
            if (n.y < -0.99f && std::fabs(a.y - cfg::kDoorHeight) < 1e-4f) ++undersides;
        }
        check(undersides == 2 * static_cast<int>(plan.doorways.size() + plan.nicheOpenings.size()),
              "every doorway and niche has a lintel underside facing down at door height");
    }
    {   // the vault, ends included, faces into the room
        const RoomPlan& room = plan.rooms.front();
        const gfx::MeshData v = build::buildRoomVaults(room, plan.wallHeight, plan.vaultApex);
        const Vec2 c = room.rect.centre();
        bool allInward = true;
        int ends = 0;
        for (std::size_t i = 0; i + 2 < v.indices.size(); i += 3) {
            const glm::vec3 a = v.vertices[v.indices[i]].position;
            const glm::vec3 b = v.vertices[v.indices[i + 1]].position;
            const glm::vec3 d = v.vertices[v.indices[i + 2]].position;
            const glm::vec3 n = glm::normalize(glm::cross(b - a, d - a));
            if (std::fabs(n.y) < 0.01f) ++ends;   // the end walls are vertical
            // look from below the crown of whichever vault this face belongs to
            const glm::vec3 mid = (a + b + d) / 3.0f;
            const float crownX = (mid.x < c.x) ? room.vaults[0].a.x : room.vaults[1].a.x;
            const glm::vec3 eye(crownX, 3.0f, worldZ(c.y));
            if (glm::dot(n, eye - mid) <= 0.0f) allInward = false;
        }
        check(ends > 0, "the vault has end walls");
        check(allInward, "every vault face, ends included, faces into the room");

        float minY = 1e9f, maxY = -1e9f;
        for (const gfx::Vertex& vx : v.vertices) {
            minY = std::min(minY, -vx.position.z);
            maxY = std::max(maxY, -vx.position.z);
        }
        checkNear(minY, room.rect.y0 + plan.wallThickness * 0.5f, 1e-3f,
                  "the vault reaches the inside face of one end wall");
        checkNear(maxY, room.rect.y1 - plan.wallThickness * 0.5f, 1e-3f,
                  "and the other");
    }
    {   // the frame moulding faces out of the wall and wraps the canvas
        HangPoint hang;
        hang.span = Segment{{0, 0}, {4, 0}};
        hang.inwardNormal = Vec2{0, 1};   // room is to the north, world -Z
        const build::FramedWork w = build::buildFramedWork(hang, 1.4f, 1.0f);
        const glm::vec3 out(0.0f, 0.0f, -1.0f);
        bool facesOut = true;
        float maxDepth = 0.0f;
        for (std::size_t i = 0; i + 2 < w.frame.indices.size(); i += 3) {
            const glm::vec3 a = w.frame.vertices[w.frame.indices[i]].position;
            const glm::vec3 b = w.frame.vertices[w.frame.indices[i + 1]].position;
            const glm::vec3 c = w.frame.vertices[w.frame.indices[i + 2]].position;
            const glm::vec3 n = glm::cross(b - a, c - a);
            if (glm::length(n) < 1e-9f) continue;
            if (glm::dot(glm::normalize(n), out) < -1e-3f) facesOut = false;
            maxDepth = std::max(maxDepth, glm::dot(a - w.centre, out));
        }
        check(w.frame.triangleCount() > 40, "the moulding is a swept curve, not four boxes");
        check(facesOut, "no frame face points into the wall");
        check(maxDepth > cfg::kFrameDepth * 0.9f, "the moulding stands out by about the frame depth");
    }
    {   // the ceilings face down, and the oculus is a real hole
        const gfx::MeshData c = build::buildCeilings(plan);
        bool allDown = c.triangleCount() > 0;
        for (std::size_t i = 0; i + 2 < c.indices.size(); i += 3) {
            const glm::vec3 a = c.vertices[c.indices[i]].position;
            const glm::vec3 n = glm::cross(c.vertices[c.indices[i + 1]].position - a,
                                           c.vertices[c.indices[i + 2]].position - a);
            if (n.y >= 0.0f) allDown = false;
        }
        check(allDown, "every corridor ceiling face points down");

        const gfx::MeshData o = build::buildOculusCeiling(plan, cfg::kOculusRim, 64);
        bool flatDown = true, drumIn = true, coversHole = false;
        float area = 0.0f;
        const glm::vec3 centre(worldX(plan.oculusCentre.x), 0.0f, worldZ(plan.oculusCentre.y));
        for (std::size_t i = 0; i + 2 < o.indices.size(); i += 3) {
            const glm::vec3 a = o.vertices[o.indices[i]].position;
            const glm::vec3 b = o.vertices[o.indices[i + 1]].position;
            const glm::vec3 cc = o.vertices[o.indices[i + 2]].position;
            const glm::vec3 n = glm::cross(b - a, cc - a);
            const glm::vec3 mid = (a + b + cc) / 3.0f;
            if (std::fabs(a.y - plan.wallHeight) < 1e-4f && std::fabs(b.y - a.y) < 1e-4f &&
                std::fabs(cc.y - a.y) < 1e-4f) {
                if (n.y >= 0.0f) flatDown = false;
                area += 0.5f * glm::length(n);
                const glm::vec3 d = mid - glm::vec3(centre.x, mid.y, centre.z);
                if (glm::length(d) < plan.oculusRadius * 0.95f) coversHole = true;
            } else {
                // a drum face: its normal points back towards the centre line
                const glm::vec3 toAxis(centre.x - mid.x, 0.0f, centre.z - mid.z);
                if (glm::dot(n, toAxis) <= 0.0f) drumIn = false;
            }
        }
        const float wantArea = plan.atriumRect.width() * plan.atriumRect.height() -
                               3.14159265f * plan.oculusRadius * plan.oculusRadius;
        check(flatDown, "the atrium ceiling faces down");
        check(!coversHole, "nothing covers the oculus");
        checkNear(area, wantArea, wantArea * 0.01f, "the ceiling covers the atrium less the oculus");
        check(drumIn, "the drum round the oculus faces inward");
    }
}

// ---- statues ----------------------------------------------------------------

// Writes bytes to a temporary file.
std::string writeTempBytes(const std::string& name, const std::vector<unsigned char>& bytes) {
    const std::string path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),
                                                static_cast<std::streamsize>(bytes.size()));
    return path;
}

// A one-triangle glTF: positions, texture coordinates and 16 bit indices.
// The node moves it by (1, 2, 3) and scales x by sx.
std::string writeTriangleGltf(const std::string& name, float sx) {
    std::vector<unsigned char> bin;
    auto put = [&](const void* p, std::size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        bin.insert(bin.end(), b, b + n);
    };
    const float pos[9] = {0, 0, 0,  1, 0, 0,  0, 1, 0};
    const float uv[6]  = {0, 0,  1, 0,  0, 1};
    const unsigned short idx[3] = {0, 1, 2};
    put(pos, sizeof(pos));
    put(uv, sizeof(uv));
    put(idx, sizeof(idx));
    writeTempBytes(name + ".bin", bin);

    const std::string gltf = std::string(R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"translation": [1, 2, 3], "scale": [)") + std::to_string(sx) + R"(, 1, 1], "mesh": 0}],
      "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "indices": 2, "material": 0}]}],
      "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
      "textures": [{"source": 0}],
      "images": [{"uri": "stone.png"}],
      "buffers": [{"uri": ")" + name + R"(.bin", "byteLength": 66}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0,  "byteLength": 36},
        {"buffer": 0, "byteOffset": 36, "byteLength": 24},
        {"buffer": 0, "byteOffset": 60, "byteLength": 6}],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
        {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2"},
        {"bufferView": 2, "componentType": 5123, "count": 3, "type": "SCALAR"}]
    })";
    return writeTemp(name + ".gltf", gltf);
}

bool modelFails(const std::string& path, const std::string& expectInMessage) {
    try {
        model::load(path);
    } catch (const std::exception& e) {
        return std::string(e.what()).find(expectInMessage) != std::string::npos;
    }
    return false;
}

void testModels() {
    std::printf("Loading statues (glTF)\n");

    {   const model::Model m = model::load(writeTriangleGltf("lumen_tri", 1.0f));
        check(m.parts.size() == 1 && m.triangleCount() == 1, "a one-triangle file gives one triangle");
        const gfx::MeshData& mesh = m.parts[0].mesh;
        const glm::vec3 b = mesh.vertices[1].position;
        checkNear(b.x, 2.0f, 1e-5f, "the node's translation is applied");
        checkNear(b.y, 2.0f, 1e-5f, "on every axis");
        checkNear(mesh.vertices[0].normal.z, 1.0f, 1e-5f, "missing normals are worked out from the triangle");
        checkNear(mesh.vertices[2].uv.y, 0.0f, 1e-5f, "texture v is flipped to match our flipped images");
        check(m.parts[0].albedoPath.find("stone.png") != std::string::npos &&
              m.parts[0].albedoPath.find(std::filesystem::temp_directory_path().filename().string()) !=
                  std::string::npos,
              "texture paths are found next to the model file");
    }
    {   // mirrored by the node: the triangle must still face the same way as its normal
        const model::Model m = model::load(writeTriangleGltf("lumen_mirror", -1.0f));
        const gfx::MeshData& mesh = m.parts[0].mesh;
        const glm::vec3 a = mesh.vertices[mesh.indices[0]].position;
        const glm::vec3 n = glm::cross(mesh.vertices[mesh.indices[1]].position - a,
                                       mesh.vertices[mesh.indices[2]].position - a);
        check(glm::dot(n, mesh.vertices[0].normal) > 0.0f, "a mirroring node does not turn faces inside out");
    }
    {   const model::Model m = model::load(writeTriangleGltf("lumen_fit", 1.0f));
        const glm::mat4 f = model::fitToHeight(m, 5.0f);
        float lo = 1e9f, hi = -1e9f;
        for (const gfx::Vertex& v : m.parts[0].mesh.vertices) {
            const float y = (f * glm::vec4(v.position, 1.0f)).y;
            lo = std::min(lo, y);
            hi = std::max(hi, y);
        }
        checkNear(lo, 0.0f, 1e-4f, "fitToHeight stands the model on the floor");
        checkNear(hi, 5.0f, 1e-4f, "at the height asked for");
    }
    {   check(modelFails("no/such/model.gltf", "Could not load model"), "a missing model gives a clear message");
        const std::string bad = writeTemp("lumen_bad_model.gltf", "{ \"meshes\": [ ");
        check(modelFails(bad, "lumen_bad_model.gltf"), "a broken model names the file");
        const std::string empty = writeTemp("lumen_empty_model.gltf", "{ \"asset\": {\"version\": \"2.0\"} }");
        check(modelFails(empty, "no triangles"), "a model with no triangles says so");
    }
    {   // a real scan: the Discobolus is upright and the right size once fitted
        const model::Model m =
            model::load(std::string(LUMEN_ASSET_DIR) + "models/discobolus_the_discus_thrower/scene.gltf");
        check(m.triangleCount() == 201698, "the Discobolus scan has all its triangles");
        check(m.size().y > m.size().x && m.size().y > m.size().z, "it stands upright, y is up");
    }
}

void testSculpture() {
    std::printf("Placing statues and reliefs\n");

    const Plan plan = Plan::load(std::string(LUMEN_ASSET_DIR) + "plan.json");
    const sculpture::Catalogue cat = sculpture::load(std::string(LUMEN_ASSET_DIR) + "sculpture.json");
    check(cat.statues.size() == 5, "five statues in the catalogue");
    check(cat.reliefs.size() == 5, "five reliefs in the catalogue");
    {   bool credited = true, filesThere = true, placed = true;
        for (const auto& s : cat.statues) {
            if (s.credit.empty()) credited = false;
            if (!std::filesystem::exists(std::string(LUMEN_ASSET_DIR) + s.model)) filesThere = false;
            try { sculpture::spotFor(plan, s.place); } catch (const std::exception&) { placed = false; }
        }
        for (const auto& r : cat.reliefs) {
            if (r.credit.empty()) credited = false;
            if (!std::filesystem::exists(std::string(LUMEN_ASSET_DIR) + r.image)) filesThere = false;
        }
        check(credited, "every statue and relief has a credit");
        check(filesThere, "every model and image file is in assets/");
        check(placed, "every statue has a real place");
    }
    {   // each niche faces back across the corridor
        bool intoCorridor = true;
        for (int n = 0; n < static_cast<int>(plan.niches.size()); ++n) {
            const sculpture::Spot s = sculpture::spotFor(plan, "niche " + std::to_string(n));
            const Vec2 ahead{s.at.x + s.facing.x * 2.0f, s.at.y + s.facing.y * 2.0f};
            if (!plan.corridorOuter.contains(ahead) || plan.corridorInner.contains(ahead)) intoCorridor = false;
        }
        check(intoCorridor, "every niche statue faces into the corridor");
    }
    {   const sculpture::Spot s = sculpture::spotFor(plan, "atrium");
        checkNear(s.at.x, plan.oculusCentre.x, 1e-5f, "the atrium statue stands under the oculus");
        check(s.facing.x < -0.9f, "and faces the way in, to the west");
        bool threw = false;
        try { sculpture::spotFor(plan, "niche 9"); } catch (const std::exception&) { threw = true; }
        check(threw, "an unknown place is refused");
    }
    {   // a scan's front (+Z) turns to face the visitor
        const sculpture::Spot s = sculpture::spotFor(plan, "niche 2");   // faces east
        const glm::mat4 t = sculpture::statueTransform(s, 0.5f, 0.0f);
        const glm::vec3 front = glm::vec3(t * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
        checkNear(front.x, 1.0f, 1e-4f, "the front turns to face east");
        const glm::vec3 base = glm::vec3(t * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        checkNear(base.y, 0.5f, 1e-5f, "and stands on top of the plinth");
    }
    {   // reliefs sit on the atrium wall and face the corridor
        bool onWall = true, facesCorridor = true, faceOut = true;
        for (const auto& r : cat.reliefs) {
            const sculpture::WallSpot w = sculpture::wallSpotFor(plan, r.wall, r.at, r.centreHeight);
            const Vec2 out{w.centre.x + w.normal.x * 1.0f, -(w.centre.z + w.normal.z * 1.0f)};
            if (!plan.corridorOuter.contains(out) || plan.atriumRect.contains(out)) facesCorridor = false;
            const Vec2 c{w.centre.x, -w.centre.z};
            const Rect& a = plan.atriumRect;
            const float half = plan.wallThickness * 0.5f;
            const bool onFace = std::fabs(c.y - (a.y1 + half)) < 1e-4f || std::fabs(c.y - (a.y0 - half)) < 1e-4f ||
                                std::fabs(c.x - (a.x1 + half)) < 1e-4f || std::fabs(c.x - (a.x0 - half)) < 1e-4f;
            if (!onFace) onWall = false;
            const sculpture::Panel p = sculpture::buildPanel(w, r.width, r.width);
            const glm::vec3 n = p.face.vertices[0].normal;
            if (glm::dot(n, w.normal) < 0.99f) faceOut = false;
        }
        check(onWall, "every relief is on an outer face of the atrium wall");
        check(facesCorridor, "and faces into the corridor");
        check(faceOut, "the carved image faces away from the wall");
    }
    {   // nobody walks through the Apollo's plinth
        std::vector<Segment> solid = plan.walls;
        for (const Segment& s : sculpture::plinthOutlines(plan, cat)) solid.push_back(s);
        const Collider col(solid, plan.wallThickness);
        const Vec2 end = walk(col, Vec2{16.0f, plan.oculusCentre.y}, plan.oculusCentre, cfg::kPlayerRadius);
        check(end.x < plan.oculusCentre.x - 0.8f, "the visitor stops at the Apollo's plinth");
        check(end.x > plan.oculusCentre.x - 1.6f, "but can walk right up to it");
    }
}

// ---- the torch bearer -------------------------------------------------------

// A two-bone arm: "root" at the origin and "tip" 1 m above it. The triangle's
// top corner follows the tip; the clip "bend" turns the tip 90 degrees about z.
std::string writeArmGltf(const std::string& name) {
    std::vector<unsigned char> bin;
    auto put = [&](const void* p, std::size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        bin.insert(bin.end(), b, b + n);
    };
    const float pos[9] = {0, 0, 0,  1, 0, 0,  0, 2, 0};
    const unsigned short joints[12] = {0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0};
    const float weights[12] = {1, 0, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0};
    const unsigned short idx[4] = {0, 1, 2, 0};   // the last one pads to 4 bytes
    const float ibm[32] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1,
                           1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, -1, 0, 1};
    const float times[2] = {0, 1};
    const float s = 0.70710678f;
    const float turns[8] = {0, 0, 0, 1,  0, 0, s, s};
    put(pos, sizeof(pos));          //   0
    put(joints, sizeof(joints));    //  36
    put(weights, sizeof(weights));  //  60
    put(idx, sizeof(idx));          // 108
    put(ibm, sizeof(ibm));          // 116
    put(times, sizeof(times));      // 244
    put(turns, sizeof(turns));      // 252, ends at 284
    writeTempBytes(name + ".bin", bin);

    return writeTemp(name + ".gltf", std::string(R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0, 2]}],
      "nodes": [
        {"name": "root", "children": [1]},
        {"name": "tip", "translation": [0, 1, 0]},
        {"name": "arm", "mesh": 0, "skin": 0}],
      "skins": [{"joints": [0, 1], "inverseBindMatrices": 4}],
      "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "JOINTS_0": 1, "WEIGHTS_0": 2}, "indices": 3}]}],
      "animations": [{"name": "bend",
        "samplers": [{"input": 5, "output": 6, "interpolation": "LINEAR"}],
        "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}}]}],
      "buffers": [{"uri": ")") + name + R"(.bin", "byteLength": 284}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0,   "byteLength": 36},
        {"buffer": 0, "byteOffset": 36,  "byteLength": 24},
        {"buffer": 0, "byteOffset": 60,  "byteLength": 48},
        {"buffer": 0, "byteOffset": 108, "byteLength": 6},
        {"buffer": 0, "byteOffset": 116, "byteLength": 128},
        {"buffer": 0, "byteOffset": 244, "byteLength": 8},
        {"buffer": 0, "byteOffset": 252, "byteLength": 32}],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
        {"bufferView": 1, "componentType": 5123, "count": 3, "type": "VEC4"},
        {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4"},
        {"bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR"},
        {"bufferView": 4, "componentType": 5126, "count": 2, "type": "MAT4"},
        {"bufferView": 5, "componentType": 5126, "count": 2, "type": "SCALAR"},
        {"bufferView": 6, "componentType": 5126, "count": 2, "type": "VEC4"}]
    })");
}

// Where a skinned vertex ends up, worked out on the CPU the way the shader does it.
glm::vec3 skinned(const gfx::SkinnedVertex& v, const std::vector<glm::mat4>& joints) {
    glm::mat4 m(0.0f);
    for (int c = 0; c < 4; ++c) m += joints[v.joints[c]] * v.weights[c];
    return glm::vec3(m * glm::vec4(v.position, 1.0f));
}

void testSkeleton() {
    std::printf("Bones and animation\n");

    const model::SkinnedModel arm = model::loadSkinned(writeArmGltf("lumen_arm"));
    const skel::Skeleton& sk = arm.skeleton;
    check(sk.joints.size() == 2 && arm.triangleCount() == 1, "the arm has two joints and one triangle");
    check(arm.clips.size() == 1 && arm.clips[0].name == "bend", "its clip is loaded by name");
    checkNear(arm.clips[0].duration, 1.0f, 1e-5f, "the clip lasts as long as its last key");
    checkNear(arm.boundsMax.y, 2.0f, 1e-5f, "in the rest pose the arm is 2 m tall");

    const gfx::SkinnedVertex& top = arm.parts[0].mesh.vertices[2];
    const std::vector<int> binding = skel::bindClip(arm.clips[0], sk);
    check(binding.size() == 1 && binding[0] == sk.find("tip"), "the track finds its bone by name");

    auto at = [&](float t, bool loop) {
        skel::Pose p = skel::restPose(sk);
        skel::sample(arm.clips[0], binding, t, loop, p);
        return skinned(top, skel::jointMatrices(sk, skel::globals(sk, p)));
    };
    {   const glm::vec3 rest = skinned(top, skel::jointMatrices(sk, skel::globals(sk, skel::restPose(sk))));
        checkNear(rest.y, 2.0f, 1e-5f, "in the rest pose the skin is where it was modelled");
        const glm::vec3 bent = at(1.0f, false);
        checkNear(bent.x, -1.0f, 1e-4f, "bent 90 degrees, the top swings round the elbow");
        checkNear(bent.y, 1.0f, 1e-4f, "to the elbow's height");
        const glm::vec3 half = at(0.5f, false);
        checkNear(half.x, -0.70710678f, 1e-4f, "half way, it has turned 45 degrees (slerp)");
        checkNear(half.y, 1.70710678f, 1e-4f, "and keeps its length");
        const glm::vec3 wrapped = at(1.25f, true);
        const glm::vec3 early = at(0.25f, true);
        checkNear(wrapped.x, early.x, 1e-4f, "a looping clip wraps round");
        checkNear(at(3.0f, false).x, -1.0f, 1e-4f, "a clip that does not loop holds its last frame");
    }
    {   // blending half way between rest and fully bent gives the 45 degree pose
        skel::Pose bent = skel::restPose(sk);
        skel::sample(arm.clips[0], binding, 1.0f, false, bent);
        const skel::Pose mid = skel::blend(skel::restPose(sk), bent, 0.5f);
        const glm::vec3 p = skinned(top, skel::jointMatrices(sk, skel::globals(sk, mid)));
        checkNear(p.x, -0.70710678f, 1e-4f, "blending two poses turns smoothly between them");
    }
    {   // a walk that travels is held on the spot, keeping its bob
        skel::Clip walk;
        skel::Track tr;
        tr.node = "root";
        tr.path = skel::Path::Translation;
        tr.times = {0.0f, 1.0f};
        tr.values = {glm::vec4(0.0f), glm::vec4(2.0f, 0.1f, 3.0f, 0.0f)};
        walk.tracks.push_back(tr);
        walk.duration = 1.0f;
        skel::keepInPlace(walk, sk);
        const glm::vec4 v = walk.tracks[0].values[1];
        check(std::fabs(v.x) < 1e-5f && std::fabs(v.z) < 1e-5f, "keepInPlace removes the travel");
        checkNear(v.y, 0.1f, 1e-5f, "but keeps the up and down");
    }
    {   // a clip from a bigger character of the same rig
        skel::Clip other;
        skel::Track hips, turn, grow, stray;
        hips.node = "root";  hips.path = skel::Path::Translation;
        hips.times = {0.0f, 1.0f};
        hips.values = {glm::vec4(0, 2, 0, 0), glm::vec4(0, 3, 0, 0)};
        turn.node = "tip";   turn.path = skel::Path::Rotation;
        turn.times = {0.0f};  turn.values = {glm::vec4(0, 0, 0, 1)};
        grow.node = "tip";   grow.path = skel::Path::Scale;
        grow.times = {0.0f};  grow.values = {glm::vec4(2, 2, 2, 0)};
        stray.node = "tail"; stray.path = skel::Path::Rotation;
        stray.times = {0.0f}; stray.values = {glm::vec4(0, 0, 0, 1)};
        skel::Track still = hips, bob = hips;
        still.node = "tip"; still.values = {glm::vec4(0, 5, 0, 0), glm::vec4(0, 5, 0, 0)};
        other.tracks = {hips, turn, grow, stray, still};
        skel::Skeleton tall = sk;
        tall.nodes[static_cast<std::size_t>(tall.find("root"))].t = glm::vec3(0, 1, 0);
        const int kept = skel::retarget(other, tall);
        check(kept == 2, "retarget keeps the hips and the turns, and drops scale, unknown bones "
                         "and bone lengths from the other character");
        skel::Clip bobbing;
        bob.node = "tip";
        bob.values = {glm::vec4(0, 1, 0, 0), glm::vec4(0, 1.2f, 0, 0)};
        bobbing.tracks = {hips, bob};
        check(skel::retarget(bobbing, tall) == 2, "a bone that really moves, like the hips bobbing, keeps its move");
        checkNear(other.tracks[0].values[1].y, 1.5f, 1e-5f, "the hips move in proportion to this character's height");
    }
}

void testIntro() {
    std::printf("The torch bearer's intro\n");

    const Plan plan = Plan::load(std::string(LUMEN_ASSET_DIR) + "plan.json");
    const sculpture::Catalogue cat = sculpture::load(std::string(LUMEN_ASSET_DIR) + "sculpture.json");
    std::vector<Segment> solid = plan.walls;
    for (const Segment& s : sculpture::plinthOutlines(plan, cat)) solid.push_back(s);
    const Collider col(solid, plan.wallThickness);
    const Vec2 visitor = plan.spawn();
    const float dt = 1.0f / 30.0f;

    Intro intro(plan, visitor);
    check(intro.stage() == Intro::Stage::Waiting, "it starts with the bearer waiting");
    check(intro.bearerHasTorch() && intro.visitorLocked(), "holding the only light, and you cannot walk yet");
    check(intro.handover() == 0.0f && intro.prompt().empty(), "no prompt yet");
    check(std::string(intro.clip()) == "idle_torch", "standing with the torch held up");
    check(intro.frozen(), "as stone, before it wakes");
    checkNear(intro.elevation(), cfg::kBearerPlinthHeight, 1e-5f, "on top of its plinth");
    {   bool clear = true;
        for (std::size_t i = 1; i < intro.path().size(); ++i)
            if (col.segmentBlocked(intro.path()[i - 1], intro.path()[i])) clear = false;
        check(clear, "the bearer's route goes through the doorway, not a wall or the plinth");
        check(plan.atriumRect.contains(intro.path().front()), "and starts in the atrium");
    }

    bool walked = false, wokeUp = false, steppedDown = false;
    float lastElevation = intro.elevation();
    bool neverRises = true;
    for (int i = 0; i < 30 * 60 && intro.stage() != Intro::Stage::Offering; ++i) {
        intro.update(dt, false);
        if (std::string(intro.clip()) == "walk") walked = true;
        if (intro.stage() == Intro::Stage::Approaching && !intro.frozen()) wokeUp = true;
        if (intro.elevation() > lastElevation + 1e-6f) neverRises = false;
        lastElevation = intro.elevation();
        if (intro.elevation() < 1e-6f) steppedDown = true;
    }
    check(wokeUp, "the statue comes alive");
    check(walked && steppedDown, "steps down off its plinth and walks out");
    check(neverRises, "the step down only ever goes down");
    check(intro.stage() == Intro::Stage::Offering, "and holds out the torch");
    checkNear(dist(intro.position(), visitor), cfg::kBearerStandOff, 0.05f, "at arm's length from you");
    check(std::string(intro.clip()) == "give", "playing the give clip");

    intro.update(dt, true);   // too early: not listened to yet
    check(intro.bearerHasTorch(), "E does nothing before the prompt shows");
    for (int i = 0; i < 60; ++i) intro.update(dt, false);
    check(!intro.prompt().empty(), "then the prompt shows");
    {   // facing you
        const float want = std::atan2(visitor.x - intro.position().x, visitor.y - intro.position().y) * 57.2957795f;
        const float diff = std::fabs(std::fmod(intro.headingDeg() - want + 540.0f, 360.0f) - 180.0f);
        check(diff < 2.0f, "the bearer turns to face you");
    }

    intro.update(dt, true);
    check(!intro.bearerHasTorch() && !intro.visitorLocked(), "E takes the lamp, and you can walk");
    for (int i = 0; i < 6; ++i) intro.update(dt, false);
    check(intro.handover() > 0.0f && intro.handover() < 1.0f, "the light moves into your hand over a moment");

    for (int i = 0; i < 30 * 60 && intro.stage() != Intro::Stage::Done; ++i) intro.update(dt, false);
    check(intro.stage() == Intro::Stage::Done, "the bearer goes back");
    check(dist(intro.position(), intro.path().front()) < 1e-3f, "to stand by the Apollo");
    checkNear(intro.elevation(), cfg::kBearerPlinthHeight, 1e-5f, "back up on its plinth");
    checkNear(intro.handover(), 1.0f, 1e-5f, "and the light is all yours");
    check(!intro.frozen(), "it is still alive for a moment");
    for (int i = 0; i < 60; ++i) intro.update(dt, false);
    check(intro.frozen(), "then turns back to stone");

    {   Intro waitIt(plan, visitor);
        for (int i = 0; i < 30 * 60 && waitIt.bearerHasTorch(); ++i) waitIt.update(dt, false);
        check(!waitIt.bearerHasTorch(), "if you never press E, it is handed over anyway");
    }
    {   Intro skipped(plan, visitor);
        skipped.skip();
        check(skipped.stage() == Intro::Stage::Done && skipped.handover() == 1.0f && !skipped.visitorLocked(),
              "skipping gives you the lamp at once");
    }
    {   BearerSettings s;
        std::string why;
        check(!loadBearerSettings("no/such/intro.json", s, why) && why.find("missing") != std::string::npos,
              "a missing intro.json says so");
        check(loadBearerSettings(std::string(LUMEN_ASSET_DIR) + "intro.json", s, why) && !s.character.empty(),
              "the real intro.json names a character");
    }
}

// ---- pop art (Gallery Three) ------------------------------------------

void testPopArt() {
    std::printf("Pop art\n");
    using popart::Grey;

    {   // Rec. 601 luminance
        const std::uint8_t px[] = {255, 255, 255,   0, 0, 0,   0, 255, 0,   255, 0, 0};
        const Grey g = popart::luminance(px, 4, 1, 3);
        checkNear(g.at(0, 0), 1.0f,   1e-5f, "white is 1");
        checkNear(g.at(1, 0), 0.0f,   1e-5f, "black is 0");
        checkNear(g.at(2, 0), 0.587f, 1e-3f, "pure green weighs 0.587");
        checkNear(g.at(3, 0), 0.299f, 1e-3f, "pure red weighs 0.299");
        checkNear(g.at(9, 0), g.at(3, 0), 1e-6f, "reads past the edge are clamped");
    }
    {   // histogram stretch: a dull ramp from 0.3 to 0.6 uses the whole range after
        Grey g(64, 1);
        for (int x = 0; x < 64; ++x) g.ref(x, 0) = 0.3f + 0.3f * static_cast<float>(x) / 63.0f;
        popart::stretch(g, 0.0f);
        checkNear(g.at(0, 0),  0.0f, 0.01f, "stretch: darkest becomes black");
        checkNear(g.at(63, 0), 1.0f, 0.01f, "stretch: brightest becomes white");
        bool rising = true;
        for (int x = 1; x < 64; ++x) rising = rising && g.at(x, 0) >= g.at(x - 1, 0);
        check(rising, "stretch keeps the order of the greys");
        Grey flat(8, 8, 0.4f);
        popart::stretch(flat);
        checkNear(flat.at(3, 3), 0.4f, 1e-6f, "a flat image is left alone");
    }

    const Colour a{10, 10, 10, 255}, b{200, 0, 0, 255}, c{250, 250, 250, 255};

    {   // posterise
        Grey g(3, 1);
        g.ref(0, 0) = 0.0f; g.ref(1, 0) = 0.5f; g.ref(2, 0) = 1.0f;
        const Canvas p = popart::posterise(g, {a, b, c}, 2);
        check(p.width() == 6 && p.height() == 2, "posterise: each pixel becomes a 2x2 block");
        check(p.get(0, 0) == a && p.get(1, 1) == a, "posterise: black is the first colour");
        check(p.get(2, 0) == b, "posterise: middle grey is the middle colour");
        check(p.get(5, 1) == c, "posterise: white is the last colour");
    }
    {   // Floyd-Steinberg keeps the average brightness
        for (float level : {0.0f, 0.25f, 0.5f, 1.0f}) {
            const Grey g(40, 40, level);
            const Canvas d = popart::floydSteinberg(g, a, c);
            const float lit = static_cast<float>(countSet(d, c)) / 1600.0f;
            checkNear(lit, level, 0.02f, "Floyd-Steinberg: share of light pixels matches the grey");
        }
    }
    {   // Bayer: halfway between two colours lights exactly half of every 4x4 tile
        const Grey g(32, 32, 0.5f);
        const Canvas d = popart::bayer(g, {a, c});
        check(countSet(d, c) == 512, "Bayer: a 50% grey lights exactly half the pixels");
        int tile = 0;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) tile += d.get(x, y) == c ? 1 : 0;
        check(tile == 8, "Bayer: and exactly 8 of the 16 in one tile");
        check(countSet(popart::bayer(Grey(8, 8, 0.0f), {a, c}), a) == 64, "Bayer: black stays black");
        check(countSet(popart::bayer(Grey(8, 8, 1.0f), {a, c}), c) == 64, "Bayer: white stays white");
    }
    {   // halftone: dots grow with brightness, and black has none
        const Colour dot{255, 220, 40, 255}, paper{20, 20, 80, 255};
        const int none   = countSet(popart::halftone(Grey(40, 40, 0.0f), 8, dot, paper), dot);
        const int dim    = countSet(popart::halftone(Grey(40, 40, 0.3f), 8, dot, paper), dot);
        const int bright = countSet(popart::halftone(Grey(40, 40, 0.9f), 8, dot, paper), dot);
        check(none == 0, "halftone: black has no dots");
        check(dim > 0 && bright > dim, "halftone: brighter cells have bigger dots");
        check(bright < 1600, "halftone: the dots do not cover all the paper");
    }
    {   // the 2x2 print stays the size its parts add up to
        Grey face(20, 24);
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 20; ++x) face.ref(x, y) = static_cast<float>(x + y) / 42.0f;
        const Canvas w = popart::warholGrid(face, "TEST", 2);
        check(w.width() == 2 * 40 + 3 * 16, "warhol grid: two prints and three gaps wide");
        check(w.height() == 2 * 48 + 3 * 16 + 60, "warhol grid: two prints, three gaps and the title tall");
    }
}

} // namespace

void testLabels() {
    std::printf("Museum labels\n");

    const std::string text = "Sold for 6.2 million dollars in 2024, then the buyer ate it.";
    const std::vector<std::string> rows = label::wrap(text, 200, 2);
    bool fits = true;
    std::string joined;
    for (const std::string& r : rows) {
        if (font::textWidth(r, 2) > 200) fits = false;
        joined += (joined.empty() ? "" : " ") + r;
    }
    check(rows.size() > 1, "a long caption wraps onto several lines");
    check(fits, "every wrapped line fits the width");
    check(joined == text, "wrapping keeps every word, in order");
    check(label::wrap("", 200, 2).empty(), "no text, no lines");

    label::Text t{"Comedian", {"Maurizio Cattelan, 2019", "Banana and duct tape"}, text, ""};
    const raster::Canvas shortCard = label::draw(label::Text{"Comedian", {}, "", ""});
    const raster::Canvas longCard  = label::draw(t);
    check(longCard.height() > shortCard.height(), "a card grows to fit its words");
    const glm::vec2 size = label::fit(longCard, 0.36f, 0.10f);
    check(size.y <= 0.10f + 1e-5f && size.x <= 0.36f + 1e-5f, "a card fits the space it is given");
    checkNear(size.x / size.y, static_cast<float>(longCard.width()) / longCard.height(), 1e-3f,
              "and keeps its shape");
}

int main() {
    std::printf("\nLUMEN tests\n===========\n\n");
    testBresenham();
    testCircle();
    testEllipse();
    testCurves();
    testClipping();
    testFills();
    testLineThick();
    testPolygons();
    testPolygonClipping();
    testPlanLoad();
    testImage();
    testJson();
    testPages();
    testTransforms();
    testTransforms3D();
    testAnimation();
    testGif();
    testAnimations();
    testWinding();
    testCollision();
    testReveal();
    testLamp();
    testWalking();
    testBuildingDetail();
    testModels();
    testSculpture();
    testLabels();
    testSkeleton();
    testIntro();
    testPopArt();

    std::printf("\n%d checks, %d failures\n\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
