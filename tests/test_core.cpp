// tests/test_core.cpp - checks on the parts of LUMEN that have no OpenGL in them.
//
// These run on any machine, in about a second, with no GPU. Run them before you
// push: `ctest --test-dir build` or run the lumen_tests executable directly.
//
// The algorithms below are the ones the project is assessed on, so a regression
// here is a regression in the marks. If you change Raster.cpp, a test must fail
// or a test must be added.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/Collision.h"
#include "core/Config.h"
#include "core/Lighting.h"
#include "core/Plan.h"
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

const Colour kInk{255, 255, 255, 255};
const Colour kBg {0, 0, 0, 255};

int countSet(const Canvas& c, Colour col) {
    int n = 0;
    for (int y = 0; y < c.height(); ++y)
        for (int x = 0; x < c.width(); ++x)
            if (c.get(x, y) == col) ++n;
    return n;
}

// ---- Bresenham -------------------------------------------------------------

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
    {   // eight-way symmetry
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
    {   // a diagonal through a corner: the trivial-reject test must not fire
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

    {   // outside the room, nothing is lit: the gallery is unlit by default
        ls.update(Vec2{20, 20}, 0.1f);
        checkNear(ls.levelFor("R1"), 0.0f, 1e-4f, "corridor leaves the room dark");
        check(ls.currentRoomId().empty(), "no current room when outside");
    }
    {   // step inside and the vault fades up over kRevealFadeIn seconds
        for (int i = 0; i < 100; ++i) ls.update(Vec2{5, 5}, cfg::kRevealFadeIn / 100.0f);
        checkNear(ls.levelFor("R1"), cfg::kRevealLevel, 1e-3f, "reveal reaches the cap");
        check(ls.currentRoomId() == "R1", "current room is tracked");
    }
    {   // and it is a cap, not full brightness - the concept depends on this
        check(cfg::kRevealLevel < 1.0f, "the reveal never reaches full brightness");
        check(ls.levelFor("R1") <= cfg::kRevealLevel + 1e-4f, "level never exceeds the cap");
    }
    {   // halfway through the fade-in it should be near half, not snapped on
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
    {   // standing on the sill must not strobe: the margin keeps it off
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
}

} // namespace

int main() {
    std::printf("\nLUMEN core tests\n================\n\n");
    testBresenham();
    testCircle();
    testClipping();
    testFills();
    testCollision();
    testReveal();
    testLamp();

    std::printf("\n%d checks, %d failures\n\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
