# Algorithms

Every algorithm is our own C++ in `src/core/`, drawing into a plain pixel array (`raster::Canvas`). No graphics library is used. Run `lumen_cp1` to save all ten pages as images.

## Lines (`Raster.cpp`)

| Algorithm | Function | How | Used for |
|---|---|---|---|
| DDA | `lineDDA` | Float step on the shorter axis, rounded each pixel | Comparison with Bresenham; light rays in the vault |
| Bresenham | `lineBresenham` | Integer error term, all eight octants | Walls, the painting's grid, the lamp's cone, the tour route |
| Thick line | `lineThick` | Parallel Bresenham lines side by side | Walls on the plan and minimap |
| Polyline | `polyline` | Joins a list of points | Drawing curves and outlines |

## Circles and ellipses (`Raster.cpp`)

**Midpoint circle** (`circleMidpoint`). Works out one eighth of the circle and mirrors it. Used for the visitor, doors and the lamp.

**Midpoint ellipse** (`ellipseMidpoint`). Works out one quarter and mirrors it, in two regions: x steps while the curve is flat, y steps once it is steep. Used for the atrium oculus, the top of the sculpture and the lamp's light on the floor.

## Filling (`Raster.cpp`)

**Scanline fill** (`fillPolygonScanline`). For each row, find where the edges cross, sort, and fill between pairs. Works on concave shapes. Used for rooms, frame pieces, the plinth.

**Boundary fill** (`fillBoundary`). Spreads from a seed until it meets the boundary colour, using its own stack. Used to highlight the room the visitor is standing in on the minimap.

## Polygons (`Polygon.cpp`)

A polygon is an ordered list of vertices.

| Operation | Function | How |
|---|---|---|
| Area | `area`, `signedArea` | Shoelace formula. The sign gives the vertex order |
| Centroid | `centroid` | Area weighted average of the edges |
| Point in polygon | `contains` | Even-odd rule: count edge crossings of a ray |
| Convex or not | `isConvex` | The turn at every vertex has the same sign |

## Clipping

**Cohen-Sutherland** (`Raster.cpp`, `clipCohenSutherland`). Each endpoint gets a 4 bit code. Both zero: keep. A shared bit: drop. Otherwise cut at an edge and check again. Clips walls and the tour route to the minimap.

**Sutherland-Hodgman** (`Polygon.cpp`, `clipSutherlandHodgman`). Clips a polygon against the left, right, bottom and top edges in turn, keeping inside vertices and adding one wherever an edge crosses. Clips room shapes to the minimap.

## Curves (`Curves.cpp`)

**Bezier.** `bezierQuadratic` and `bezierCubic` use the Bernstein formula; `bezierDeCasteljau` uses repeated interpolation for any number of points. The tests check both agree. The quadratic Bezier is the vault arch, flatter than a semicircle. The cubic Bezier is the picture frame's cross section.

**Catmull-Rom spline** (`catmullRom`). Passes through its points, so it suits a route. Centripetal (`alpha = 0.5`) avoids loops. The guided tour route is a closed loop through the corridor, with extra points every 2.5 m so it stays in the middle.

**Arc length** (`arcLengths`, `pointAtDistance`). Places a point by distance along the curve rather than by t.

## Supporting code

**Bitmap font** (`Font.cpp`). 5 by 7 pixel letters, drawn pixel by pixel, for every label.

**Image export** (`Image.cpp`). Writes a Canvas as a .bmp file, byte by byte.

**JSON reader** (`Json.cpp`). Reads `assets/plan.json` so the project needs no libraries.

**Generated artwork** (`ArtPage.cpp`). Four works for Gallery Three, each made from a seed by a small random number generator: Bresenham lines on a grid, drifting polygons, random polygons clipped by Sutherland-Hodgman and filled by scanline, and cubic Beziers with midpoint circles.

## Tests

`tests/test_core.cpp` has 118 checks:

- Bresenham: endpoints in all eight octants, no gaps on steep lines, matches DDA at 45 degrees
- Thick lines: exact pixel count, thickness on both sides, no gaps on diagonals
- Circle: pixels close to the radius, eight way symmetry, filled area close to pi r squared
- Ellipse: no gaps in any row or column, all four extremes hit, matches a circle when the radii are equal
- Cohen-Sutherland: inside kept, crossing clipped, outside dropped, corner case handled
- Fills: a 10 by 10 square fills exactly 100 pixels, a concave notch stays empty, boundary fill does not leak
- Polygons: area, centroid, point in polygon, convexity
- Sutherland-Hodgman: inside kept, outside removed, corner overlap has the right area, a cut triangle gains a vertex
- Bezier and Catmull-Rom: endpoints, both Bezier methods agree, the spline passes through its points
- Plan and JSON: the real plan loads; a missing file, broken JSON or a missing field each give a clear message
- Image export, every page drawing something, and the same seed giving the same artwork
