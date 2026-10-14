// Components.cpp - the gallery's components drawn with our algorithms: a framed
// painting, the vault in section, and a plinth with a sculpture and the lamp.
// Owner: Timothy.

#include <cmath>
#include <vector>

#include "Curves.h"
#include "PageKit.h"

namespace lumen::demo {
namespace {

// ---- page 7: gallery components --------------------------------------------

// A Vera Molnar style painting: a grid of squares drawn with Bresenham lines,
// drifting more and more from left to right. Same idea as the placeholder art.
void molnarPainting(Canvas &c, int x0, int y0, int cols, int rows, int cell,
                    Colour ink) {
  unsigned seed = 12345u;
  auto rnd = [&seed]() { // small repeatable random numbers, -1 to 1
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 16383.5f - 1.0f;
  };
  const int size = cell - 6;
  for (int r = 0; r < rows; ++r) {
    for (int q = 0; q < cols; ++q) {
      const float drift =
          7.0f * static_cast<float>(q) / static_cast<float>(cols - 1);
      const int bx = x0 + q * cell, by = y0 + r * cell;
      Point p[4] = {
          {bx, by}, {bx + size, by}, {bx + size, by + size}, {bx, by + size}};
      for (Point &v : p) {
        v.x += static_cast<int>(std::lround(rnd() * drift));
        v.y += static_cast<int>(std::lround(rnd() * drift));
      }
      for (int k = 0; k < 4; ++k)
        raster::lineBresenham(c, p[k].x, p[k].y, p[(k + 1) % 4].x,
                              p[(k + 1) % 4].y, ink);
    }
  }
}

} // namespace

void pageComponents(Canvas &c, float /*time*/) {
  const int h = c.height();
  panel(c, 30, 60, 400, h - 160, "A FRAMED PAINTING");
  panel(c, 450, 60, 400, h - 160, "THE VAULT, IN SECTION");
  panel(c, 870, 60, 380, h - 160, "PLINTH, SCULPTURE AND LAMP");

  // ---- 1. the painting ---------------------------------------------------
  // The canvas, then four mitred frame pieces as polygons filled by scanline.
  raster::fillPolygonScanline(c,
                              {{120, 160}, {340, 160}, {340, 320}, {120, 320}},
                              Colour{30, 26, 34, 255});
  molnarPainting(c, 132, 172, 10, 7, 20, Colour{214, 170, 150, 255});

  const Colour lit{214, 176, 88, 255}, shade{150, 116, 50, 255};
  raster::fillPolygonScanline(
      c, {{98, 138}, {362, 138}, {340, 160}, {120, 160}}, lit); // top
  raster::fillPolygonScanline(c, {{98, 138}, {120, 160}, {120, 320}, {98, 342}},
                              lit); // left
  raster::fillPolygonScanline(
      c, {{120, 320}, {340, 320}, {362, 342}, {98, 342}}, shade); // bottom
  raster::fillPolygonScanline(
      c, {{362, 138}, {362, 342}, {340, 320}, {340, 160}}, shade); // right
  raster::polyline(c, {{98, 138}, {362, 138}, {362, 342}, {98, 342}}, 1,
                   kGoldDim, true);

  // The wall label, with its text in our bitmap font.
  raster::fillPolygonScanline(
      c, {{292, 352}, {362, 352}, {362, 376}, {292, 376}}, kInk);
  font::drawText(c, 298, 360, "WORK 01", Colour{40, 34, 26, 255}, 1);

  // The frame's cross section: a cubic Bezier, with its control polygon.
  caption(c, 60, 404, "FRAME CROSS SECTION: A CUBIC BEZIER");
  const float pw = 280.0f, ph = 110.0f;
  auto prof = [&](float u, float v) {
    return curves::P2{70.0f + u * pw, 560.0f - v * ph};
  };
  const std::vector<curves::P2> ctrl{prof(0.0f, 0.35f), prof(0.10f, 1.25f),
                                     prof(0.55f, 1.05f), prof(1.0f, 0.45f)};
  std::vector<Point> cp;
  for (const curves::P2 &p : ctrl)
    cp.push_back({static_cast<int>(p.x), static_cast<int>(p.y)});
  raster::polyline(c, cp, 1, Colour{90, 80, 62, 255});
  for (const Point &p : cp)
    raster::circleFilled(c, p.x, p.y, 3, kCoral);
  std::vector<Point> curve;
  curve.push_back(
      {static_cast<int>(prof(0, 0).x), static_cast<int>(prof(0, 0).y)});
  for (const curves::P2 &p : curves::sampleBezier(ctrl, 40))
    curve.push_back({static_cast<int>(std::lround(p.x)),
                     static_cast<int>(std::lround(p.y))});
  curve.push_back(
      {static_cast<int>(prof(1, 0).x), static_cast<int>(prof(1, 0).y)});
  raster::polyline(c, curve, 2, kGold);
  raster::lineBresenham(c, 60, 560, 370, 560, kRule);
  caption(c, 60, 572, "CANVAS EDGE     WALL (BASELINE)     OUTER EDGE");

  // ---- 2. the vault ------------------------------------------------------
  // Two walls, and a quadratic Bezier arch with a gap at the top: the light
  // slot.
  const int spring = 380, floorY = 560;
  raster::fillPolygonScanline(
      c, {{480, spring}, {500, spring}, {500, floorY}, {480, floorY}}, kInk);
  raster::fillPolygonScanline(
      c, {{800, spring}, {820, spring}, {820, floorY}, {800, floorY}}, kInk);
  raster::lineThick(c, 470, floorY, 830, floorY, 3, kDim);

  // A semicircle for comparison, as points from the circle equation.
  std::vector<Point> semi;
  for (int i = 0; i <= 40; ++i) {
    const float a = kPi + kPi * static_cast<float>(i) / 40.0f;
    semi.push_back(
        {650 + static_cast<int>(std::lround(150.0f * std::cos(a))),
         spring + static_cast<int>(std::lround(150.0f * std::sin(a)))});
  }
  raster::polyline(c, semi, 1, Colour{90, 80, 62, 255});

  const curves::P2 a0{500, static_cast<float>(spring)},
      a1{650, static_cast<float>(spring) - 220.0f},
      a2{800, static_cast<float>(spring)};
  std::vector<Point> left, right;
  for (int i = 0; i <= 40; ++i) {
    const float t = static_cast<float>(i) / 40.0f;
    const curves::P2 p = curves::bezierQuadratic(a0, a1, a2, t);
    const Point px{static_cast<int>(std::lround(p.x)),
                   static_cast<int>(std::lround(p.y))};
    if (t < 0.46f)
      left.push_back(px);
    else if (t > 0.54f)
      right.push_back(px);
  }
  raster::polyline(c, left, 3, kInk);
  raster::polyline(c, right, 3, kInk);

  // Light coming down through the slot, drawn with DDA.
  for (int i = -3; i <= 3; ++i)
    raster::lineDDA(c, 650 + i * 3, 272, 650 + i * 40, floorY - 4, kGoldDim);

  legend(c, 480, 420 - 300, kInk, "QUADRATIC BEZIER ARCH");
  legend(c, 480, 438 - 300, Colour{90, 80, 62, 255},
         "SEMICIRCLE, FOR COMPARISON");
  legend(c, 480, 456 - 300, kGoldDim, "LIGHT THROUGH THE SLOT (DDA)");
  caption(c, 480, 578, "FLATTER THAN A SEMICIRCLE, LIKE KAHN'S KIMBELL");

  // ---- 3. plinth, sculpture and lamp ---------------------------------------
  raster::lineThick(c, 890, floorY, 1230, floorY, 3, kDim);
  raster::fillPolygonScanline(
      c, {{1000, 440}, {1100, 440}, {1100, floorY}, {1000, floorY}},
      Colour{200, 194, 180, 255}); // plinth
  raster::fillPolygonScanline(
      c, {{1018, 362}, {1082, 362}, {1082, 440}, {1018, 440}},
      Colour{226, 222, 214, 255}); // bust
  raster::ellipseFilled(c, 1050, 362, 32, 8,
                        Colour{240, 236, 228, 255}); // its top
  raster::ellipseMidpoint(c, 1050, 362, 32, 8, kDim);

  // The lamp, its cone to the floor, and the ellipse of light it makes.
  raster::circleFilled(c, 930, 250, 7, kGold);
  raster::circleMidpoint(c, 930, 250, 11, kGoldDim);
  raster::lineBresenham(c, 930, 250, 1010, floorY, kGoldDim);
  raster::lineBresenham(c, 930, 250, 1215, floorY, kGoldDim);
  raster::ellipseMidpoint(c, 1112, floorY, 103, 12, kGold);

  caption(c, 890, 578, "RECTANGLES: SCANLINE.  TOP: MIDPOINT ELLIPSE.");
  caption(c, 890, 594, "LAMP: CIRCLE.  CONE: BRESENHAM.  POOL: ELLIPSE.");
}

} // namespace lumen::demo
