// tests/test_core.cpp - tests for the Checkpoint 1 algorithms. Owner: Priscillah.
// Run lumen_tests before you push.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/Curves.h"
#include "core/Demo.h"
#include "core/Image.h"
#include "core/Json.h"
#include "core/Plan.h"
#include "core/Polygon.h"
#include "core/Raster.h"

namespace {

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
        check(p.walls.size() == 45, "the real plan has 45 wall segments");
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
    check(demo::kPageCount == 10, "ten pages");

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

} // namespace

int main() {
    std::printf("\nLUMEN checkpoint 1 tests\n========================\n\n");
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

    std::printf("\n%d checks, %d failures\n\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
