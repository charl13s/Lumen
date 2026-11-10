#include "Anim.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace lumen::anim {

float lerp(float a, float b, float t) { return a + (b - a) * t; }

poly::PointF lerp(poly::PointF a, poly::PointF b, float t) {
    return poly::PointF{lerp(a.x, b.x, t), lerp(a.y, b.y, t)};
}

raster::Colour lerp(raster::Colour a, raster::Colour b, float t) {
    auto mix = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(lerp(static_cast<float>(x), static_cast<float>(y), t)));
    };
    return raster::Colour{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

float clamp01(float t) { return std::clamp(t, 0.0f, 1.0f); }

float easeInOut(float t) {
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

float easeOut(float t) {
    t = 1.0f - clamp01(t);
    return 1.0f - t * t * t;
}

Track::Track(std::vector<Key> keys) : keys_(std::move(keys)) {
    std::sort(keys_.begin(), keys_.end(), [](const Key& a, const Key& b) { return a.time < b.time; });
}

float Track::at(float time) const {
    if (keys_.empty()) return 0.0f;
    if (time <= keys_.front().time) return keys_.front().value;
    if (time >= keys_.back().time) return keys_.back().value;
    for (std::size_t i = 0; i + 1 < keys_.size(); ++i) {
        const Key& a = keys_[i];
        const Key& b = keys_[i + 1];
        if (time <= b.time) {
            const float span = b.time - a.time;
            const float t = span > 0.0f ? (time - a.time) / span : 1.0f;
            return lerp(a.value, b.value, easeInOut(t));
        }
    }
    return keys_.back().value;
}

float loop(float time, float period) {
    if (period <= 0.0f) return 0.0f;
    const float f = std::fmod(time, period) / period;
    return f < 0.0f ? f + 1.0f : f;
}

} // namespace lumen::anim
