// MapPages.cpp - the minimap around a visitor, the full floor plan, and the
// guided tour route through it. Owner: Charles.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Curves.h"
#include "PageKit.h"
#include "Polygon.h"

namespace lumen::demo {

// The guided tour route: extra points every 2.5 m along each straight, so the
// spline stays in the corridor, then a closed Catmull-Rom loop through them.
std::vector<curves::P2> tourPath(const Plan &plan) {
  std::vector<curves::P2> pts;
  const std::size_t n = plan.route.size();
  for (std::size_t i = 0; i < n; ++i) {
    const Vec2 a = plan.route[i], b = plan.route[(i + 1) % n];
    const float len =
        std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
    const int steps = std::max(1, static_cast<int>(len / 2.5f));
    for (int k = 0; k < steps; ++k) {
      const float t = static_cast<float>(k) / static_cast<float>(steps);
      pts.push_back(curves::P2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
    }
  }
  return curves::sampleCatmullRomLoop(pts, 6);
}

namespace {

// ---- page 8: the minimap ---------------------------------------------------

// The minimap as the game will show it, centred on a visitor.
void drawMinimap(Canvas &m, const Plan &plan, Vec2 at, float scale,
                 const std::string &currentRoom) {
  const int n = m.width();
  const int c = n / 2;
  auto toPx = [&](Vec2 p) {
    return Point{c + static_cast<int>(std::lround((p.x - at.x) * scale)),
                 c - static_cast<int>(std::lround((p.y - at.y) * scale))};
  };
  const Colour ground{20, 17, 13, 255}, room{44, 37, 24, 255},
      lit{86, 70, 36, 255};
  const Colour wall{240, 229, 198, 255}, boundary{255, 0, 255, 255};
  const float vmax = static_cast<float>(n - 2);
  m.clear(ground);

  // Rooms: clipped to the map with Sutherland-Hodgman, then scanline filled.
  auto fillRect = [&](const Rect &r, Colour col) {
    const Point a = toPx(Vec2{r.x0, r.y0}), b = toPx(Vec2{r.x1, r.y1});
    const poly::Polygon shape =
        poly::rectangle(static_cast<float>(a.x), static_cast<float>(a.y),
                        static_cast<float>(b.x), static_cast<float>(b.y));
    const poly::Polygon visible =
        poly::clipSutherlandHodgman(shape, 0.0f, 0.0f, vmax + 1, vmax + 1);
    if (!visible.empty())
      raster::fillPolygonScanline(m, poly::toPixels(visible), col);
  };
  for (const RoomPlan &r : plan.rooms)
    fillRect(r.rect, room);
  fillRect(plan.corridorOuter, room);
  fillRect(plan.corridorInner, ground);

  // The room the visitor is in: outline it on a scratch canvas, boundary fill
  // from the visitor, then copy the filled pixels across.
  for (const RoomPlan &r : plan.rooms) {
    if (r.id != currentRoom)
      continue;
    Canvas scratch(n, n);
    scratch.clear(Colour{0, 0, 0, 0});
    const Point a = toPx(Vec2{r.rect.x0, r.rect.y0}),
                b = toPx(Vec2{r.rect.x1, r.rect.y1});
    raster::polyline(scratch, {{a.x, a.y}, {b.x, a.y}, {b.x, b.y}, {a.x, b.y}},
                     1, boundary, true);
    raster::fillBoundary(scratch, c, c, lit, boundary);
    for (int y = 0; y < n; ++y)
      for (int x = 0; x < n; ++x)
        if (scratch.get(x, y) == lit)
          m.set(x, y, lit);
  }

  // Walls: clipped with Cohen-Sutherland, then thick Bresenham lines.
  for (const Segment &s : plan.walls) {
    const Point a = toPx(s.a), b = toPx(s.b);
    float x0 = static_cast<float>(a.x), y0 = static_cast<float>(a.y);
    float x1 = static_cast<float>(b.x), y1 = static_cast<float>(b.y);
    if (!raster::clipCohenSutherland(x0, y0, x1, y1, 1.0f, 1.0f, vmax, vmax))
      continue;
    raster::lineThick(m, static_cast<int>(std::lround(x0)),
                      static_cast<int>(std::lround(y0)),
                      static_cast<int>(std::lround(x1)),
                      static_cast<int>(std::lround(y1)), 3, wall);
  }

  // The tour route: a Catmull-Rom spline, clipped and drawn with Bresenham.
  const std::vector<curves::P2> path = tourPath(plan);
  for (std::size_t i = 0; i + 1 < path.size(); ++i) {
    const Point a = toPx(Vec2{path[i].x, path[i].y}),
                b = toPx(Vec2{path[i + 1].x, path[i + 1].y});
    float x0 = static_cast<float>(a.x), y0 = static_cast<float>(a.y);
    float x1 = static_cast<float>(b.x), y1 = static_cast<float>(b.y);
    if (!raster::clipCohenSutherland(x0, y0, x1, y1, 1.0f, 1.0f, vmax, vmax))
      continue;
    raster::lineBresenham(m, static_cast<int>(std::lround(x0)),
                          static_cast<int>(std::lround(y0)),
                          static_cast<int>(std::lround(x1)),
                          static_cast<int>(std::lround(y1)), kTeal);
  }

  // The atrium oculus and the lamp's light ahead of the visitor: midpoint
  // ellipses.
  const Point o = toPx(plan.oculusCentre);
  const int orr = static_cast<int>(std::lround(plan.oculusRadius * scale));
  raster::ellipseMidpoint(m, o.x, o.y, orr, orr, kGoldDim);
  const int reach = static_cast<int>(std::lround(4.0f * scale));
  raster::ellipseMidpoint(m, c, c - reach * 6 / 10, reach * 45 / 100, reach,
                          kGold);

  // Doors and the visitor: midpoint circles. North is up.
  for (const RoomPlan &r : plan.rooms) {
    const Point d = toPx(r.door.centre(r.rect));
    raster::circleFilled(m, d.x, d.y, 3, kCoral);
  }
  raster::circleFilled(m, c, c, 5, kGold);
  raster::circleMidpoint(m, c, c, 9, kGold);
  raster::lineThick(m, c, c, c, c - 18, 2, kGold);

  raster::polyline(m, {{0, 0}, {n - 1, 0}, {n - 1, n - 1}, {0, n - 1}}, 1,
                   kGoldDim, true);
}

} // namespace

void pageMinimap(Canvas &c, const Plan *plan, float /*time*/) {
  const int w = c.width(), h = c.height();
  panel(c, 30, 60, 520, h - 160, "THE MINIMAP, VISITOR IN GALLERY TWO");
  panel(c, 570, 60, w - 600, h - 160,
        "EVERY MARK, AND THE ALGORITHM THAT MADE IT");
  if (!plan) {
    caption(c, 60, 120, "NO PLAN LOADED");
    return;
  }

  Canvas map(460, 460);
  drawMinimap(map, *plan, Vec2{27.5f, 31.0f}, 9.0f, "R2");
  for (int y = 0; y < map.height(); ++y)
    for (int x = 0; x < map.width(); ++x)
      c.set(60 + x, 95 + y, map.get(x, y));

  const int x = 600;
  int y = 110;
  auto line = [&](Colour col, const char *what, const char *how) {
    legend(c, x, y, col, what);
    caption(c, x + 20, y + 15, how);
    y += 44;
  };
  line(kInk, "WALLS", "COHEN-SUTHERLAND CLIP, THEN THICK BRESENHAM LINES");
  line(Colour{86, 70, 36, 255}, "THE ROOM YOU ARE IN",
       "BOUNDARY FILL FROM THE VISITOR'S POSITION");
  line(Colour{44, 37, 24, 255}, "ROOMS AND CORRIDOR",
       "SUTHERLAND-HODGMAN CLIP, THEN SCANLINE FILL");
  line(kTeal, "GUIDED TOUR ROUTE", "CATMULL-ROM SPLINE, CLIPPED, BRESENHAM");
  line(kGold, "LAMP LIGHT ON THE FLOOR",
       "MIDPOINT ELLIPSE AHEAD OF THE VISITOR");
  line(kGoldDim, "ATRIUM OCULUS", "MIDPOINT ELLIPSE");
  line(kCoral, "DOORS", "MIDPOINT CIRCLE, FILLED");
  line(kGold, "THE VISITOR", "MIDPOINT CIRCLE AND A HEADING LINE");
  caption(c, x, y + 10, "THE MAP IS REDRAWN AROUND THE VISITOR", kGold);
  caption(c, x, y + 26, "WHEREVER THEY STAND IN THE GALLERY.", kGold);
}

// ---- page 9: the gallery ---------------------------------------------------

void pageGallery(Canvas &c, const Plan *plan, float time) {
  const int w = c.width(), h = c.height();
  panel(c, 30, 60, w - 60, h - 160,
        "THE GALLERY PLAN, DRAWN WITH EVERY ALGORITHM ABOVE");

  if (!plan) {
    caption(c, 60, 120, "NO PLAN LOADED");
    return;
  }

  // Fit the whole building into the panel.
  const float pw = plan->bounds.x1 - plan->bounds.x0;
  const float ph = plan->bounds.y1 - plan->bounds.y0;
  const float sx = (w - 200) / std::max(pw, 1.0f);
  const float sy = (h - 300) / std::max(ph, 1.0f);
  const float s = std::min(sx, sy);

  const int offX = (w - static_cast<int>(pw * s)) / 2;
  const int offY = 120;

  auto toPx = [&](Vec2 p) {
    return Point{
        offX + static_cast<int>(std::lround((p.x - plan->bounds.x0) * s)),
        offY + static_cast<int>(std::lround((plan->bounds.y1 - p.y) * s))};
  };

  // Room footprints: scanline fill.
  for (const RoomPlan &r : plan->rooms) {
    const Point a = toPx(Vec2{r.rect.x0, r.rect.y0});
    const Point b = toPx(Vec2{r.rect.x1, r.rect.y1});
    raster::fillPolygonScanline(
        c, {{a.x, a.y}, {b.x, a.y}, {b.x, b.y}, {a.x, b.y}},
        Colour{44, 37, 24, 255});
  }

  // The tour route: Catmull-Rom spline.
  {
    const std::vector<curves::P2> path = tourPath(*plan);
    std::vector<Point> px;
    for (const curves::P2 &p : path)
      px.push_back(toPx(Vec2{p.x, p.y}));
    raster::polyline(c, px, 2, kTeal, true);

    // A point placed by distance around the loop.
    const std::vector<float> len = curves::arcLengths(path);
    const curves::P2 q = curves::pointAtDistance(path, len, time * 4.0f);
    const Point qp = toPx(Vec2{q.x, q.y});
    raster::circleFilled(c, qp.x, qp.y, 5, kGold);
  }

  // Walls: Bresenham.
  for (const Segment &seg : plan->walls) {
    const Point a = toPx(seg.a);
    const Point b = toPx(seg.b);
    raster::lineThick(c, a.x, a.y, b.x, b.y, 2, kInk);
  }

  // The oculus: midpoint ellipse.
  {
    const Point o = toPx(plan->oculusCentre);
    const int r = static_cast<int>(std::lround(plan->oculusRadius * s));
    raster::ellipseMidpoint(c, o.x, o.y, r, r, kGoldDim);
  }

  // Doors: midpoint circle.
  for (const RoomPlan &r : plan->rooms) {
    const Point d = toPx(r.door.centre(r.rect));
    raster::circleMidpoint(c, d.x, d.y, 5, kCoral);
  }

  // Labels.
  for (const RoomPlan &r : plan->rooms) {
    const Point cc = toPx(r.rect.centre());
    font::drawTextCentred(c, cc.x, cc.y - 4, r.name, kDim, 1);
  }

  legend(c, 60, h - 170, kInk, "WALLS: THICK BRESENHAM LINES");
  legend(c, 60, h - 150, kCoral, "DOORS: MIDPOINT CIRCLE");
  legend(c, 60, h - 130, kGoldDim, "OCULUS: MIDPOINT ELLIPSE");
  legend(c, 60, h - 110, kTeal,
         "TOUR ROUTE: CATMULL-ROM SPLINE, BY ARC LENGTH");
}

} // namespace lumen::demo
