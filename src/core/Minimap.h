// Minimap.h - the map in the corner, drawn pixel by pixel with our own rasteriser.
//
//   walls            lineThick (Bresenham), clipped with clipCohenSutherland
//   rooms            clipSutherlandHodgman, then fillPolygonScanline
//   tour route       lineBresenham
//   visitor, doors   circleMidpoint, circleFilled
//   oculus           ellipseMidpoint
//   lamp light       an ellipse polygon, rotated and moved by a 3x3 matrix

#pragma once
#include <string>
#include <vector>

#include "CanvasView.h"
#include "Config.h"
#include "Curves.h"
#include "Plan.h"
#include "Raster.h"

namespace lumen {

class Minimap {
public:
    Minimap(const Plan& plan, int pixels = cfg::kMinimapPx);
    ~Minimap();

    // Draws into the canvas. No OpenGL.
    void redraw(Vec2 playerPlanPos, float headingDeg, const std::string& currentRoomId);

    const raster::Canvas& canvas() const { return canvas_; }

    // Shows the canvas in the screen corner.
    void present(int screenW, int screenH);

private:
    const Plan*    plan_ = nullptr;
    raster::Canvas canvas_;
    float          scale_ = cfg::kMinimapScale;
    Vec2           origin_{0.0f, 0.0f};   // plan point at the canvas centre

    std::vector<curves::P2> tourPath_;   // worked out once, it never changes

    gfx::CanvasView view_;

    // plan metres to canvas pixels
    raster::Point toPx(Vec2 planPoint) const;
};

} // namespace lumen
