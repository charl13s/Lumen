#include "Tour.h"

#include <cmath>

#include "Collision.h"
#include "Config.h"

namespace lumen {
namespace {
constexpr float kRad2Deg = 180.0f / 3.14159265358979f;
}

std::vector<curves::P2> buildTourPath(const Plan& plan, int samplesPerSpan) {
    std::vector<curves::P2> waypoints;
    waypoints.reserve(plan.route.size());
    for (const Vec2& v : plan.route)
        waypoints.push_back(curves::P2{v.x, v.y});

    // Drop a repeated last point; a zero length span would divide by zero.
    if (waypoints.size() > 2) {
        const curves::P2& f = waypoints.front();
        const curves::P2& b = waypoints.back();
        if (std::fabs(f.x - b.x) < 1e-3f && std::fabs(f.y - b.y) < 1e-3f)
            waypoints.pop_back();
    }
    if (waypoints.size() < 3) return waypoints;

    // Add a point every 2.5 m along each straight. With only the corners, the
    // spline swung about 1.9 m wide in a 3 m corridor.
    constexpr float kWaypointSpacing = 2.5f;   // metres
    std::vector<curves::P2> dense;
    const std::size_t wn = waypoints.size();
    for (std::size_t i = 0; i < wn; ++i) {
        const curves::P2& a = waypoints[i];
        const curves::P2& b = waypoints[(i + 1) % wn];
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        const int steps = std::max(1, static_cast<int>(len / kWaypointSpacing));
        for (int s = 0; s < steps; ++s) {
            const float t = static_cast<float>(s) / steps;
            dense.push_back(curves::P2{a.x + dx * t, a.y + dy * t});
        }
    }

    // Centripetal (alpha 0.5).
    std::vector<curves::P2> path =
        curves::sampleCatmullRomLoop(dense, std::max(2, samplesPerSpan / 4), 0.5f);

    // Push every point back inside the walls, with a bit more clearance than the player.
    const Collider collider(plan.walls, plan.wallThickness);
    const float clearance = cfg::kPlayerRadius * 1.6f;
    for (curves::P2& p : path) {
        const Vec2 fixed = collider.resolve(Vec2{p.x, p.y}, clearance);
        p.x = fixed.x;
        p.y = fixed.y;
    }
    return path;
}

TourCamera::TourCamera(const Plan& plan, int samplesPerSpan) {
    path_ = buildTourPath(plan, samplesPerSpan);
    lengths_ = curves::arcLengths(path_);

    if (!path_.empty()) {
        position_ = Vec2{path_.front().x, path_.front().y};
        sampleAt(0.0f);
    }
}

float TourCamera::totalLength() const {
    return lengths_.empty() ? 0.0f : lengths_.back();
}

void TourCamera::start() {
    running_ = true;
}

void TourCamera::stop() {
    running_ = false;
}

void TourCamera::sampleAt(float distance) {
    if (path_.size() < 2) return;

    const curves::P2 here = curves::pointAtDistance(path_, lengths_, distance);
    // Look a little way ahead so the heading is smooth.
    const curves::P2 ahead = curves::pointAtDistance(path_, lengths_,
                                                     distance + 0.6f);

    position_ = Vec2{here.x, here.y};

    const float dx = ahead.x - here.x;
    const float dy = ahead.y - here.y;
    if (std::fabs(dx) > 1e-5f || std::fabs(dy) > 1e-5f) {
        // Heading 0 is north (+y), measured toward +x, as in Camera.cpp.
        heading_ = std::atan2(dx, dy) * kRad2Deg;
        if (heading_ < 0.0f) heading_ += 360.0f;
    }
}

void TourCamera::update(float dt) {
    if (!running_ || path_.size() < 2) return;

    // Move by distance, not by t, for a constant speed.
    travelled_ += cfg::kWalkSpeed * dt;
    const float total = totalLength();
    if (total > 1e-4f && travelled_ > total) travelled_ -= total;

    sampleAt(travelled_);
}

} // namespace lumen
