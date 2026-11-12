// Collision.h - keeps the visitor out of the walls. Owner: Charles.
//
// The visitor is a circle and each wall is a thick line. If the visitor gets too
// close to a wall, they are pushed back out. Pushing out instead of stopping the
// move lets you slide along walls.

#pragma once
#include <vector>

#include "Plan.h"

namespace lumen {

class Collider {
public:
    // Doorways are already gaps in the plan's wall list.
    Collider(const std::vector<Segment>& walls, float wallThickness);

    // Where the visitor ends up after being pushed out of walls.
    Vec2 resolve(Vec2 desired, float radius) const;

    // True if a wall crosses the line from a to b.
    bool segmentBlocked(Vec2 a, Vec2 b) const;

    std::size_t wallCount() const { return walls_.size(); }

private:
    std::vector<Segment> walls_;
    float half_ = 0.225f;   // half the wall thickness

    static Vec2 closestPointOnSegment(const Segment& s, Vec2 p);
};

} // namespace lumen
