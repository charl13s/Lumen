// Skeleton.h - bones, animation clips and poses, for the torch bearer.
//
// A skeleton is a tree of nodes, each with a local move, turn and scale. A
// clip is a set of keyframed tracks that move those nodes over time. Sampling
// a clip gives a pose; multiplying down the tree gives each bone's transform,
// and the skin follows the bones. No OpenGL here, so it can be tested.

#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lumen::skel {

// One node of the model's tree. Joints are the nodes the skin follows.
struct Node {
    std::string name;
    int         parent = -1;
    glm::vec3   t{0.0f};
    glm::quat   r{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3   s{1.0f};
};

struct Skeleton {
    std::vector<Node>      nodes;
    std::vector<int>       order;        // every node, each parent before its children
    std::vector<int>       joints;       // skin joint i is node joints[i]
    std::vector<glm::mat4> inverseBind;  // one per joint: model space to bone space

    int find(const std::string& name) const;   // node index, or -1
    int jointOf(int node) const;               // joint index of a node, or -1
    int depth(int node) const;                 // 0 for a root
};

// A local transform for every node.
struct Pose {
    std::vector<glm::vec3> t;
    std::vector<glm::quat> r;
    std::vector<glm::vec3> s;
};

enum class Path { Translation, Rotation, Scale };

struct Track {
    std::string node;          // matched by name, so a clip from another file fits
    Path        path = Path::Rotation;
    bool        step = false;  // hold each key instead of blending
    std::vector<float>     times;
    std::vector<glm::vec4> values;   // x y z, or a quaternion as x y z w
};

struct Clip {
    std::string        name;
    float              duration = 0.0f;
    std::vector<Track> tracks;
};

Pose restPose(const Skeleton& sk);

// Which node each track moves in this skeleton, -1 if it has no such node.
std::vector<int> bindClip(const Clip& clip, const Skeleton& sk);

// Writes the clip at a time in seconds into the pose. A looping clip wraps
// round; otherwise the last frame is held.
void sample(const Clip& clip, const std::vector<int>& binding, float time,
            bool loop, Pose& pose);

// Mixes two poses: w = 0 gives a, w = 1 gives b. Turns use slerp.
Pose blend(const Pose& a, const Pose& b, float w);

// Each node's transform in model space.
std::vector<glm::mat4> globals(const Skeleton& sk, const Pose& pose);

// What the shader needs for each joint: its model transform times its inverse bind.
std::vector<glm::mat4> jointMatrices(const Skeleton& sk, const std::vector<glm::mat4>& globals);

// Fits a clip made on another character of the same rig to this skeleton.
// Turns are copied as they are. Moves are copied only for the root and for
// bones that really move (the hips bobbing as they walk), scaled to this
// character's proportions; every other bone keeps its own length. Returns how
// many tracks found a bone.
int retarget(Clip& clip, const Skeleton& sk);

// Takes the forward and sideways travel out of the hips, so a walk stays on
// the spot and the program moves the character instead.
void keepInPlace(Clip& clip, const Skeleton& sk);

} // namespace lumen::skel
