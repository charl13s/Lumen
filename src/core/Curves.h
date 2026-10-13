// Curves.h - Bezier curves and Catmull-Rom splines, in plain C++.
//
// Curves are turned into lists of points, which Raster.cpp then draws.
//
//   Quadratic Bezier   the vault arch, flatter than a semicircle
//   Cubic Bezier       the cross section of a picture frame
//   Catmull-Rom        the guided tour route, because it passes through its
//   points
//
// Bezier is written two ways, Bernstein formula and de Casteljau, and the tests
// check they agree.

#pragma once
#include <vector>

namespace lumen::curves {

struct P2 {
  float x = 0.0f, y = 0.0f;
};

struct P3 {
  float x = 0.0f, y = 0.0f, z = 0.0f;
};

// ---- Bezier ----------------------------------------------------------------

// Quadratic, by the Bernstein polynomial form:
//   B(t) = (1-t)^2 P0 + 2(1-t)t P1 + t^2 P2
P2 bezierQuadratic(const P2 &p0, const P2 &p1, const P2 &p2, float t);

// Cubic, Bernstein form:
//   B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3 P3
P2 bezierCubic(const P2 &p0, const P2 &p1, const P2 &p2, const P2 &p3, float t);

// Any degree, by de Casteljau: keep interpolating between neighbouring points
// until one is left.
P2 bezierDeCasteljau(const std::vector<P2> &control, float t);
P3 bezierDeCasteljau3(const std::vector<P3> &control, float t);

// Turns a curve into `segments + 1` points.
std::vector<P2> sampleBezier(const std::vector<P2> &control, int segments);
std::vector<P3> sampleBezier3(const std::vector<P3> &control, int segments);

// ---- Catmull-Rom spline ----------------------------------------------------

// One span from p1 to p2, shaped by p0 and p3. Passes through p1 and p2.
// alpha: 0 uniform, 0.5 centripetal (no loops or kinks), 1 chordal.
P2 catmullRom(const P2 &p0, const P2 &p1, const P2 &p2, const P2 &p3, float t,
              float alpha = 0.5f);

// A closed loop through the points, `perSegment` samples per span.
std::vector<P2> sampleCatmullRomLoop(const std::vector<P2> &points,
                                     int perSegment, float alpha = 0.5f);

// Same, but with a start and an end.
std::vector<P2> sampleCatmullRomOpen(const std::vector<P2> &points,
                                     int perSegment, float alpha = 0.5f);

// ---- arc length ------------------------------------------------------------

// Running distance along the curve at each point. Equal steps in t are not
// equal distances, so the tour uses this to move at a constant speed.
std::vector<float> arcLengths(const std::vector<P2> &pts);

// The point a given distance along the curve. Wraps around on a loop.
P2 pointAtDistance(const std::vector<P2> &pts,
                   const std::vector<float> &lengths, float distance);

} // namespace lumen::curves
