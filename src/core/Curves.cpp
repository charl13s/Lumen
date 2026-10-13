#include "Curves.h"

#include <algorithm>
#include <cmath>

namespace lumen::curves {
namespace {

P2 lerp(const P2 &a, const P2 &b, float t) {
  return P2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

P3 lerp3(const P3 &a, const P3 &b, float t) {
  return P3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t};
}

float dist(const P2 &a, const P2 &b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  return std::sqrt(dx * dx + dy * dy);
}

} // namespace

// ---- Bezier ----------------------------------------------------------------

P2 bezierQuadratic(const P2 &p0, const P2 &p1, const P2 &p2, float t) {
  const float u = 1.0f - t;
  const float b0 = u * u;
  const float b1 = 2.0f * u * t;
  const float b2 = t * t;
  return P2{b0 * p0.x + b1 * p1.x + b2 * p2.x,
            b0 * p0.y + b1 * p1.y + b2 * p2.y};
}

P2 bezierCubic(const P2 &p0, const P2 &p1, const P2 &p2, const P2 &p3,
               float t) {
  const float u = 1.0f - t;
  const float b0 = u * u * u;
  const float b1 = 3.0f * u * u * t;
  const float b2 = 3.0f * u * t * t;
  const float b3 = t * t * t;
  return P2{b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x,
            b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y};
}

P2 bezierDeCasteljau(const std::vector<P2> &control, float t) {
  if (control.empty())
    return P2{};
  // Interpolate neighbours until one point is left. That point is on the curve.
  std::vector<P2> work = control;
  for (std::size_t level = work.size(); level > 1; --level)
    for (std::size_t i = 0; i + 1 < level; ++i)
      work[i] = lerp(work[i], work[i + 1], t);
  return work[0];
}

P3 bezierDeCasteljau3(const std::vector<P3> &control, float t) {
  if (control.empty())
    return P3{};
  std::vector<P3> work = control;
  for (std::size_t level = work.size(); level > 1; --level)
    for (std::size_t i = 0; i + 1 < level; ++i)
      work[i] = lerp3(work[i], work[i + 1], t);
  return work[0];
}

std::vector<P2> sampleBezier(const std::vector<P2> &control, int segments) {
  std::vector<P2> out;
  if (control.empty() || segments < 1)
    return out;
  out.reserve(static_cast<std::size_t>(segments) + 1);
  for (int i = 0; i <= segments; ++i)
    out.push_back(bezierDeCasteljau(control, static_cast<float>(i) / segments));
  return out;
}

std::vector<P3> sampleBezier3(const std::vector<P3> &control, int segments) {
  std::vector<P3> out;
  if (control.empty() || segments < 1)
    return out;
  out.reserve(static_cast<std::size_t>(segments) + 1);
  for (int i = 0; i <= segments; ++i)
    out.push_back(
        bezierDeCasteljau3(control, static_cast<float>(i) / segments));
  return out;
}

// ---- Catmull-Rom -----------------------------------------------------------

P2 catmullRom(const P2 &p0, const P2 &p1, const P2 &p2, const P2 &p3, float t,
              float alpha) {
  // Knot spacing is the distance between points raised to alpha.
  // 0.5 (square root of distance) avoids loops on uneven spacing.
  auto knot = [alpha](float t0, const P2 &a, const P2 &b) {
    const float d = dist(a, b);
    return t0 + std::pow(std::max(d, 1e-5f), alpha);
  };

  const float t0 = 0.0f;
  const float t1 = knot(t0, p0, p1);
  const float t2 = knot(t1, p1, p2);
  const float t3 = knot(t2, p2, p3);

  // Map t in [0,1] onto the segment between the middle two knots.
  const float tt = t1 + (t2 - t1) * std::clamp(t, 0.0f, 1.0f);

  auto mix = [](const P2 &a, const P2 &b, float ta, float tb, float x) {
    const float d = tb - ta;
    if (std::fabs(d) < 1e-6f)
      return a;
    const float w = (tb - x) / d;
    const float v = (x - ta) / d;
    return P2{a.x * w + b.x * v, a.y * w + b.y * v};
  };

  // Barry and Goldman: three levels of interpolation.
  const P2 a1 = mix(p0, p1, t0, t1, tt);
  const P2 a2 = mix(p1, p2, t1, t2, tt);
  const P2 a3 = mix(p2, p3, t2, t3, tt);
  const P2 b1 = mix(a1, a2, t0, t2, tt);
  const P2 b2 = mix(a2, a3, t1, t3, tt);
  return mix(b1, b2, t1, t2, tt);
}

std::vector<P2> sampleCatmullRomLoop(const std::vector<P2> &points,
                                     int perSegment, float alpha) {
  std::vector<P2> out;
  const std::size_t n = points.size();
  if (n < 3 || perSegment < 1)
    return points;

  out.reserve(n * static_cast<std::size_t>(perSegment));
  for (std::size_t i = 0; i < n; ++i) {
    const P2 &p0 = points[(i + n - 1) % n];
    const P2 &p1 = points[i];
    const P2 &p2 = points[(i + 1) % n];
    const P2 &p3 = points[(i + 2) % n];
    for (int s = 0; s < perSegment; ++s)
      out.push_back(catmullRom(p0, p1, p2, p3,
                               static_cast<float>(s) / perSegment, alpha));
  }
  return out;
}

std::vector<P2> sampleCatmullRomOpen(const std::vector<P2> &points,
                                     int perSegment, float alpha) {
  std::vector<P2> out;
  const std::size_t n = points.size();
  if (n < 2 || perSegment < 1)
    return points;

  // The end points have no outer neighbour, so one is made by reflection.
  auto at = [&](long long i) -> P2 {
    if (i < 0) {
      return P2{2.0f * points[0].x - points[1].x,
                2.0f * points[0].y - points[1].y};
    }
    if (static_cast<std::size_t>(i) >= n) {
      return P2{2.0f * points[n - 1].x - points[n - 2].x,
                2.0f * points[n - 1].y - points[n - 2].y};
    }
    return points[static_cast<std::size_t>(i)];
  };

  for (std::size_t i = 0; i + 1 < n; ++i) {
    const P2 p0 = at(static_cast<long long>(i) - 1);
    const P2 p1 = points[i];
    const P2 p2 = points[i + 1];
    const P2 p3 = at(static_cast<long long>(i) + 2);
    for (int s = 0; s < perSegment; ++s)
      out.push_back(catmullRom(p0, p1, p2, p3,
                               static_cast<float>(s) / perSegment, alpha));
  }
  out.push_back(points[n - 1]);
  return out;
}

// ---- arc length ------------------------------------------------------------

std::vector<float> arcLengths(const std::vector<P2> &pts) {
  std::vector<float> out(pts.size(), 0.0f);
  for (std::size_t i = 1; i < pts.size(); ++i)
    out[i] = out[i - 1] + dist(pts[i - 1], pts[i]);
  return out;
}

P2 pointAtDistance(const std::vector<P2> &pts,
                   const std::vector<float> &lengths, float distance) {
  if (pts.empty())
    return P2{};
  if (pts.size() == 1 || lengths.size() != pts.size())
    return pts[0];

  const float total = lengths.back();
  if (total <= 1e-5f)
    return pts[0];

  // Wrap around, so a loop can be followed forever.
  float d = std::fmod(distance, total);
  if (d < 0.0f)
    d += total;

  const auto it = std::upper_bound(lengths.begin(), lengths.end(), d);
  const std::size_t hi = std::min<std::size_t>(
      static_cast<std::size_t>(it - lengths.begin()), pts.size() - 1);
  const std::size_t lo = (hi == 0) ? 0 : hi - 1;

  const float span = lengths[hi] - lengths[lo];
  const float f = (span > 1e-6f) ? (d - lengths[lo]) / span : 0.0f;
  return lerp(pts[lo], pts[hi], f);
}

} // namespace lumen::curves
