// Collision.h - keeps the visitor inside the building.
//
// The player is a circle in plan (radius cfg::kPlayerRadius); every wall is a
// thick line segment from the plan. We resolve by projection: find the closest
// point on each wall, and if the player has got closer than radius + half the
// wall thickness, push them back out along the normal.
//
// Pushing out (rather than blocking the move) is what gives sliding along a wall
// instead of sticking, which matters a lot when someone is walking a 3 m corridor
// with a mouse in their hand.
//
// No OpenGL here. Tested in tests/test_core.cpp.

#pragma once
#include <vector>

#include "Plan.h"

namespace lumen {

class Collider {
public:
    // Builds from the plan's wall list. Doorways are already subtracted by the
    // plan generator, so an opening is simply an absence of segments.
    Collider(const std::vector<Segment>& walls, float wallThickness);

    // Returns the position the player actually ends up at after sliding.
    Vec2 resolve(Vec2 desired, float radius) const;

    // Straight-line visibility between two plan points, ignoring wall thickness.
    // Used by the reveal logic and (later) by anything that needs line of sight.
    bool segmentBlocked(Vec2 a, Vec2 b) const;

    std::size_t wallCount() const { return walls_.size(); }

private:
    std::vector<Segment> walls_;
    float half_ = 0.225f;   // half the wall thickness

    static Vec2 closestPointOnSegment(const Segment& s, Vec2 p);
};

} // namespace lumen
