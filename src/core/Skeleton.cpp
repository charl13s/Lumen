#include "Skeleton.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace lumen::skel {

int Skeleton::find(const std::string& name) const {
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].name == name) return static_cast<int>(i);
    return -1;
}

int Skeleton::jointOf(int node) const {
    for (std::size_t j = 0; j < joints.size(); ++j)
        if (joints[j] == node) return static_cast<int>(j);
    return -1;
}

int Skeleton::depth(int node) const {
    int d = 0;
    for (int p = nodes.at(static_cast<std::size_t>(node)).parent; p >= 0 && d < 256;
         p = nodes[static_cast<std::size_t>(p)].parent)
        ++d;
    return d;
}

Pose restPose(const Skeleton& sk) {
    Pose p;
    for (const Node& n : sk.nodes) {
        p.t.push_back(n.t);
        p.r.push_back(n.r);
        p.s.push_back(n.s);
    }
    return p;
}

std::vector<int> bindClip(const Clip& clip, const Skeleton& sk) {
    std::vector<int> out;
    out.reserve(clip.tracks.size());
    for (const Track& t : clip.tracks) out.push_back(sk.find(t.node));
    return out;
}

namespace {

glm::quat toQuat(const glm::vec4& v) { return glm::quat(v.w, v.x, v.y, v.z); }

// The two keys either side of time, and how far between them it is.
void bracket(const std::vector<float>& times, float time, std::size_t& a, std::size_t& b, float& f) {
    if (times.size() < 2 || time <= times.front()) { a = b = 0; f = 0.0f; return; }
    if (time >= times.back()) { a = b = times.size() - 1; f = 0.0f; return; }
    const auto it = std::upper_bound(times.begin(), times.end(), time);
    b = static_cast<std::size_t>(it - times.begin());
    a = b - 1;
    const float span = times[b] - times[a];
    f = span > 1e-6f ? (time - times[a]) / span : 0.0f;
}

// The hips: of the nodes a clip moves along, the one nearest the root.
int rootTrack(const Clip& clip, const Skeleton& sk, const std::vector<int>& binding) {
    int best = -1, bestDepth = 1 << 20;
    for (std::size_t i = 0; i < clip.tracks.size(); ++i) {
        if (clip.tracks[i].path != Path::Translation || binding[i] < 0) continue;
        if (sk.jointOf(binding[i]) < 0) continue;
        const int d = sk.depth(binding[i]);
        if (d < bestDepth) { bestDepth = d; best = static_cast<int>(i); }
    }
    return best;
}

} // namespace

void sample(const Clip& clip, const std::vector<int>& binding, float time,
            bool loop, Pose& pose) {
    if (clip.duration > 1e-6f) {
        if (loop) {
            time = std::fmod(time, clip.duration);
            if (time < 0.0f) time += clip.duration;
        } else {
            time = std::clamp(time, 0.0f, clip.duration);
        }
    }
    for (std::size_t i = 0; i < clip.tracks.size() && i < binding.size(); ++i) {
        const int node = binding[i];
        const Track& tr = clip.tracks[i];
        if (node < 0 || tr.values.empty()) continue;
        const std::size_t n = static_cast<std::size_t>(node);

        std::size_t a = 0, b = 0;
        float f = 0.0f;
        bracket(tr.times, time, a, b, f);
        if (tr.step) f = 0.0f;
        a = std::min(a, tr.values.size() - 1);
        b = std::min(b, tr.values.size() - 1);

        if (tr.path == Path::Rotation) {
            pose.r[n] = glm::normalize(glm::slerp(toQuat(tr.values[a]), toQuat(tr.values[b]), f));
        } else {
            const glm::vec3 v = glm::mix(glm::vec3(tr.values[a]), glm::vec3(tr.values[b]), f);
            if (tr.path == Path::Translation) pose.t[n] = v;
            else                              pose.s[n] = v;
        }
    }
}

Pose blend(const Pose& a, const Pose& b, float w) {
    Pose out = a;
    const std::size_t n = std::min(a.t.size(), b.t.size());
    for (std::size_t i = 0; i < n; ++i) {
        out.t[i] = glm::mix(a.t[i], b.t[i], w);
        out.r[i] = glm::normalize(glm::slerp(a.r[i], b.r[i], w));
        out.s[i] = glm::mix(a.s[i], b.s[i], w);
    }
    return out;
}

std::vector<glm::mat4> globals(const Skeleton& sk, const Pose& pose) {
    std::vector<glm::mat4> g(sk.nodes.size(), glm::mat4(1.0f));
    for (int i : sk.order) {
        const std::size_t n = static_cast<std::size_t>(i);
        const glm::mat4 local = glm::translate(glm::mat4(1.0f), pose.t[n]) *
                                glm::mat4_cast(pose.r[n]) *
                                glm::scale(glm::mat4(1.0f), pose.s[n]);
        const int p = sk.nodes[n].parent;
        g[n] = p >= 0 ? g[static_cast<std::size_t>(p)] * local : local;
    }
    return g;
}

std::vector<glm::mat4> jointMatrices(const Skeleton& sk, const std::vector<glm::mat4>& g) {
    std::vector<glm::mat4> out(sk.joints.size(), glm::mat4(1.0f));
    for (std::size_t j = 0; j < sk.joints.size(); ++j)
        out[j] = g[static_cast<std::size_t>(sk.joints[j])] * sk.inverseBind[j];
    return out;
}

int retarget(Clip& clip, const Skeleton& sk) {
    const std::vector<int> binding = bindClip(clip, sk);
    const int root = rootTrack(clip, sk, binding);

    std::vector<Track> kept;
    int found = 0;
    for (std::size_t i = 0; i < clip.tracks.size(); ++i) {
        Track& tr = clip.tracks[i];
        if (binding[i] < 0) continue;
        if (tr.path == Path::Scale) continue;
        if (tr.path == Path::Translation) {
            if (tr.values.empty()) continue;
            // A bone that only holds its place keeps this character's own length.
            float moves = 0.0f;
            for (const glm::vec4& v : tr.values)
                moves = std::max(moves, glm::length(glm::vec3(v - tr.values.front())));
            if (static_cast<int>(i) != root && moves < 1e-4f) continue;
            // Move the bone the same way, scaled to this character's proportions.
            const glm::vec3 rest = sk.nodes[static_cast<std::size_t>(binding[i])].t;
            const glm::vec3 first(tr.values.front());
            const float len = glm::length(first);
            const float k = len > 1e-6f ? glm::length(rest) / len : 1.0f;
            for (glm::vec4& v : tr.values)
                v = glm::vec4(rest + (glm::vec3(v) - first) * k, 0.0f);
        }
        kept.push_back(std::move(tr));
        ++found;
    }
    clip.tracks = std::move(kept);
    return found;
}

void keepInPlace(Clip& clip, const Skeleton& sk) {
    const std::vector<int> binding = bindClip(clip, sk);
    const int root = rootTrack(clip, sk, binding);
    if (root < 0) return;
    Track& tr = clip.tracks[static_cast<std::size_t>(root)];
    if (tr.values.empty()) return;

    // "Up" seen from the hips' parent, which may be turned or scaled.
    const int node = binding[static_cast<std::size_t>(root)];
    const int parent = sk.nodes[static_cast<std::size_t>(node)].parent;
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (parent >= 0) {
        const glm::mat4 pg = globals(sk, restPose(sk))[static_cast<std::size_t>(parent)];
        up = glm::inverse(glm::mat3(pg)) * up;
    }
    up = glm::normalize(up);

    // Keep the up and down bob; take the across from the first key.
    const glm::vec3 first(tr.values.front());
    const glm::vec3 across = first - glm::dot(first, up) * up;
    for (glm::vec4& v : tr.values) {
        const glm::vec3 p(v);
        v = glm::vec4(across + glm::dot(p, up) * up, 0.0f);
    }
}

} // namespace lumen::skel
