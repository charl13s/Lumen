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

## Pop art from a photograph (`PopArt.cpp`)

Gallery Three's centrepiece is one portrait printed four ways, after Warhol's screen prints. The photograph is read with stb_image; every step after that is ours, on a Canvas.

| Step | Function | How |
|---|---|---|
| Colour to grey | `luminance` | Rec. 601 luma, Y = 0.299 R + 0.587 G + 0.114 B. Green counts most because the eye is most sensitive to it |
| Contrast | `stretch` | A 256 bin histogram. The level below which 2% of pixels fall becomes black, the level above which 2% fall becomes white, and everything between is spread out linearly |
| Posterise | `posterise` | Cuts the grey range into equal bands, one flat colour each |
| Halftone | `halftone` | One dot per 5 x 5 cell, drawn with the midpoint filled circle. The dot's area matches the cell's brightness: pi r^2 = b x cell^2, so r = cell x sqrt(b / pi). b goes through a printer's tone curve (b^1.6) first so the shadows do not merge |
| Error diffusion | `floydSteinberg` | Each pixel is rounded to dark or light and the rounding error is passed on: 7/16 right, 3/16 down left, 5/16 down, 1/16 down right. The average brightness is kept, which the tests check |
| Ordered dither | `bayer` | A 4 x 4 Bayer matrix of thresholds. Between two palette colours, a pixel rounds up when its fraction is above its threshold, so a 50% grey lights exactly 8 of every 16 pixels |

`warholGrid` lays the four out in a 2 x 2 grid on a dark mount, with the title in our bitmap font, and the result goes onto the wall as a texture.

## Supporting code

**Bitmap font** (`Font.cpp`). 5 by 7 pixel letters, drawn pixel by pixel, for every label.

**Image export** (`Image.cpp`). Writes a Canvas as a .bmp file, byte by byte.

**JSON reader** (`Json.cpp`). Reads `assets/plan.json`, so the plan needs no library.

**Generated artwork** (`ArtPage.cpp`). Four works, shown on the F2 pages, each made from a seed by a small random number generator: Bresenham lines on a grid, rotated squares, random polygons clipped by Sutherland-Hodgman and filled by scanline, and cubic Beziers with midpoint circles. Each also moves, looping every 8 seconds.

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

**Gallery Three, the Hall of Champions** (`RoomThree.cpp`). Six edited sports photographs, the pop art, and the six trophies that go with the photographs, in a hall dressed in gold.

- *The hall.* The plan's walls are lined: a band of gold damask wallpaper, a darker panel band, and boxes for the skirting, dado rail and crown moulding, split round the doorway. The plan's hang points sit 12 cm off the wall, so each is moved onto the lining and the frames sit against it. The vault is gilded by drawing the plan's vaults again, scaled 2 cm in about the room's middle so the gold sits just inside the plain plaster.
- *Textures drawn by our own code.* The damask is one 256 px tile: a diamond trellis of thick Bresenham lines, midpoint ellipses and circles for the medallions and studs. Anything crossing the tile's edge is drawn again from the other side, so it repeats without a seam. The terrazzo floor is black with chips placed by a seeded linear congruential generator (the same floor every run), each chip also drawn in the eight neighbouring tiles. The title is "engraved": the text drawn once in a light colour one step down and right, then in a dark colour on top.
- *Solid furniture.* Rooms can now say what is solid (`Room::footprints()`, a box on the plan each). `main.cpp` adds each box's outline to the collider, pulled in by half a wall thickness so the visitor stops their own radius from its true edge, as the atrium's plinths already did. Gallery Three makes the roped-off trophies and the two athletes' plinths solid. Rooms that do not use it are unchanged.
- *Trophies turned on a lathe.* Each trophy's outline is a profile of (radius, height) points, made of cubic Bezier segments sampled with `bezierCubic`, then turned round the vertical axis: point (r, y) at angle a lands at (r cos a, y, r sin a). The normal there is the profile's tangent (dr, dy) turned a quarter, (dy, -dr), swung round with it. The lathe can also scale the radius by angle as it turns: r(1 + 0.05 cos 18a sin(pi v)) gives the cup's bowl 18 flutes that fade out at both ends of the band, and the same idea flutes the golden ball's stem. A shaped surface needs its normals worked out afresh, as the cross product of its two directions taken from the neighbouring points. The six are our own designs, one per sport: a laurel wreath, a two-handled cup, a golden ball, a sprint medal, two belts on a drum, and a basketball in a cradle.
- *Engraving and enamel.* The fine detail is drawn by our own code as a height map (white is high), turned into a normal map with a Sobel filter (the slope across and down tips the normal), packed into an in-memory .bmp and uploaded as linear data, as a normal map needs. The same drawing gives the colour: black enamel collars with the champion's name in raised gold letters, the medal's "9.58" and its ring of laurel leaves (filled with our scanline fill), the belt plates' engraved CHAMPION, stitched and grained leather, and the basketball's pebbles. The lathe runs its texture coordinates against the angle so that, with the shader's B = N x T, the engraving's up is up and it reads left to right.
- *The golden ball's panels.* Each point on the sphere belongs to the nearest of 32 centres: the 12 corners of an icosahedron (pentagons, in black enamel) and its 20 face centres (hexagons, in gold). Where the two nearest centres are almost equally near, the surface sinks into a groove; in the middle of a panel it swells slightly.
- *Leaves.* A laurel leaf is our own mesh: pointed at both ends, its edge cut into teeth, and folded along the midrib so the rib catches the light. Pairs of them follow the wreath's two branches and the basketball cradle's scrolls, which are tubes that taper as they curl into a shrinking spiral. The garnet berries and the belts' gems are spheres with the sharpest highlight in the room.
- *Our own meshes.* A torus (the five rings, the chandelier hoops, the wreath's branch, ball seams): a circle of radius R, with a tube of radius r swept round it, (R + r cos v)(cos u, sin u) and r sin v. A UV sphere. Octagonal belt plates (a polygon fan on each face, quads round the edge), rings of beads round rims and feet, and a buckle. A medal: two flat-mapped discs and a rim. Ribbons as two-sided strips. And a tube swept along any path, with a ring of vertices square to the path at each point, for the ropes, chandelier arms, cup handles and the basketball's cradle. Quads are wound to agree with their normals (using the cross product of the diagonals, which still works where a quad collapses to a triangle at a pole), so none is culled.
- *Curves.* Each stanchion rope is a quadratic Bezier from `Curves.cpp`, with its control point twice the sag below the middle, so the lowest point is exactly the sag. Sag grows with the rope's length. The chandelier arms are quadratic Beziers too.
- *Shading.* Gold is Blinn-Phong with a tinted albedo and a high specular, tightest on the trophies and crystal drops; the belts' leather has a soft sheen, the drum's velvet none. The rings lean 7 degrees alternately about the vertical, so each passes in front of one neighbour and behind the next. The prints are glossy, the labels and carpet matte (Lambert). Candle flames and picture lights are emissive, so they glow without lighting anything.
- *Animation.* The trophies stand on turntables: each frame their model matrices are rebuilt as T(home) x Ry(angle + phase) x S, so each turns from its own starting angle, scaled up 1.4 times. Walking in sets a cheer value that decays exponentially, so they spin up and settle.

## Tests

`tests/test_core.cpp` has 335 checks. None needs a window. The Checkpoint 1 and 2 checks:

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
- Pop art: Rec. 601 weights, the histogram stretch, posterise bands, Floyd-Steinberg keeps the average brightness, Bayer lights exactly half at 50%, halftone dots grow with brightness
