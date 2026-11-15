# LUMEN

A walkable virtual art gallery, built for ICS 3204 Computer Graphics at Strathmore University.

## Description

LUMEN is a gallery you walk through in first person. The building is dark: a loop corridor around a central atrium, with five galleries off it, one built by each member. At the start a marble statue of a torch bearer comes to life, steps down from its plinth and hands you its torch. You carry that light through the gallery, and each room lights up when you walk in.

The 3D view is drawn with OpenGL. The minimap, the status strip and the algorithm pages are drawn by our own C++ code from Checkpoints 1 and 2, pixel by pixel, and shown on screen as textures.

## Group members and responsibilities

| Member | Responsible for | Work so far |
|---|---|---|
| Shawn Kuria | Integration, repository, main program | Build setup with vcpkg, the gallery's main loop (`main.cpp`), the list of rooms, Gallery Three; earlier the GIF writer and the pages program |
| Charles Murage | Camera, movement, collision | Camera, collision, the guided tour, the minimap, the torch bearer and the intro (`Camera.cpp`, `Collision.cpp`, `Tour.cpp`, `Minimap.cpp`, `Character.cpp`, `Intro.cpp`), Gallery Two; earlier the curves and animation helpers |
| Timothy Kimani | Lighting, shading, artwork | The shaders, textures and shadow map, the lamp and room lights, the statues and reliefs (`Gfx.cpp`, `Lighting.cpp`, `Sculpture.cpp`, `shaders/`), Gallery One; earlier the generated artwork |
| Erykah Diang'a | Geometry, floor plan | The building from the plan, the glTF model reader and the skeleton (`Builders.cpp`, `Model.cpp`, `Skeleton.cpp`, `Plan.cpp`), Gallery Four; earlier the 3D transformations |
| Priscillah Njoroge | Rasterisation algorithms, tests | Our 2D drawing shown in OpenGL (`CanvasView.cpp`), the tests and the algorithm notes, the floor plan with open niches, Gallery Five; earlier the algorithms and 2D transformations |

## Technologies and tools

- C++17
- CMake and vcpkg
- Visual Studio 2022 or newer (MSVC), Windows
- OpenGL 3.3, GLFW, glad, glm
- nlohmann/json, stb_image
- Python 3, for generating the floor plan

The libraries open the window, talk to the graphics card and read image files. The algorithms below are our own.

## Graphics techniques and algorithms implemented so far

| Topic | Implemented | Used for in the project |
|---|---|---|
| Lines | DDA, Bresenham (all eight octants), thick lines, polylines | Minimap walls and tour route, wireframes |
| Polygons | Area (shoelace), centroid, point in polygon, convex test | Room shapes, frame pieces |
| Clipping | Cohen-Sutherland, Sutherland-Hodgman | Minimap walls and route; room shapes and the lamp's light on the minimap |
| Filling | Scanline polygon fill, boundary fill | Rooms on the minimap |
| Circles and ellipses | Midpoint circle, midpoint ellipse | Visitor marker, doors, the oculus, the lamp's light |
| Curves | Quadratic and cubic Bezier, de Casteljau, Catmull-Rom, arc length | Vault ceilings, picture frame moulding, the guided tour |
| 2D transformations | 3x3 matrices, about any point or line, composite, inverse | The lamp's light turned to your heading on the minimap |
| 3D transformations | Model, view (look at) and perspective projection matrices | Every vertex in the gallery |
| Animation | Interpolation, easing, keyframes, arc length motion; skeletal animation with slerp, crossfades and skinning | The tour, the room lights fading, the torch bearer |
| Shading | Lambert, Blinn-Phong, Gouraud, normal mapping, emissive, triplanar | Walls, paintings, reliefs, statues |
| Lighting | Spotlight with falloff, hemisphere room light, tone mapping, sRGB | The carried lamp, the room lights, the oculus |
| Shadows | Shadow mapping from the lamp, with soft edges (PCF) | Everything the lamp lights |
| Textures | sRGB, mipmaps, anisotropic filtering, normal maps made with a Sobel filter | Paintings, reliefs, statues |

More detail is in `docs/ALGORITHMS.md`.

## How to compile and run

You need Visual Studio 2022 or newer with **Desktop development with C++**, and Git.

1. Install vcpkg, once per machine:

   ```bat
   git clone --depth 1 https://github.com/microsoft/vcpkg C:\dev\vcpkg
   C:\dev\vcpkg\bootstrap-vcpkg.bat
   ```

2. Build, from the project folder:

   ```bat
   cmake --preset default
   cmake --build --preset debug
   ```

   The first build downloads the libraries and takes 10 to 20 minutes.
   If you built Checkpoint 2 before, delete the `build` folder first.

3. Run, from the project folder:

   ```bat
   build\Debug\lumen.exe
   build\Debug\lumen_pages.exe
   build\Debug\lumen_tests.exe
   ```

   `lumen` is the gallery. `lumen_pages` saves the Checkpoint 1 and 2 pages and animations into `output/`. `lumen_tests` runs the unit tests.

**Controls:** WASD to walk, mouse to look, Shift to run, E to take the lamp, T for the guided tour, L to change the lighting, K to turn shadows on and off, F2 for the algorithm pages (arrows to turn them), F3 for the credits, Esc to free the mouse.

`lumen --no-intro` starts with the lamp already in hand. `lumen --shot out.bmp 28 29 0` saves a view from plan position (28, 29) facing north.

If `Could not find a package configuration file` appears, vcpkg is not at `C:\dev\vcpkg`: install it there, or change the path in `CMakePresets.json`. Run the programs from the project folder, not from `build\Debug`.

## Current progress

- The building is generated from the floor plan: walls with lintels, vault ceilings, the corridor ceiling, and the atrium with its open oculus.
- You can walk it with collision, or take the guided tour along a Catmull-Rom spline.
- The intro: a marble torch bearer comes to life, hands you the lamp, and turns back to stone.
- The lamp lights the gallery and casts shadows; each room lights up when you enter.
- Five museum statues stand in the atrium and the corridor niches, and five carved reliefs hang on the atrium walls.
- The minimap and the twelve algorithm pages are drawn by our own code and shown in the gallery.
- Gallery Two is complete with framed paintings.
- 309 unit tests pass.

## Known limitations

- Galleries One, Three, Four and Five are still empty.
- Gallery Two's paintings are placeholders we generated, not real artwork.
- The gallery opens in the bright Exhibition lighting. The intended darker look is one press of L away.
- Only tested on Windows with Visual Studio.
