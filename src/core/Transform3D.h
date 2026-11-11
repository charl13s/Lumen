// Transform3D.h - 3D transformations with 4x4 matrices, and wireframes.
// Owner: Erykah.
//
// A 3D point (x, y, z) is written as (x, y, z, 1). Moving, rotating and scaling
// it are matrix multiplies, and so is the perspective projection that turns it
// into a point on the screen. Lines between projected points are then drawn
// with our own Bresenham, clipped with Cohen-Sutherland.

#pragma once
#include <utility>
#include <vector>

#include "Plan.h"
#include "Raster.h"

namespace lumen::xf3 {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Mat4 {
    float m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

    // a * b means "apply b first, then a".
    Mat4 operator*(const Mat4& o) const;
    // Transforms a point; w is returned separately for the perspective divide.
    Vec3 apply(Vec3 p, float* w = nullptr) const;
};

Mat4 translation(float x, float y, float z);
Mat4 scaling(float x, float y, float z);
Mat4 rotationX(float radians);
Mat4 rotationY(float radians);
Mat4 rotationZ(float radians);

// A camera at `eye` looking at `target`, with y up.
Mat4 lookAt(Vec3 eye, Vec3 target);
// Perspective: things further away look smaller. fovY is the vertical angle.
Mat4 perspective(float fovY, float aspect, float nearPlane, float farPlane);

// Projects a point to pixel coordinates. Returns false if it is behind the camera.
bool project(const Mat4& viewProj, Vec3 p, int width, int height, float& sx, float& sy);

// A wireframe: points, and which pairs of them are joined.
struct Wire {
    std::vector<Vec3> points;
    std::vector<std::pair<int, int>> edges;

    int add(Vec3 p);
    void line(Vec3 a, Vec3 b);
};

// One gallery room from the plan: walls with the doorway left open, and the
// vaults, each arch a quadratic Bezier. Plan x is world x, plan y is world -z.
Wire roomWire(const RoomPlan& room, float wallHeight, float apex);

// Draws every edge, clipped to the canvas.
void drawWire(raster::Canvas& c, const Wire& w, const Mat4& viewProj, raster::Colour col);

} // namespace lumen::xf3
