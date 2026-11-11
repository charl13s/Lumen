// Transform.h - 2D transformations with 3x3 matrices. Owner: Priscillah.
//
// A point (x, y) is written as (x, y, 1), so moving it is a matrix multiply just
// like rotating or scaling it. That lets any chain of transformations be
// combined into one matrix and applied to every vertex at once.

#pragma once
#include "Polygon.h"

namespace lumen::xf {

using poly::PointF;
using poly::Polygon;

struct Mat3 {
    float m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    // a * b means "apply b first, then a".
    Mat3 operator*(const Mat3& o) const;
    PointF apply(PointF p) const;
};

// ---- the basic transformations, about the origin ---------------------------

Mat3 translation(float dx, float dy);
Mat3 rotation(float radians);              // anticlockwise when y points up
Mat3 scaling(float sx, float sy);
Mat3 reflectionX();                         // mirror in the x axis: y becomes -y
Mat3 reflectionY();                         // mirror in the y axis: x becomes -x
Mat3 shearing(float shx, float shy);        // x += shx * y, y += shy * x

// ---- about any point or line -----------------------------------------------
// Move the point to the origin, transform, move it back.

Mat3 rotationAbout(float radians, PointF pivot);
Mat3 scalingAbout(float sx, float sy, PointF pivot);
Mat3 reflectionAbout(PointF pointOnLine, float lineAngle);   // mirror in any line

// The matrix that undoes m. Returns the identity if m cannot be undone.
Mat3 inverse(const Mat3& m);

Polygon transform(const Polygon& poly, const Mat3& m);

} // namespace lumen::xf
