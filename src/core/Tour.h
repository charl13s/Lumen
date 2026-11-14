// Tour.h - the guided tour, following a Catmull-Rom spline around the corridor.
//
// The spline rounds the corners but still passes through every waypoint.
// The camera moves by distance, not by t, so its speed stays constant.

#pragma once
#include <vector>

#include "Curves.h"
#include "Plan.h"

namespace lumen {

// Builds the tour path from the plan's waypoints, then pushes every point back
// inside the walls, because the spline bulges outward at corners.
// The minimap draws the same path.
std::vector<curves::P2> buildTourPath(const Plan& plan, int samplesPerSpan = 24);

class TourCamera {
public:
    // A closed loop, since the corridor is a ring.
    explicit TourCamera(const Plan& plan, int samplesPerSpan = 24);

    void  start();
    void  stop();
    bool  running() const { return running_; }

    // Call once per frame while running.
    void  update(float dt);

    Vec2  position() const { return position_; }
    float headingDeg() const { return heading_; }

    // The whole path, for the minimap.
    const std::vector<curves::P2>& path() const { return path_; }
    float totalLength() const;

private:
    std::vector<curves::P2> path_;
    std::vector<float>      lengths_;
    float travelled_ = 0.0f;
    float heading_   = 0.0f;
    Vec2  position_{0.0f, 0.0f};
    bool  running_   = false;

    void sampleAt(float distance);
};

} // namespace lumen
