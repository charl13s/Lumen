#include "Minimap.h"

#include <cmath>
#include <vector>

#include "Polygon.h"
#include "Transform.h"
#include "Tour.h"

namespace lumen {
namespace {

using raster::Colour;
using raster::Point;

// Colours: dark ground, cream walls, gold accents.
const Colour kGround {20, 17, 13, 210};
const Colour kWall   {230, 214, 164, 255};
const Colour kRoom   {44, 37, 24, 255};
const Colour kLitRoom{95, 78, 40, 255};
const Colour kVisitor{242, 212, 121, 255};
const Colour kDoor   {150, 132, 86, 255};
const Colour kEdge   {120, 108, 78, 255};
const Colour kLampPool{88, 76, 44, 255};
const Colour kTour   {88, 116, 104, 255};
const Colour kOculus {112, 100, 66, 255};

constexpr float kDeg2Rad = 3.14159265358979f / 180.0f;

} // namespace

Minimap::Minimap(const Plan& plan, int pixels)
    : plan_(&plan), canvas_(pixels, pixels) {
    // Same path the tour camera follows.
    tourPath_ = buildTourPath(plan, 18);
}

Minimap::~Minimap() = default;

Point Minimap::toPx(Vec2 p) const {
    // Canvas y points down, so flip north.
    const float half = canvas_.width() * 0.5f;
    return Point{
        static_cast<int>(std::lround(half + (p.x - origin_.x) * scale_)),
        static_cast<int>(std::lround(half - (p.y - origin_.y) * scale_)),
    };
}

void Minimap::redraw(Vec2 player, float headingDeg, const std::string& currentRoomId) {
    if (!plan_) return;
    origin_ = player;                 // the map moves, the visitor stays centred
    canvas_.clear(kGround);

    const int n = canvas_.width();
    const int c = n / 2;                  // centre of the map
    const float vxMin = 1.0f, vyMin = 1.0f;
    const float vxMax = static_cast<float>(n - 2);
    const float vyMax = static_cast<float>(n - 2);

    // --- room footprints: clipped with Sutherland-Hodgman, then scanline filled
    auto fillRect = [&](const Rect& r, Colour col) {
        const Point a = toPx(Vec2{r.x0, r.y0});
        const Point b = toPx(Vec2{r.x1, r.y1});
        const poly::Polygon shape = poly::rectangle(
            static_cast<float>(a.x), static_cast<float>(a.y),
            static_cast<float>(b.x), static_cast<float>(b.y));
        const poly::Polygon visible =
            poly::clipSutherlandHodgman(shape, 0.0f, 0.0f, vxMax + 1.0f, vyMax + 1.0f);
        if (!visible.empty())
            raster::fillPolygonScanline(canvas_, poly::toPixels(visible), col);
    };

    for (const RoomPlan& r : plan_->rooms)
        fillRect(r.rect, (r.id == currentRoomId) ? kLitRoom : kRoom);

    // The corridor and atrium.
    fillRect(plan_->corridorOuter, kRoom);
    fillRect(plan_->corridorInner, kGround);

    // --- walls, clipped then drawn with Bresenham -------------------------
    for (const Segment& s : plan_->walls) {
        const Point a = toPx(s.a);
        const Point b = toPx(s.b);

        float x0 = static_cast<float>(a.x), y0 = static_cast<float>(a.y);
        float x1 = static_cast<float>(b.x), y1 = static_cast<float>(b.y);

        // Clip to the map first, so off-map walls are never drawn.
        if (!raster::clipCohenSutherland(x0, y0, x1, y1, vxMin, vyMin, vxMax, vyMax))
            continue;

        raster::lineThick(canvas_,
                          static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0)),
                          static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)),
                          2, kWall);
    }

    // --- the tour route, as a Catmull-Rom spline --------------------------
    // Drawn as clipped Bresenham lines between the spline points.
    if (!tourPath_.empty()) {
        std::vector<Point> px;
        px.reserve(tourPath_.size());
        for (const curves::P2& p : tourPath_)
            px.push_back(toPx(Vec2{p.x, p.y}));

        for (std::size_t i = 0; i + 1 < px.size(); ++i) {
            float x0 = static_cast<float>(px[i].x),     y0 = static_cast<float>(px[i].y);
            float x1 = static_cast<float>(px[i + 1].x), y1 = static_cast<float>(px[i + 1].y);
            if (!raster::clipCohenSutherland(x0, y0, x1, y1, vxMin, vyMin, vxMax, vyMax))
                continue;
            raster::lineBresenham(canvas_,
                                  static_cast<int>(std::lround(x0)),
                                  static_cast<int>(std::lround(y0)),
                                  static_cast<int>(std::lround(x1)),
                                  static_cast<int>(std::lround(y1)), kTour);
        }
    }

    // --- the atrium oculus, as an ellipse ---------------------------------
    // Drawn with the ellipse routine in case the x and y scales ever differ.
    {
        const Point o = toPx(plan_->oculusCentre);
        const int orx = static_cast<int>(std::lround(plan_->oculusRadius * scale_));
        const int ory = static_cast<int>(std::lround(plan_->oculusRadius * scale_));
        raster::ellipseMidpoint(canvas_, o.x, o.y, orx, ory, kOculus);
    }

    // --- the lamp's reach on the floor ------------------------------------
    // The lamp's light on the floor is an ellipse stretched the way you face.
    // Build it pointing north, rotate it to the heading and move it ahead of the
    // visitor with one composite matrix, then clip it to the map.
    {
        const float h = headingDeg * kDeg2Rad;
        const float reach = cfg::kLampRange * 0.30f * scale_;
        const float waist = reach * 0.45f;
        const poly::PointF ahead{c + std::sin(h) * reach * 0.6f,
                                 c - std::cos(h) * reach * 0.6f};
        const xf::Mat3 place = xf::translation(ahead.x, ahead.y) * xf::rotation(h);
        const poly::Polygon pool = xf::transform(
            poly::ellipse(poly::PointF{0.0f, 0.0f}, waist, reach, 40), place);
        const poly::Polygon visible =
            poly::clipSutherlandHodgman(pool, vxMin, vyMin, vxMax, vyMax);
        if (!visible.empty())
            raster::polyline(canvas_, poly::toPixels(visible), 1, kLampPool, true);
    }

    // --- doorways, as small circles --------------------------------------
    for (const RoomPlan& r : plan_->rooms) {
        const Point d = toPx(r.door.centre(r.rect));
        raster::circleFilled(canvas_, d.x, d.y, 2, kDoor);
    }

    // --- the visitor -------------------------------------------------------
    raster::circleFilled(canvas_, c, c, 4, kVisitor);
    raster::circleMidpoint(canvas_, c, c, 7, kVisitor);

    // A short line showing which way they face. North is up.
    const float h = headingDeg * kDeg2Rad;
    const int tipX = c + static_cast<int>(std::lround(std::sin(h) * 14.0f));
    const int tipY = c - static_cast<int>(std::lround(std::cos(h) * 14.0f));
    raster::lineThick(canvas_, c, c, tipX, tipY, 2, kVisitor);

    // --- border ------------------------------------------------------------
    raster::lineBresenham(canvas_, 0, 0, n - 1, 0, kEdge);
    raster::lineBresenham(canvas_, n - 1, 0, n - 1, n - 1, kEdge);
    raster::lineBresenham(canvas_, n - 1, n - 1, 0, n - 1, kEdge);
    raster::lineBresenham(canvas_, 0, n - 1, 0, 0, kEdge);
}

void Minimap::present(int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0) return;

    // Bottom right corner.
    const int m = static_cast<int>(cfg::kMinimapMargin);
    view_.draw(canvas_, screenW, screenH,
               screenW - canvas_.width() - m, m,
               canvas_.width(), canvas_.height(),
               cfg::kMinimapOpacity);
}

} // namespace lumen
