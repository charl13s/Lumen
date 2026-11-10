#include "Demo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Curves.h"
#include "Font.h"
#include "PageKit.h"
#include "Polygon.h"

namespace lumen::demo {
namespace {

// ---- page 1: lines ---------------------------------------------------------

void pageLines(Canvas& c, float time) {
    const int w = c.width(), h = c.height();

    panel(c, 30, 60, w / 2 - 50, h - 160, "DDA  VS  BRESENHAM");
    panel(c, w / 2 + 10, 60, w / 2 - 40, h - 160, "ALL EIGHT OCTANTS");

    // Left: the same line drawn both ways, offset so both are visible.
    const int ox = 60, oy = 120;
    caption(c, ox, oy - 22, "SAME ENDPOINTS, BOTH ALGORITHMS");

    raster::lineDDA(c, ox, oy, ox + 300, oy + 90, kCoral);
    raster::lineBresenham(c, ox, oy + 40, ox + 300, oy + 130, kTeal);
    legend(c, ox, oy + 170, kCoral, "DDA: FLOAT ACCUMULATOR, ROUNDS EVERY STEP");
    legend(c, ox, oy + 190, kTeal, "BRESENHAM: INTEGER ERROR TERM, NO DIVISION");

    // A shallow and a steep case, where the two differ most.
    caption(c, ox, oy + 225, "SHALLOW AND STEEP CASES");
    raster::lineBresenham(c, ox, oy + 250, ox + 300, oy + 262, kGold);
    raster::lineBresenham(c, ox + 20, oy + 280, ox + 70, oy + 400, kGold);
    raster::lineBresenham(c, ox + 120, oy + 400, ox + 170, oy + 280, kGold);
    caption(c, ox, oy + 420, "NO GAPS ON THE STEEP RUNS: THE MINOR AXIS");
    caption(c, ox, oy + 436, "STEPS ONLY WHEN THE ERROR TERM SAYS SO");

    // Right: a fan through all eight octants, from one centre.
    const int cx = w / 2 + 10 + (w / 2 - 40) / 2;
    const int cy = 60 + (h - 160) / 2;
    const int r  = std::min(w / 2 - 40, h - 160) / 2 - 60;

    for (int i = 0; i < 16; ++i) {
        const float a = 2.0f * kPi * i / 16.0f + time * 0.15f;
        const int ex = cx + static_cast<int>(std::lround(std::cos(a) * r));
        const int ey = cy + static_cast<int>(std::lround(std::sin(a) * r));
        raster::lineBresenham(c, cx, cy, ex, ey, (i % 2) ? kGoldDim : kGold);
    }
    raster::circleFilled(c, cx, cy, 3, kInk);
    caption(c, cx - 150, cy + r + 40,
            "ONE IMPLEMENTATION HANDLES EVERY DIRECTION");
    caption(c, cx - 150, cy + r + 56,
            "NO ENDPOINT SWAPPING, NO STEEP SPECIAL CASE");
}

// ---- page 2: circles and ellipses -----------------------------------------

void pageCircles(Canvas& c, float time) {
    const int w = c.width(), h = c.height();

    panel(c, 30, 60, w / 2 - 50, h - 160, "MIDPOINT CIRCLE");
    panel(c, w / 2 + 10, 60, w / 2 - 40, h - 160, "MIDPOINT ELLIPSE");

    const int lcx = 30 + (w / 2 - 50) / 2;
    const int lcy = 60 + (h - 160) / 2 - 20;

    // Concentric circles, to show the eight way symmetry.
    for (int r = 30; r <= 150; r += 30)
        raster::circleMidpoint(c, lcx, lcy, r, (r % 60) ? kGold : kGoldDim);

    // Mark the eight symmetric points of one radius.
    const int mr = 90;
    const int pts[8][2] = {{1,1},{1,-1},{-1,1},{-1,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    for (int i = 0; i < 4; ++i) {
        const int dx = static_cast<int>(mr * 0.70f) * pts[i][0];
        const int dy = static_cast<int>(mr * 0.70f) * pts[i][1];
        raster::circleFilled(c, lcx + dx, lcy + dy, 3, kTeal);
        raster::circleFilled(c, lcx + dy, lcy + dx, 3, kTeal);
    }
    caption(c, 55, lcy + 190, "ONE OCTANT IS COMPUTED, SEVEN ARE MIRRORED");
    caption(c, 55, lcy + 206, "THE DECISION VARIABLE ASKS WHETHER THE");
    caption(c, 55, lcy + 222, "MIDPOINT BETWEEN TWO CANDIDATES IS INSIDE");

    // Right: nested ellipses of varying eccentricity, plus the region split.
    const int rcx = w / 2 + 10 + (w / 2 - 40) / 2;
    const int rcy = 60 + (h - 160) / 2 - 20;

    raster::ellipseMidpoint(c, rcx, rcy, 180, 60, kGoldDim);
    raster::ellipseMidpoint(c, rcx, rcy, 140, 90, kGold);
    raster::ellipseMidpoint(c, rcx, rcy, 90, 120, kGoldDim);

    // One more ellipse, with the two regions marked.
    const int br = 40 + static_cast<int>(25.0f * (1.0f + std::sin(time * 1.2f)));
    raster::ellipseMidpoint(c, rcx, rcy, 150, br, kTeal);

    caption(c, w / 2 + 35, rcy + 190, "FOUR WAY SYMMETRY ONLY, NOT EIGHT");
    caption(c, w / 2 + 35, rcy + 206, "REGION 1: X DRIVES WHILE THE SLOPE IS");
    caption(c, w / 2 + 35, rcy + 222, "SHALLOW.  REGION 2: Y DRIVES AFTER IT");
    caption(c, w / 2 + 35, rcy + 238, "PASSES -1.  ONE REGION ALONE LEAVES GAPS.");
    caption(c, w / 2 + 35, rcy + 262, "USED FOR THE ATRIUM OCULUS AND THE", kGold);
    caption(c, w / 2 + 35, rcy + 278, "LAMP POOL ON THE MINIMAP.", kGold);
}

// ---- page 3: fills ---------------------------------------------------------

void pageFills(Canvas& c, float /*time*/) {
    const int w = c.width(), h = c.height();

    panel(c, 30, 60, w / 2 - 50, h - 160, "SCANLINE POLYGON FILL");
    panel(c, w / 2 + 10, 60, w / 2 - 40, h - 160, "BOUNDARY FILL");

    // A concave shape, which needs the parity rule.
    const int ox = 70, oy = 120;
    const std::vector<Point> star{
        {ox + 150, oy},        {ox + 190, oy + 110}, {ox + 305, oy + 110},
        {ox + 212, oy + 180},  {ox + 248, oy + 290}, {ox + 150, oy + 225},
        {ox + 52,  oy + 290},  {ox + 88,  oy + 180}, {ox - 5,   oy + 110},
        {ox + 110, oy + 110},
    };
    raster::fillPolygonScanline(c, star, kGoldDim);
    raster::polyline(c, star, 1, kGold, true);

    // One scanline and where it crosses the edges.
    const int sy = oy + 150;
    raster::lineBresenham(c, ox - 14, sy, ox + 330, sy, kTeal);
    caption(c, ox - 14, sy - 20, "ONE SCANLINE", kTeal);

    caption(c, 55, oy + 330, "FOR EACH SCANLINE: FIND EVERY EDGE CROSSING,");
    caption(c, 55, oy + 346, "SORT THEM, FILL BETWEEN PAIRS. THE PARITY");
    caption(c, 55, oy + 362, "RULE IS WHAT KEEPS A CONCAVE SHAPE CORRECT.");
    caption(c, 55, oy + 386, "USED FOR THE ROOM FOOTPRINTS ON THE MINIMAP.", kGold);

    // Right: a bounded region filled from a seed.
    const int bx = w / 2 + 60, by = 120;
    const std::vector<Point> room{
        {bx, by}, {bx + 300, by}, {bx + 300, by + 120},
        {bx + 180, by + 120}, {bx + 180, by + 260}, {bx, by + 260},
    };
    raster::polyline(c, room, 2, kGold, true);
    raster::fillBoundary(c, bx + 60, by + 60, kCoral, kGold);
    raster::circleFilled(c, bx + 60, by + 60, 4, kInk);
    caption(c, bx + 72, by + 54, "SEED", kInk);

    caption(c, w / 2 + 35, by + 310, "FLOOD OUTWARD FROM A SEED, STOPPING AT THE");
    caption(c, w / 2 + 35, by + 326, "BOUNDARY COLOUR. ITERATIVE WITH AN EXPLICIT");
    caption(c, w / 2 + 35, by + 342, "STACK: THE RECURSIVE FORM OVERFLOWS ON A");
    caption(c, w / 2 + 35, by + 358, "REGION THE SIZE OF A ROOM.");
    caption(c, w / 2 + 35, by + 382, "USED TO HIGHLIGHT THE ROOM YOU ARE IN.", kGold);
}

// ---- page 4: clipping ------------------------------------------------------

void pageClipping(Canvas& c, float time) {
    const int w = c.width(), h = c.height();
    panel(c, 30, 60, w - 60, h - 160, "COHEN-SUTHERLAND LINE CLIPPING");

    const int vx0 = w / 2 - 190, vy0 = 160;
    const int vx1 = w / 2 + 190, vy1 = 420;

    // The clipping rectangle.
    const std::vector<Point> view{{vx0, vy0}, {vx1, vy0}, {vx1, vy1}, {vx0, vy1}};
    raster::fillPolygonScanline(c, view, Colour{34, 30, 22, 255});
    raster::polyline(c, view, 2, kGold, true);
    font::drawText(c, vx0 + 8, vy0 + 8, "VIEWPORT", kGoldDim, 1);

    // The nine outcode regions, labelled.
    const char* codes[3][3] = {
        {"1001", "1000", "1010"},
        {"0001", "0000", "0010"},
        {"0101", "0100", "0110"},
    };
    for (int r = 0; r < 3; ++r) {
        for (int q = 0; q < 3; ++q) {
            const int lx = (q == 0) ? vx0 - 110 : (q == 1) ? (vx0 + vx1) / 2 - 20 : vx1 + 40;
            const int ly = (r == 0) ? vy0 - 40 : (r == 1) ? (vy0 + vy1) / 2 : vy1 + 24;
            if (r == 1 && q == 1) continue;        // inside is drawn elsewhere
            font::drawText(c, lx, ly, codes[r][q], kDim, 1);
        }
    }

    // A long line through the window, clipped at both ends.
    const int cx = (vx0 + vx1) / 2, cy = (vy0 + vy1) / 2;
    const float a = time * 0.6f;
    const int R = 280;
    const int ax = cx + static_cast<int>(std::lround(std::cos(a) * R));
    const int ay = cy + static_cast<int>(std::lround(std::sin(a) * R * 0.75f));
    const int bx = cx - static_cast<int>(std::lround(std::cos(a) * R));
    const int by = cy - static_cast<int>(std::lround(std::sin(a) * R * 0.75f));

    // Whole line dim, clipped part bright.
    raster::lineBresenham(c, ax, ay, bx, by, Colour{70, 62, 48, 255});

    float fx0 = static_cast<float>(ax), fy0 = static_cast<float>(ay);
    float fx1 = static_cast<float>(bx), fy1 = static_cast<float>(by);
    const bool visible = raster::clipCohenSutherland(
        fx0, fy0, fx1, fy1,
        static_cast<float>(vx0), static_cast<float>(vy0),
        static_cast<float>(vx1), static_cast<float>(vy1));

    if (visible) {
        raster::lineThick(c, static_cast<int>(std::lround(fx0)),
                             static_cast<int>(std::lround(fy0)),
                             static_cast<int>(std::lround(fx1)),
                             static_cast<int>(std::lround(fy1)), 3, kTeal);
        raster::circleFilled(c, static_cast<int>(std::lround(fx0)),
                                static_cast<int>(std::lround(fy0)), 4, kCoral);
        raster::circleFilled(c, static_cast<int>(std::lround(fx1)),
                                static_cast<int>(std::lround(fy1)), 4, kCoral);
    }

    // Three fixed cases, one of each outcome.
    struct Case { int x0, y0, x1, y1; };
    const Case cases[] = {
        {vx0 + 40, vy0 + 40, vx1 - 40, vy1 - 40},        // wholly inside
        {vx0 - 150, vy0 + 60, vx1 + 150, vy0 + 90},      // crosses both sides
        {vx0 - 160, vy1 + 60, vx0 - 40, vy1 + 90},       // wholly outside
    };
    for (const Case& k : cases) {
        raster::lineBresenham(c, k.x0, k.y0, k.x1, k.y1, Colour{70, 62, 48, 255});
        float x0 = static_cast<float>(k.x0), y0 = static_cast<float>(k.y0);
        float x1 = static_cast<float>(k.x1), y1 = static_cast<float>(k.y1);
        if (raster::clipCohenSutherland(x0, y0, x1, y1,
                                        static_cast<float>(vx0), static_cast<float>(vy0),
                                        static_cast<float>(vx1), static_cast<float>(vy1)))
            raster::lineThick(c, static_cast<int>(std::lround(x0)),
                                 static_cast<int>(std::lround(y0)),
                                 static_cast<int>(std::lround(x1)),
                                 static_cast<int>(std::lround(y1)), 3, kBlue);
    }

    legend(c, 60, h - 150, Colour{70, 62, 48, 255}, "THE LINE AS GIVEN");
    legend(c, 60, h - 130, kTeal, "THE PORTION THAT SURVIVES CLIPPING");
    legend(c, 60, h - 110, kBlue, "FIXED CASES: INSIDE, CROSSING, REJECTED");
    caption(c, w / 2 + 40, h - 150,
            "FOUR BITS PER ENDPOINT. BOTH ZERO MEANS ACCEPT AT ONCE;");
    caption(c, w / 2 + 40, h - 130,
            "A SHARED BIT MEANS REJECT AT ONCE. ONLY WHAT IS LEFT");
    caption(c, w / 2 + 40, h - 110,
            "NEEDS AN INTERSECTION COMPUTED.");
    caption(c, w / 2 + 40, h - 86,
            "USED TO CLIP WALLS TO THE MINIMAP VIEWPORT.", kGold);
}

// ---- page 5: polygons ------------------------------------------------------

// Labels one polygon: vertex numbers, centroid, and its area and convexity.
void describePolygon(Canvas& c, const poly::Polygon& p, Colour fill, Colour edge,
                     int labelX, int labelY, const char* name) {
    raster::fillPolygonScanline(c, poly::toPixels(p), fill);
    raster::polyline(c, poly::toPixels(p), 2, edge, true);
    for (std::size_t i = 0; i < p.size(); ++i) {
        const int x = static_cast<int>(std::lround(p[i].x));
        const int y = static_cast<int>(std::lround(p[i].y));
        raster::circleFilled(c, x, y, 3, kCoral);
        font::drawText(c, x + 5, y - 10, std::to_string(i), kDim, 1);
    }
    const poly::PointF g = poly::centroid(p);
    raster::circleMidpoint(c, static_cast<int>(std::lround(g.x)),
                              static_cast<int>(std::lround(g.y)), 4, kGold);

    char buf[96];
    font::drawText(c, labelX, labelY, name, kInk, 1);
    std::snprintf(buf, sizeof(buf), "%zu VERTICES  AREA %.0f", p.size(), poly::area(p));
    font::drawText(c, labelX, labelY + 14, buf, kDim, 1);
    font::drawText(c, labelX, labelY + 28, poly::isConvex(p) ? "CONVEX" : "CONCAVE", kDim, 1);
}

void pagePolygons(Canvas& c, float /*time*/) {
    const int w = c.width(), h = c.height();
    panel(c, 30, 60, w / 2 - 50, h - 160, "POLYGON REPRESENTATION AND OPERATIONS");
    panel(c, w / 2 + 10, 60, w / 2 - 40, h - 160, "SUTHERLAND-HODGMAN POLYGON CLIPPING");

    // Left: three polygons, each an ordered list of vertices.
    const poly::Polygon pentagon = poly::ellipse({160, 200}, 70, 70, 5);
    const poly::Polygon starShape = poly::star({440, 210}, 85, 36, 5);
    const poly::Polygon room{{90, 360}, {300, 360}, {300, 420}, {200, 420}, {200, 490}, {90, 490}};

    describePolygon(c, pentagon, kPanel, kBlue, 100, 290, "CONVEX PENTAGON");
    describePolygon(c, starShape, kPanel, kTeal, 370, 310, "CONCAVE STAR");
    describePolygon(c, room, Colour{44, 37, 24, 255}, kGold, 320, 430, "L SHAPED ROOM");

    // Point in polygon: test dots around the star.
    const poly::PointF tests[] = {{440, 210}, {440, 150}, {500, 200}, {470, 260},
                                  {390, 250}, {440, 290}, {520, 150}, {360, 160}};
    for (const poly::PointF& t : tests) {
        const bool in = poly::contains(starShape, t);
        raster::circleFilled(c, static_cast<int>(t.x), static_cast<int>(t.y), 4, in ? kInk : kCoral);
    }
    legend(c, 60, 520, kInk, "WHITE DOT: INSIDE (EVEN-ODD RULE)");
    legend(c, 60, 538, kCoral, "RED DOT: OUTSIDE");
    caption(c, 60, 560, "GOLD RING: CENTROID.  AREA: SHOELACE FORMULA.");

    // Right: polygons clipped against a window, one edge at a time.
    const int vx0 = w / 2 + 130, vy0 = 170;
    const int vx1 = w - 150,     vy1 = 420;
    const poly::Polygon window = poly::rectangle(
        static_cast<float>(vx0), static_cast<float>(vy0),
        static_cast<float>(vx1), static_cast<float>(vy1));
    raster::fillPolygonScanline(c, poly::toPixels(window), Colour{34, 30, 22, 255});
    raster::polyline(c, poly::toPixels(window), 2, kGold, true);
    font::drawText(c, vx0 + 8, vy0 + 8, "WINDOW", kGoldDim, 1);

    const poly::Polygon shapes[] = {
        poly::star({static_cast<float>(vx1) - 20.0f, 330.0f}, 110.0f, 45.0f, 6),
        poly::Polygon{{static_cast<float>(vx0) - 70, 120}, {static_cast<float>(vx0) + 110, 140},
                      {static_cast<float>(vx0) + 60, 230}, {static_cast<float>(vx0) - 40, 260}},
    };
    char buf[96];
    int row = 455;
    for (const poly::Polygon& s : shapes) {
        raster::polyline(c, poly::toPixels(s), 1, Colour{90, 80, 62, 255}, true);
        const poly::Polygon clipped = poly::clipSutherlandHodgman(
            s, static_cast<float>(vx0), static_cast<float>(vy0),
            static_cast<float>(vx1), static_cast<float>(vy1));
        if (!clipped.empty()) {
            raster::fillPolygonScanline(c, poly::toPixels(clipped), Colour{40, 70, 62, 255});
            raster::polyline(c, poly::toPixels(clipped), 2, kTeal, true);
            for (const poly::PointF& p : clipped)
                raster::circleFilled(c, static_cast<int>(std::lround(p.x)),
                                        static_cast<int>(std::lround(p.y)), 2, kCoral);
        }
        std::snprintf(buf, sizeof(buf), "VERTICES IN: %zu   OUT: %zu", s.size(), clipped.size());
        caption(c, w / 2 + 40, row, buf);
        row += 18;
    }

    caption(c, w / 2 + 40, h - 150, "CLIP AGAINST LEFT, RIGHT, BOTTOM, TOP IN TURN.");
    caption(c, w / 2 + 40, h - 130, "EACH PASS KEEPS INSIDE POINTS AND ADDS ONE WHERE");
    caption(c, w / 2 + 40, h - 110, "AN EDGE CROSSES THE BOUNDARY.");
    caption(c, w / 2 + 40, h - 86, "USED TO CLIP ROOM SHAPES TO THE MINIMAP.", kGold);
}

// ---- page 6: curves --------------------------------------------------------

void pageCurves(Canvas& c, float time) {
    const int w = c.width(), h = c.height();

    panel(c, 30, 60, w / 2 - 50, h - 160, "BEZIER AND DE CASTELJAU");
    panel(c, w / 2 + 10, 60, w / 2 - 40, h - 160, "CATMULL-ROM SPLINE");

    // Left: a cubic Bezier with its control polygon, plus the de Casteljau
    // construction at one value of t.
    const curves::P2 p0{80.0f, 420.0f};
    const curves::P2 p1{150.0f, 150.0f};
    const curves::P2 p2{340.0f, 150.0f};
    const curves::P2 p3{430.0f, 420.0f};
    const std::vector<curves::P2> ctrl{p0, p1, p2, p3};

    // Control polygon.
    std::vector<Point> poly;
    for (const curves::P2& p : ctrl)
        poly.push_back(Point{static_cast<int>(p.x), static_cast<int>(p.y)});
    raster::polyline(c, poly, 1, Colour{80, 70, 54, 255});
    for (const Point& p : poly) raster::circleFilled(c, p.x, p.y, 4, kCoral);

    // The curve, sampled then joined with Bresenham runs.
    const std::vector<curves::P2> pts = curves::sampleBezier(ctrl, 64);
    std::vector<Point> px;
    for (const curves::P2& p : pts)
        px.push_back(Point{static_cast<int>(std::lround(p.x)),
                           static_cast<int>(std::lround(p.y))});
    raster::polyline(c, px, 3, kGold);

    // de Casteljau at one t: the intermediate lerps, drawn.
    const float t = 0.5f + 0.5f * std::sin(time * 0.8f);
    {
        std::vector<curves::P2> work = ctrl;
        Colour shades[3] = {kTeal, kBlue, kInk};
        for (int level = static_cast<int>(work.size()); level > 1; --level) {
            std::vector<Point> lvl;
            for (int i = 0; i + 1 < level; ++i) {
                work[i] = curves::P2{work[i].x + (work[i + 1].x - work[i].x) * t,
                                     work[i].y + (work[i + 1].y - work[i].y) * t};
                lvl.push_back(Point{static_cast<int>(std::lround(work[i].x)),
                                    static_cast<int>(std::lround(work[i].y))});
            }
            const int idx = 4 - level;
            if (idx >= 0 && idx < 3) {
                raster::polyline(c, lvl, 1, shades[idx]);
                for (const Point& q : lvl)
                    raster::circleFilled(c, q.x, q.y, 3, shades[idx]);
            }
        }
    }

    caption(c, 55, 470, "CONTROL POINTS IN CORAL, THE CURVE IN GOLD.");
    caption(c, 55, 486, "DE CASTELJAU BUILDS THE POINT BY REPEATED");
    caption(c, 55, 502, "LINEAR INTERPOLATION: THE THIN LINES ARE");
    caption(c, 55, 518, "THE INTERMEDIATE LEVELS AT THIS T.");
    caption(c, 55, 542, "A QUADRATIC BEZIER SHAPES EACH VAULT ARCH.", kGold);

    // Right: a Catmull-Rom loop through fixed waypoints.
    const int rx = w / 2 + 50;
    const std::vector<curves::P2> way{
        {static_cast<float>(rx + 60),  180.0f},
        {static_cast<float>(rx + 300), 150.0f},
        {static_cast<float>(rx + 360), 330.0f},
        {static_cast<float>(rx + 190), 430.0f},
        {static_cast<float>(rx + 40),  330.0f},
    };

    // Straight lines between the points, for comparison.
    std::vector<Point> straight;
    for (const curves::P2& p : way)
        straight.push_back(Point{static_cast<int>(p.x), static_cast<int>(p.y)});
    raster::polyline(c, straight, 1, Colour{80, 70, 54, 255}, true);

    const std::vector<curves::P2> loop = curves::sampleCatmullRomLoop(way, 24, 0.5f);
    std::vector<Point> lx;
    for (const curves::P2& p : loop)
        lx.push_back(Point{static_cast<int>(std::lround(p.x)),
                           static_cast<int>(std::lround(p.y))});
    raster::polyline(c, lx, 3, kTeal, true);

    for (const Point& p : straight) raster::circleFilled(c, p.x, p.y, 5, kCoral);

    // A point placed by distance along the curve, using the arc length table.
    {
        const std::vector<float> len = curves::arcLengths(loop);
        const curves::P2 q = curves::pointAtDistance(loop, len, time * 90.0f);
        raster::circleFilled(c, static_cast<int>(std::lround(q.x)),
                                static_cast<int>(std::lround(q.y)), 6, kGold);
    }

    caption(c, w / 2 + 35, 470, "THE SPLINE PASSES THROUGH EVERY WAYPOINT,");
    caption(c, w / 2 + 35, 486, "WHICH A BEZIER WOULD NOT. GREY SHOWS THE");
    caption(c, w / 2 + 35, 502, "STRAIGHT JOINS IT REPLACES.");
    caption(c, w / 2 + 35, 518, "THE GOLD DOT MOVES BY ARC LENGTH, SO ITS");
    caption(c, w / 2 + 35, 534, "SPEED IS CONSTANT ON LONG AND SHORT SPANS.");
    caption(c, w / 2 + 35, 558, "THIS IS THE GUIDED TOUR ROUTE.", kGold);
}

} // namespace

const char* pageTitle(Page page) {
    switch (page) {
        case Page::Lines:      return "1 / 10   LINE DRAWING";
        case Page::Circles:    return "2 / 10   CIRCLES AND ELLIPSES";
        case Page::Fills:      return "3 / 10   POLYGON FILLING";
        case Page::Clipping:   return "4 / 10   LINE CLIPPING";
        case Page::Polygons:   return "5 / 10   POLYGONS AND POLYGON CLIPPING";
        case Page::Curves:     return "6 / 10   CURVES AND SPLINES";
        case Page::Components: return "7 / 10   GALLERY COMPONENTS";
        case Page::Artwork:    return "8 / 10   GENERATED ARTWORK";
        case Page::Minimap:    return "9 / 10   THE MINIMAP";
        case Page::Gallery:    return "10 / 10   THE GALLERY PLAN";
        default:             return "";
    }
}

void renderPage(Canvas& canvas, Page page, const Plan* plan) {
    const float time = 2.5f;   // fixes the angles and positions used in the examples
    canvas.clear(kBg);

    const int w = canvas.width();
    const int h = canvas.height();

    // Header.
    font::drawText(canvas, 30, 22, "LUMEN", kGold, 2);
    font::drawText(canvas, 150, 26, "RASTERISATION ALGORITHMS, IMPLEMENTED IN C++",
                   kDim, 1);
    raster::lineBresenham(canvas, 30, 48, w - 30, 48, kRule);

    switch (page) {
        case Page::Lines:    pageLines(canvas, time); break;
        case Page::Circles:  pageCircles(canvas, time); break;
        case Page::Fills:    pageFills(canvas, time); break;
        case Page::Clipping: pageClipping(canvas, time); break;
        case Page::Polygons: pagePolygons(canvas, time); break;
        case Page::Curves:   pageCurves(canvas, time); break;
        case Page::Components: pageComponents(canvas, time); break;
        case Page::Artwork:  pageArtwork(canvas, time); break;
        case Page::Minimap:  pageMinimap(canvas, plan, time); break;
        case Page::Gallery:  pageGallery(canvas, plan, time); break;
        default: break;
    }

    // Footer.
    raster::lineBresenham(canvas, 30, h - 54, w - 30, h - 54, kRule);
    font::drawText(canvas, 30, h - 40, pageTitle(page), kGold, 1);
}

} // namespace lumen::demo
