#include "Polygon.h"

#include <cmath>

namespace lumen::poly {
namespace {

constexpr float kPi = 3.14159265358979f;

// Which side of one clip edge a point is on, and where a line crosses that edge.
enum class Edge { Left, Right, Bottom, Top };

bool inside(PointF p, Edge e, float xmin, float ymin, float xmax, float ymax) {
    switch (e) {
        case Edge::Left:   return p.x >= xmin;
        case Edge::Right:  return p.x <= xmax;
        case Edge::Bottom: return p.y >= ymin;
        case Edge::Top:    return p.y <= ymax;
    }
    return false;
}

PointF crossing(PointF a, PointF b, Edge e, float xmin, float ymin, float xmax, float ymax) {
    float t = 0.0f;
    switch (e) {
        case Edge::Left:   t = (xmin - a.x) / (b.x - a.x); return {xmin, a.y + t * (b.y - a.y)};
        case Edge::Right:  t = (xmax - a.x) / (b.x - a.x); return {xmax, a.y + t * (b.y - a.y)};
        case Edge::Bottom: t = (ymin - a.y) / (b.y - a.y); return {a.x + t * (b.x - a.x), ymin};
        case Edge::Top:    t = (ymax - a.y) / (b.y - a.y); return {a.x + t * (b.x - a.x), ymax};
    }
    return a;
}

} // namespace

// ---- polygon properties ----------------------------------------------------

float signedArea(const Polygon& poly) {
    const std::size_t n = poly.size();
    if (n < 3) return 0.0f;
    float sum = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const PointF& a = poly[i];
        const PointF& b = poly[(i + 1) % n];
        sum += a.x * b.y - b.x * a.y;
    }
    return sum * 0.5f;
}

float area(const Polygon& poly) { return std::fabs(signedArea(poly)); }

PointF centroid(const Polygon& poly) {
    const std::size_t n = poly.size();
    if (n == 0) return {};
    const float a = signedArea(poly);
    if (std::fabs(a) < 1e-8f) {
        // No area: fall back to the average of the vertices.
        PointF s;
        for (const PointF& p : poly) { s.x += p.x; s.y += p.y; }
        return PointF{s.x / n, s.y / n};
    }
    float cx = 0.0f, cy = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const PointF& p = poly[i];
        const PointF& q = poly[(i + 1) % n];
        const float cross = p.x * q.y - q.x * p.y;
        cx += (p.x + q.x) * cross;
        cy += (p.y + q.y) * cross;
    }
    return PointF{cx / (6.0f * a), cy / (6.0f * a)};
}

bool contains(const Polygon& poly, PointF p) {
    bool in = false;
    const std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const PointF& a = poly[i];
        const PointF& b = poly[j];
        // Does this edge cross the horizontal line through p, to the right of p?
        if ((a.y > p.y) != (b.y > p.y)) {
            const float x = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (p.x < x) in = !in;
        }
    }
    return in;
}

bool isConvex(const Polygon& poly) {
    const std::size_t n = poly.size();
    if (n < 3) return false;
    int sign = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const PointF& a = poly[i];
        const PointF& b = poly[(i + 1) % n];
        const PointF& c = poly[(i + 2) % n];
        const float cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
        if (std::fabs(cross) < 1e-8f) continue;
        const int s = cross > 0.0f ? 1 : -1;
        if (sign == 0) sign = s;
        else if (s != sign) return false;
    }
    return true;
}

// ---- building shapes -------------------------------------------------------

Polygon rectangle(float x0, float y0, float x1, float y1) {
    return Polygon{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

Polygon ellipse(PointF c, float rx, float ry, int segments) {
    if (segments < 3) segments = 3;
    Polygon out;
    out.reserve(static_cast<std::size_t>(segments));
    for (int i = 0; i < segments; ++i) {
        const float t = 2.0f * kPi * static_cast<float>(i) / segments;
        out.push_back(PointF{c.x + rx * std::cos(t), c.y + ry * std::sin(t)});
    }
    return out;
}

Polygon star(PointF c, float outer, float inner, int points) {
    if (points < 2) points = 2;
    Polygon out;
    for (int i = 0; i < points * 2; ++i) {
        const float r = (i % 2 == 0) ? outer : inner;
        const float t = kPi * static_cast<float>(i) / points - kPi * 0.5f;
        out.push_back(PointF{c.x + r * std::cos(t), c.y + r * std::sin(t)});
    }
    return out;
}

// ---- clipping --------------------------------------------------------------

Polygon clipSutherlandHodgman(const Polygon& poly,
                              float xmin, float ymin, float xmax, float ymax) {
    Polygon out = poly;
    for (Edge e : {Edge::Left, Edge::Right, Edge::Bottom, Edge::Top}) {
        if (out.empty()) break;
        const Polygon in = out;
        out.clear();

        PointF prev = in.back();
        bool prevIn = inside(prev, e, xmin, ymin, xmax, ymax);
        for (const PointF& cur : in) {
            const bool curIn = inside(cur, e, xmin, ymin, xmax, ymax);
            if (curIn) {
                if (!prevIn) out.push_back(crossing(prev, cur, e, xmin, ymin, xmax, ymax));
                out.push_back(cur);                 // inside: keep it
            } else if (prevIn) {
                out.push_back(crossing(prev, cur, e, xmin, ymin, xmax, ymax));
            }                                        // both outside: drop it
            prev = cur;
            prevIn = curIn;
        }
    }
    if (out.size() < 3) out.clear();
    return out;
}

// ---- drawing ---------------------------------------------------------------

std::vector<raster::Point> toPixels(const Polygon& poly) {
    std::vector<raster::Point> out;
    out.reserve(poly.size());
    for (const PointF& p : poly)
        out.push_back(raster::Point{static_cast<int>(std::lround(p.x)),
                                    static_cast<int>(std::lround(p.y))});
    return out;
}

} // namespace lumen::poly
