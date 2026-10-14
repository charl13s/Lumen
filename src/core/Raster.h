// Raster.h - our own rasteriser. Owner: Priscillah.
//
// Draws into a plain pixel array with no OpenGL or libraries. Used by the
// minimap and the algorithm demo, and tested in tests/test_core.cpp.

#pragma once
#include <cstdint>
#include <vector>

namespace lumen::raster {

struct Colour {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
    bool operator==(const Colour& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const Colour& o) const { return !(*this == o); }
};

// A pixel buffer. (0, 0) is the top left.
class Canvas {
public:
    Canvas(int w, int h);

    int  width()  const { return w_; }
    int  height() const { return h_; }

    void   clear(Colour c);
    void   set(int x, int y, Colour c);        // bounds-checked, silently ignores outside
    Colour get(int x, int y) const;            // returns transparent outside
    bool   inside(int x, int y) const;

    const std::uint8_t* pixels() const { return px_.data(); }
    std::size_t         byteSize() const { return px_.size(); }

private:
    int w_, h_;
    std::vector<std::uint8_t> px_;  // RGBA8, row-major
};

// ---- lines -----------------------------------------------------------------

// DDA: step one pixel along the longer axis, add a float step to the other and
// round. Kept to compare with Bresenham.
void lineDDA(Canvas& c, int x0, int y0, int x1, int y1, Colour col);

// Bresenham: integers only. An error term decides when to step on the shorter
// axis. Works in all eight octants.
void lineBresenham(Canvas& c, int x0, int y0, int x1, int y1, Colour col);

// A thick line made of parallel Bresenham lines.
void lineThick(Canvas& c, int x0, int y0, int x1, int y1, int thickness, Colour col);

// ---- circles ---------------------------------------------------------------

// Midpoint circle: work out one eighth and mirror it. The decision value says
// whether the midpoint between the next two pixels is inside the circle.
void circleMidpoint(Canvas& c, int cx, int cy, int r, Colour col);
void circleFilled(Canvas& c, int cx, int cy, int r, Colour col);

// ---- ellipses --------------------------------------------------------------

// Midpoint ellipse: work out one quarter and mirror it. Region 1 steps in x
// while the curve is flat, region 2 steps in y once it is steep. One region
// alone would leave gaps.
void ellipseMidpoint(Canvas& c, int cx, int cy, int rx, int ry, Colour col);
void ellipseFilled(Canvas& c, int cx, int cy, int rx, int ry, Colour col);

// ---- polylines -------------------------------------------------------------

// Joins a list of points with Bresenham lines. Used to draw curves.
void polyline(Canvas& c, const std::vector<struct Point>& pts, int thickness,
              Colour col, bool closed = false);

// ---- clipping --------------------------------------------------------------

// Cohen-Sutherland region outcodes.
enum : int { kInside = 0, kLeft = 1, kRight = 2, kBottom = 4, kTop = 8 };

int outcode(float x, float y, float xmin, float ymin, float xmax, float ymax);

// Clips the line to the rectangle. Returns false if it is fully outside.
bool clipCohenSutherland(float& x0, float& y0, float& x1, float& y1,
                         float xmin, float ymin, float xmax, float ymax);

// ---- area fills ------------------------------------------------------------

struct Point {
    int x = 0, y = 0;
};

// Scanline fill: for each row, find where the edges cross, sort, and fill
// between pairs. Works on concave shapes.
void fillPolygonScanline(Canvas& c, const std::vector<Point>& poly, Colour col);

// Boundary fill, 4 connected. Uses its own stack, since recursion would
// overflow on a large area.
void fillBoundary(Canvas& c, int x, int y, Colour fill, Colour boundary);

} // namespace lumen::raster
