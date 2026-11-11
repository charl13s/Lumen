#include "Transform3D.h"

#include <cmath>

#include "Curves.h"

namespace lumen::xf3 {
namespace {

Vec3 sub(Vec3 a, Vec3 b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 normalise(Vec3 v) {
    const float len = std::sqrt(dot(v, v));
    return len > 1e-8f ? Vec3{v.x / len, v.y / len, v.z / len} : v;
}

// Plan (x, y) at height h, as a world point: plan y north is world -z.
Vec3 world(float px, float h, float py) { return Vec3{px, h, -py}; }

} // namespace

Mat4 Mat4::operator*(const Mat4& o) const {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            r.m[i][j] = 0.0f;
            for (int k = 0; k < 4; ++k) r.m[i][j] += m[i][k] * o.m[k][j];
        }
    return r;
}

Vec3 Mat4::apply(Vec3 p, float* w) const {
    const Vec3 out{m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
                   m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
                   m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3]};
    if (w) *w = m[3][0] * p.x + m[3][1] * p.y + m[3][2] * p.z + m[3][3];
    return out;
}

Mat4 translation(float x, float y, float z) {
    Mat4 t;
    t.m[0][3] = x;
    t.m[1][3] = y;
    t.m[2][3] = z;
    return t;
}

Mat4 scaling(float x, float y, float z) {
    Mat4 s;
    s.m[0][0] = x;
    s.m[1][1] = y;
    s.m[2][2] = z;
    return s;
}

Mat4 rotationX(float a) {
    const float c = std::cos(a), s = std::sin(a);
    Mat4 r;
    r.m[1][1] = c; r.m[1][2] = -s;
    r.m[2][1] = s; r.m[2][2] =  c;
    return r;
}

Mat4 rotationY(float a) {
    const float c = std::cos(a), s = std::sin(a);
    Mat4 r;
    r.m[0][0] =  c; r.m[0][2] = s;
    r.m[2][0] = -s; r.m[2][2] = c;
    return r;
}

Mat4 rotationZ(float a) {
    const float c = std::cos(a), s = std::sin(a);
    Mat4 r;
    r.m[0][0] = c; r.m[0][1] = -s;
    r.m[1][0] = s; r.m[1][1] =  c;
    return r;
}

Mat4 lookAt(Vec3 eye, Vec3 target) {
    // The camera's own axes: forward, right and up.
    const Vec3 f = normalise(sub(target, eye));
    const Vec3 s = normalise(cross(f, Vec3{0, 1, 0}));
    const Vec3 u = cross(s, f);
    Mat4 v;
    v.m[0][0] =  s.x; v.m[0][1] =  s.y; v.m[0][2] =  s.z; v.m[0][3] = -dot(s, eye);
    v.m[1][0] =  u.x; v.m[1][1] =  u.y; v.m[1][2] =  u.z; v.m[1][3] = -dot(u, eye);
    v.m[2][0] = -f.x; v.m[2][1] = -f.y; v.m[2][2] = -f.z; v.m[2][3] =  dot(f, eye);
    return v;
}

Mat4 perspective(float fovY, float aspect, float n, float f) {
    const float t = 1.0f / std::tan(fovY * 0.5f);
    Mat4 p;
    p.m[0][0] = t / aspect;
    p.m[1][1] = t;
    p.m[2][2] = (f + n) / (n - f);
    p.m[2][3] = 2.0f * f * n / (n - f);
    p.m[3][2] = -1.0f;   // w becomes the distance in front of the camera
    p.m[3][3] = 0.0f;
    return p;
}

bool project(const Mat4& vp, Vec3 p, int width, int height, float& sx, float& sy) {
    float w = 0.0f;
    const Vec3 c = vp.apply(p, &w);
    if (w <= 1e-4f) return false;
    // Divide by w (things further away shrink), then map -1..1 to pixels.
    sx = (c.x / w * 0.5f + 0.5f) * static_cast<float>(width);
    sy = (1.0f - (c.y / w * 0.5f + 0.5f)) * static_cast<float>(height);
    return true;
}

int Wire::add(Vec3 p) {
    points.push_back(p);
    return static_cast<int>(points.size()) - 1;
}

void Wire::line(Vec3 a, Vec3 b) { edges.push_back({add(a), add(b)}); }

Wire roomWire(const RoomPlan& room, float h, float apex) {
    Wire w;
    const Rect& r = room.rect;
    const float doorTop = 2.6f;

    // Floor and wall tops, and the four corners.
    const Vec3 f0 = world(r.x0, 0, r.y0), f1 = world(r.x1, 0, r.y0), f2 = world(r.x1, 0, r.y1), f3 = world(r.x0, 0, r.y1);
    const Vec3 t0 = world(r.x0, h, r.y0), t1 = world(r.x1, h, r.y0), t2 = world(r.x1, h, r.y1), t3 = world(r.x0, h, r.y1);
    w.line(f0, f1); w.line(f1, f2); w.line(f2, f3); w.line(f3, f0);
    w.line(t0, t1); w.line(t1, t2); w.line(t2, t3); w.line(t3, t0);
    w.line(f0, t0); w.line(f1, t1); w.line(f2, t2); w.line(f3, t3);

    // The doorway: two jambs and a lintel.
    const Segment d = room.door.segment(r);
    const Vec3 da = world(d.a.x, 0, d.a.y), db = world(d.b.x, 0, d.b.y);
    const Vec3 ua = world(d.a.x, doorTop, d.a.y), ub = world(d.b.x, doorTop, d.b.y);
    w.line(da, ua); w.line(db, ub); w.line(ua, ub);

    // The paintings, as rectangles a little off each wall.
    for (const HangPoint& hp : room.hangs) {
        const float mx = (hp.span.a.x + hp.span.b.x) * 0.5f, my = (hp.span.a.y + hp.span.b.y) * 0.5f;
        const float half = hp.width() * 0.39f, tall = hp.width() * 0.78f * 0.74f * 0.5f;
        const float ax = hp.span.horizontal() ? 1.0f : 0.0f, ay = hp.span.horizontal() ? 0.0f : 1.0f;
        const float cx = mx + hp.inwardNormal.x * 0.05f, cy = my + hp.inwardNormal.y * 0.05f;
        const Vec3 p0 = world(cx - ax * half, 1.52f - tall, cy - ay * half);
        const Vec3 p1 = world(cx + ax * half, 1.52f - tall, cy + ay * half);
        const Vec3 p2 = world(cx + ax * half, 1.52f + tall, cy + ay * half);
        const Vec3 p3 = world(cx - ax * half, 1.52f + tall, cy - ay * half);
        w.line(p0, p1); w.line(p1, p2); w.line(p2, p3); w.line(p3, p0);
    }

    // The vaults: each arch is a quadratic Bezier, repeated as ribs along the
    // room, with lines running the length of the room joining the ribs.
    if (room.vaults.empty()) return w;
    const bool alongY = !room.vaults.front().horizontal();
    const float span = (alongY ? r.width() : r.height()) / static_cast<float>(room.vaults.size());
    const float lo = alongY ? r.y0 : r.x0, hi = alongY ? r.y1 : r.x1;
    const int ribs = 6, steps = 16;
    for (const Segment& crown : room.vaults) {
        const float c = alongY ? crown.a.x : crown.a.y;
        const curves::P2 c0{c - span * 0.5f, h}, c1{c, h + 2.0f * (apex - h)}, c2{c + span * 0.5f, h};
        auto at = [&](float t, float along) {
            const curves::P2 p = curves::bezierQuadratic(c0, c1, c2, t);
            return alongY ? world(p.x, p.y, along) : world(along, p.y, p.x);
        };
        for (int k = 0; k <= ribs; ++k) {
            const float along = lo + (hi - lo) * static_cast<float>(k) / ribs;
            for (int s = 0; s < steps; ++s)
                w.line(at(static_cast<float>(s) / steps, along), at(static_cast<float>(s + 1) / steps, along));
        }
        for (float t : {0.25f, 0.5f, 0.75f}) w.line(at(t, lo), at(t, hi));
    }
    return w;
}

void drawWire(raster::Canvas& c, const Wire& wire, const Mat4& vp, raster::Colour col) {
    std::vector<float> xs(wire.points.size()), ys(wire.points.size());
    std::vector<bool> ok(wire.points.size());
    for (std::size_t i = 0; i < wire.points.size(); ++i)
        ok[i] = project(vp, wire.points[i], c.width(), c.height(), xs[i], ys[i]);

    const float maxX = static_cast<float>(c.width() - 1), maxY = static_cast<float>(c.height() - 1);
    for (const auto& [a, b] : wire.edges) {
        if (!ok[a] || !ok[b]) continue;
        float x0 = xs[a], y0 = ys[a], x1 = xs[b], y1 = ys[b];
        if (!raster::clipCohenSutherland(x0, y0, x1, y1, 0.0f, 0.0f, maxX, maxY)) continue;
        raster::lineBresenham(c, static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0)),
                              static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)), col);
    }
}

} // namespace lumen::xf3
