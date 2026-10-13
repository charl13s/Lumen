// Polygon.h - polygons, in plain C++. Owner: Priscillah.
//
// A polygon is a list of vertices in order. This file works out its area,
// centre and whether a point is inside it, and clips it against a rectangle
// with Sutherland-Hodgman.
//
// Used on the minimap: room shapes are clipped to the map edge before filling.

#pragma once
#include <vector>

#include "Raster.h"

namespace lumen::poly {

struct PointF {
    float x = 0.0f, y = 0.0f;
};

using Polygon = std::vector<PointF>;

// ---- polygon properties ----------------------------------------------------

// Shoelace formula. Positive when the vertices run anticlockwise (y up).
float signedArea(const Polygon& poly);
float area(const Polygon& poly);

PointF centroid(const Polygon& poly);

// Even-odd rule: cast a ray to the right and count edge crossings.
bool contains(const Polygon& poly, PointF p);

bool isConvex(const Polygon& poly);

// ---- building shapes -------------------------------------------------------

Polygon rectangle(float x0, float y0, float x1, float y1);

// Points on an ellipse from x = rx cos t, y = ry sin t.
Polygon ellipse(PointF centre, float rx, float ry, int segments);

// A star with alternating outer and inner radii. Concave, good for testing.
Polygon star(PointF centre, float outer, float inner, int points);

// ---- clipping --------------------------------------------------------------

// Sutherland-Hodgman: clip against each edge of the rectangle in turn
// (left, right, bottom, top). Each pass keeps the inside part and adds a vertex
// where an edge crosses the boundary. Returns an empty polygon if nothing is left.
Polygon clipSutherlandHodgman(const Polygon& poly,
                              float xmin, float ymin, float xmax, float ymax);

// ---- drawing ---------------------------------------------------------------

// Rounds to whole pixels for the rasteriser.
std::vector<raster::Point> toPixels(const Polygon& poly);

} // namespace lumen::poly
