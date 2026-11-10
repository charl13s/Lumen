// Minimap.h - drawn by hand, on the CPU, with our own rasteriser.
//
// This is where the week 4 and 5 algorithms do real work in LUMEN. The 3D view
// is rasterised by the GPU; the minimap is rasterised by us:
//
//   walls          lineBresenham  (thickened by parallel runs)
//   room outlines  fillPolygonScanline
//   current room   fillBoundary
//   visitor, doors circleMidpoint / circleFilled
//   viewport edge  clipCohenSutherland
//
// The Canvas is produced entirely on the CPU and uploaded as one texture per
// frame, so the drawing logic can be tested without a window - and so that the
// answer to "where does your project use Bresenham?" is a file, not a shrug.

#pragma once
#include <string>

#include "Config.h"
#include "Plan.h"
#include "Raster.h"

namespace lumen {

class Minimap {
public:
    Minimap(const Plan& plan, int pixels = cfg::kMinimapPx);
    ~Minimap();

    // Rasterises into the internal canvas. No OpenGL - call it from a test.
    void redraw(Vec2 playerPlanPos, float headingDeg, const std::string& currentRoomId);

    const raster::Canvas& canvas() const { return canvas_; }

    // Uploads the canvas and draws it in the screen corner. Needs a GL context.
    void present(int screenW, int screenH);

private:
    const Plan*    plan_ = nullptr;
    raster::Canvas canvas_;
    float          scale_ = cfg::kMinimapScale;
    Vec2           origin_{0.0f, 0.0f};   // plan point at the canvas centre

    unsigned texture_ = 0;
    unsigned vao_ = 0, vbo_ = 0;
    unsigned progId_ = 0;
    bool     glReady_ = false;

    void ensureGL();

    // plan metres -> canvas pixels
    raster::Point toPx(Vec2 planPoint) const;
};

} // namespace lumen
