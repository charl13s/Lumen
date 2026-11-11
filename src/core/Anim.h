// Anim.h - the building blocks of animation. Owner: Charles.
//
// An animation is a picture drawn as a function of time. These helpers turn a
// time into the values a frame needs: in-between positions (interpolation),
// speeding up and slowing down (easing), and values set at chosen moments
// (keyframes).

#pragma once
#include <vector>

#include "Polygon.h"
#include "Raster.h"

namespace lumen::anim {

// Straight-line in-between: t = 0 gives a, t = 1 gives b.
float lerp(float a, float b, float t);
poly::PointF lerp(poly::PointF a, poly::PointF b, float t);
raster::Colour lerp(raster::Colour a, raster::Colour b, float t);

float clamp01(float t);

// Easing: starts slow, speeds up, ends slow (smoothstep, 3t^2 - 2t^3).
float easeInOut(float t);
// Starts fast and settles gently.
float easeOut(float t);

// A value that changes over time, set at a few keyframes and eased between them.
struct Key {
    float time;
    float value;
};

class Track {
public:
    explicit Track(std::vector<Key> keys);
    // Before the first key and after the last, holds that key's value.
    float at(float time) const;

private:
    std::vector<Key> keys_;
};

// The same time, as a fraction 0 to 1 of a loop of `period` seconds.
float loop(float time, float period);

} // namespace lumen::anim
