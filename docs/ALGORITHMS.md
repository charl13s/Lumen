# Algorithms

The 2D algorithms are our own C++ in `src/core/`, drawing into a plain pixel array (`raster::Canvas`) with no graphics library. Run `lumen_pages` to save all twelve pages as images and the five animations as GIFs. The gallery itself is drawn with OpenGL, described at the end.

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

**Sutherland-Hodgman** (`Polygon.cpp`, `clipSutherlandHodgman`). Clips a polygon against the left, right, bottom and top edges in turn, keeping inside vertices and adding one wherever an edge crosses. Clips room shapes and the lamp's light to the minimap.

## Curves (`Curves.cpp`)

**Bezier.** `bezierQuadratic` and `bezierCubic` use the Bernstein formula; `bezierDeCasteljau` uses repeated interpolation for any number of points. The tests check both agree. The quadratic Bezier is the vault arch, flatter than a semicircle. The cubic Bezier is the picture frame's cross section.

**Catmull-Rom spline** (`catmullRom`). Passes through its points, so it suits a route. Centripetal (`alpha = 0.5`) avoids loops. The guided tour route is a closed loop through the corridor, with extra points every 2.5 m so it stays in the middle.

**Arc length** (`arcLengths`, `pointAtDistance`). Places a point by distance along the curve rather than by t.

## 2D transformations (`Transform.cpp`)

A point (x, y) is written as (x, y, 1), so every transformation is a 3x3 matrix and any chain of them multiplies into one matrix.

| Transformation | Function | Matrix idea |
|---|---|---|
| Translate | `translation` | Adds dx and dy through the third column |
| Rotate | `rotation` | cos and sin in the top left 2x2 |
| Scale | `scaling` | sx and sy on the diagonal |
| Reflect | `reflectionX`, `reflectionY` | Scale one axis by -1 |
| Shear | `shearing` | x gains a share of y, or y of x |
| About a point | `rotationAbout`, `scalingAbout` | Move the point to the origin, transform, move back |
| In any line | `reflectionAbout` | Turn the line onto the x axis, mirror, turn back |
| Undo | `inverse` | Adjugate divided by the determinant |

Used for the lamp's light and heading on the minimap (turned to the way the visitor faces), Schotter's rotated squares, and the spinning mosaic.

## 3D transformations (`Transform3D.cpp`)

A point is (x, y, z, 1) and every step is a 4x4 matrix: move, scale, and rotate about x, y or z. `lookAt` builds the camera's view from its position and target. `perspective` makes far things smaller: after it, x and y are divided by w, the distance in front of the camera. The final chain is projection x view x model. Gallery Two is built from the plan as a wireframe (walls, doorway, paintings, and the vaults as quadratic Bezier ribs) and drawn with Bresenham, clipped with Cohen-Sutherland.

## Animation (`Anim.cpp`, `Gif.cpp`)

Each animation draws a frame for a time in seconds.

- **Interpolation** (`lerp`): in-between values for numbers, points and colours.
- **Easing** (`easeInOut`, smoothstep 3t^2 - 2t^3): starts and ends gently instead of jerking.
- **Keyframes** (`Track`): values set at chosen times, eased in between. The room light fades on keyframes.
- **Motion by arc length**: the tour moves a set distance each frame, so the speed stays even round corners.
- **GIF output**: each frame's colours are cut to 256 by median cut, then the pixels are compressed with LZW.

## Supporting code

**Bitmap font** (`Font.cpp`). 5 by 7 pixel letters, drawn pixel by pixel, for every label.

**Image export** (`Image.cpp`). Writes a Canvas as a .bmp file, byte by byte.

**JSON reader** (`Json.cpp`). Reads `assets/plan.json`, so the plan needs no library.

**Generated artwork** (`ArtPage.cpp`). Four works for Gallery Three, each made from a seed by a small random number generator: Bresenham lines on a grid, rotated squares, random polygons clipped by Sutherland-Hodgman and filled by scanline, and cubic Beziers with midpoint circles. Each also moves, looping every 8 seconds.

## The gallery in OpenGL

`lumen` is the walkable gallery. OpenGL, GLFW and glad open the window and draw the triangles; glm does the matrix maths; stb_image reads image files. Everything else below is ours.

**The building** (`Builders.cpp`). Every wall in the plan is raised into a 3D block, with a lintel over each doorway and niche. The vault ceilings are quadratic Bezier arches, the frame moulding is a cubic Bezier profile swept round the canvas, and the atrium ceiling is a rectangle with the round oculus cut out, triangulated wedge by wedge.

**Camera and collision** (`Camera.cpp`, `Collision.cpp`). The view is a look-at matrix from the visitor's eye and the projection is a perspective matrix, so each vertex goes through projection x view x model. The visitor is a circle and each wall a thick line; when they overlap, the visitor is pushed back out along the shortest way, which also lets them slide along walls.

**Shading** (`shaders/lumen.vert`, `shaders/lumen.frag`). One shader with a mode per surface: Lambert (diffuse only), Blinn-Phong (per pixel, with the halfway vector), Gouraud (per vertex, used on purpose to show the difference), normal mapping (a bumped surface from a tangent space normal map), emissive, and triplanar (a texture projected along x, y and z and blended, for scans with no texture coordinates).

**Lighting** (`Lighting.cpp`). The lamp is a spotlight that follows the view with a lag, with inverse square falloff fading to zero at its range and a soft cone edge. Each room's vault light is a hemisphere light that fades in when you walk in. The oculus lights the atrium the same way. Colours are lit in linear space, tone mapped (Reinhard) and converted to sRGB once at the end.

**Shadows** (`Gfx.cpp`). Each frame the scene's depth is drawn from the lamp into a shadow map. A pixel is in shadow if something nearer to the lamp covers it; nine nearby lookups are averaged (PCF) for a soft edge.

**Textures** (`Gfx.cpp`). Colour images are uploaded as sRGB and data such as normal maps as linear, with mipmaps and anisotropic filtering. Paintings get a normal map made from their own brightness with a Sobel filter, so brushstrokes and carved reliefs catch the lamp.

**Our 2D code on screen** (`CanvasView.cpp`, `Minimap.cpp`). The minimap, the status strip and the F2 pages are drawn by our Checkpoint 1 and 2 code into a Canvas, then shown as a texture on a flat rectangle. The lamp's light on the minimap is an ellipse turned to your heading with a 3x3 matrix and clipped by Sutherland-Hodgman.

**The tour** (`Tour.cpp`). A Catmull-Rom spline through the corridor waypoints, walked at an even speed by arc length.

**Statues** (`Model.cpp`, `Sculpture.cpp`). Our own glTF 2.0 reader: the node tree with its transforms, the vertex data and the materials. Each statue is scaled to its real height, turned to face the visitor and stood on a plinth, in the atrium or a niche, from `assets/sculpture.json`.

**The torch bearer** (`Skeleton.cpp`, `Character.cpp`, `Intro.cpp`). A skeleton is a tree of bones; a clip is keyframed turns and moves over time, sampled with linear interpolation and slerp for turns. Changing clips crossfades the two poses. Each vertex follows up to four bones, mixed by its weights, in the vertex shader (linear blend skinning). The clips come from an animation library for the same rig and are fitted to this character's bones by name. The intro itself is a small state machine: waiting as stone, walking out, offering the torch, walking back.

## Tests

`tests/test_core.cpp` has 309 checks. None needs a window. The Checkpoint 1 and 2 checks:

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
- 2D transformations: each one on a known point, composite equals step by step, order matters, points about which we turn stay fixed, the inverse undoes, rotation and shear keep area, reflection reverses vertex order
- 3D transformations: turns about x, y and z, the camera view, a point ahead lands in the centre, further looks smaller, behind the camera is not drawn
- Animation: interpolation, easing starts slow and never goes backwards, keyframes, looping, colour blending
- GIF: LZW output decodes back exactly, few colours are kept exactly, the file has the right header and trailer
- Every animation draws something and changes over time

The gallery's checks:

- Walls, floors, ceilings and vaults all face the right way, so none is culled or lit black
- Collision: blocked by walls, slides along them, fits through doorways
- The room light fades in and out on time, and the lamp follows the view with a lag
- Walking the real building: through every doorway both ways, but not through the wall beside one
- Lintels over every doorway and niche, vault ends, frame mouldings, and the oculus left open
- glTF: node transforms, missing normals, mirrored nodes, clear messages for broken files, a real scan loads upright
- Statues and reliefs: every one is credited, its files exist, it faces the visitor, and nobody walks through a plinth
- Skeleton: a bent joint moves the skin the right way, slerp, looping, blending, retargeting, keeping a walk on the spot
- The intro: the route misses every wall, the statue wakes, steps down, offers the torch, waits for E, and turns back to stone
