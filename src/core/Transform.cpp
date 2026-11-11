#include "Transform.h"

#include <cmath>

namespace lumen::xf {

Mat3 Mat3::operator*(const Mat3& o) const {
    Mat3 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) r.m[i][j] += m[i][k] * o.m[k][j];
        }
    return r;
}

PointF Mat3::apply(PointF p) const {
    return PointF{m[0][0] * p.x + m[0][1] * p.y + m[0][2],
                  m[1][0] * p.x + m[1][1] * p.y + m[1][2]};
}

Mat3 translation(float dx, float dy) {
    Mat3 t;
    t.m[0][2] = dx;
    t.m[1][2] = dy;
    return t;
}

Mat3 rotation(float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    Mat3 r;
    r.m[0][0] = c; r.m[0][1] = -s;
    r.m[1][0] = s; r.m[1][1] =  c;
    return r;
}

Mat3 scaling(float sx, float sy) {
    Mat3 s;
    s.m[0][0] = sx;
    s.m[1][1] = sy;
    return s;
}

Mat3 reflectionX() { return scaling(1.0f, -1.0f); }
Mat3 reflectionY() { return scaling(-1.0f, 1.0f); }

Mat3 shearing(float shx, float shy) {
    Mat3 s;
    s.m[0][1] = shx;
    s.m[1][0] = shy;
    return s;
}

Mat3 rotationAbout(float radians, PointF p) {
    return translation(p.x, p.y) * rotation(radians) * translation(-p.x, -p.y);
}

Mat3 scalingAbout(float sx, float sy, PointF p) {
    return translation(p.x, p.y) * scaling(sx, sy) * translation(-p.x, -p.y);
}

Mat3 reflectionAbout(PointF p, float angle) {
    // Turn the line onto the x axis, mirror in the x axis, turn it back.
    return translation(p.x, p.y) * rotation(angle) * reflectionX() *
           rotation(-angle) * translation(-p.x, -p.y);
}

Mat3 inverse(const Mat3& a) {
    const auto& m = a.m;
    const float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                    - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                    + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (std::fabs(det) < 1e-12f) return Mat3{};

    // The adjugate (transposed cofactors) divided by the determinant.
    Mat3 r;
    r.m[0][0] =  (m[1][1] * m[2][2] - m[1][2] * m[2][1]) / det;
    r.m[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) / det;
    r.m[0][2] =  (m[0][1] * m[1][2] - m[0][2] * m[1][1]) / det;
    r.m[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) / det;
    r.m[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]) / det;
    r.m[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) / det;
    r.m[2][0] =  (m[1][0] * m[2][1] - m[1][1] * m[2][0]) / det;
    r.m[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) / det;
    r.m[2][2] =  (m[0][0] * m[1][1] - m[0][1] * m[1][0]) / det;
    return r;
}

Polygon transform(const Polygon& poly, const Mat3& m) {
    Polygon out;
    out.reserve(poly.size());
    for (const PointF& p : poly) out.push_back(m.apply(p));
    return out;
}

} // namespace lumen::xf
