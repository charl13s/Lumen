// Raster.h - our own rasteriser, written from the algorithms in weeks 4 and 5.
//
// WHY THIS FILE EXISTS, and why it must not be replaced by library calls:
//
// A 3D OpenGL walkthrough hides the rasteriser inside the GPU. If every pixel in
// LUMEN were drawn by OpenGL, we could not point at a single line of our own code
// that implements a graphics algorithm, and the examiner's fair question -
// "where does your project use Bresenham?" - would have no answer.
//
// The minimap is 2D, so we draw it ourselves, into a CPU pixel buffer that is
// then uploaded as one texture per frame. Every algorithm below is on the course:
//
//   DDA line            ICS 3204 week 4   (kept for the comparison write-up)
//   Bresenham line      week 4            - the minimap's walls
//   Midpoint circle     week 4            - the visitor marker, room nodes
//   Cohen-Sutherland    week 4            - clipping to the minimap viewport
//   Scanline fill       week 5            - filled room footprints
//   Boundary fill       week 5            - the revealed-room highlight
//
// Everything here is integer or float arithmetic on a plain array. No OpenGL.
// That makes it testable: see tests/test_core.cpp.

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

// A CPU pixel buffer. Origin is top-left, matching the texture we upload.
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

// DDA: step the major axis by 1, accumulate a fractional step on the minor axis.
// Simple, but it carries a float per pixel and rounds every step. We keep it so
// the report can compare it against Bresenham on the same input.
void lineDDA(Canvas& c, int x0, int y0, int x1, int y1, Colour col);

// Bresenham: integer only. Tracks an error term that says whether the ideal line
// has drifted far enough from the pixel centre to justify a step on the minor
// axis. No division, no floats, no rounding. Handles all eight octants.
void lineBresenham(Canvas& c, int x0, int y0, int x1, int y1, Colour col);

// Thickness is drawn as parallel Bresenham runs offset perpendicular to the line.
void lineThick(Canvas& c, int x0, int y0, int x1, int y1, int thickness, Colour col);

// ---- circles ---------------------------------------------------------------

// Midpoint circle: compute one octant, mirror it to the other seven. The decision
// variable asks whether the midpoint between the two candidate pixels falls
// inside or outside the circle.
void circleMidpoint(Canvas& c, int cx, int cy, int r, Colour col);
void circleFilled(Canvas& c, int cx, int cy, int r, Colour col);

// ---- clipping --------------------------------------------------------------

// Cohen-Sutherland region outcodes.
enum : int { kInside = 0, kLeft = 1, kRight = 2, kBottom = 4, kTop = 8 };

int outcode(float x, float y, float xmin, float ymin, float xmax, float ymax);

// Clips the segment to the rectangle. Returns false if it lies entirely outside;
// otherwise rewrites the endpoints to the clipped segment.
bool clipCohenSutherland(float& x0, float& y0, float& x1, float& y1,
                         float xmin, float ymin, float xmax, float ymax);

// ---- area fills ------------------------------------------------------------

struct Point {
    int x = 0, y = 0;
};

// Scanline polygon fill: for each scanline, find the x where each edge crosses,
// sort them, and fill between pairs. Handles concave outlines; the parity rule
// is what makes the paired spans correct.
void fillPolygonScanline(Canvas& c, const std::vector<Point>& poly, Colour col);

// Boundary fill, 4-connected, iterative (an explicit stack, because the
// recursive form overflows on a region the size of a room).
void fillBoundary(Canvas& c, int x, int y, Colour fill, Colour boundary);

} // namespace lumen::raster
