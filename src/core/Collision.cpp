#include "Collision.h"

#include <algorithm>
#include <cmath>

namespace lumen {
namespace {

bool segmentsIntersect(Vec2 p1, Vec2 p2, Vec2 q1, Vec2 q2) {
    auto cross = [](Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; };
    const Vec2 r{p2.x - p1.x, p2.y - p1.y};
    const Vec2 s{q2.x - q1.x, q2.y - q1.y};
    const float denom = cross(r, s);
    if (std::fabs(denom) < 1e-8f) return false;   // parallel or collinear

    const Vec2 pq{q1.x - p1.x, q1.y - p1.y};
    const float t = cross(pq, s) / denom;
    const float u = cross(pq, r) / denom;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

} // namespace

Collider::Collider(const std::vector<Segment>& walls, float wallThickness)
    : walls_(walls), half_(wallThickness * 0.5f) {}

Vec2 Collider::closestPointOnSegment(const Segment& s, Vec2 p) {
    const float dx = s.b.x - s.a.x;
    const float dy = s.b.y - s.a.y;
    const float len2 = dx * dx + dy * dy;
    if (len2 < 1e-10f) return s.a;

    float t = ((p.x - s.a.x) * dx + (p.y - s.a.y) * dy) / len2;
    t = std::clamp(t, 0.0f, 1.0f);
    return Vec2{s.a.x + t * dx, s.a.y + t * dy};
}

Vec2 Collider::resolve(Vec2 desired, float radius) const {
    Vec2 p = desired;
    const float minDist = radius + half_;

    // Two passes. One is enough in the middle of a wall; the second settles
    // inside corners, where pushing out of one wall can push you into the other.
    for (int pass = 0; pass < 2; ++pass) {
        bool moved = false;

        for (const Segment& w : walls_) {
            const Vec2 c = closestPointOnSegment(w, p);
            float dx = p.x - c.x;
            float dy = p.y - c.y;
            float d2 = dx * dx + dy * dy;

            if (d2 >= minDist * minDist) continue;

            float d = std::sqrt(d2);
            if (d < 1e-5f) {
                // Dead centre on the wall line: push along the wall's normal
                // rather than a zero-length vector.
                const float wx = w.b.x - w.a.x;
                const float wy = w.b.y - w.a.y;
                const float wl = std::sqrt(wx * wx + wy * wy);
                if (wl < 1e-6f) continue;
                dx = -wy / wl;
                dy =  wx / wl;
                d  = 0.0f;
            } else {
                dx /= d;
                dy /= d;
            }

            const float push = minDist - d;
            p.x += dx * push;
            p.y += dy * push;
            moved = true;
        }

        if (!moved) break;
    }
    return p;
}

bool Collider::segmentBlocked(Vec2 a, Vec2 b) const {
    for (const Segment& w : walls_)
        if (segmentsIntersect(a, b, w.a, w.b)) return true;
    return false;
}

} // namespace lumen
