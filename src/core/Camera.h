// Camera.h - the visitor. First-person, walks at gallery pace, slides along walls.
//
// Owned by Charles (camera, movement and collision). If you need the camera to do
// something new, that is a core change: open an issue rather than editing this
// from a room file.

#pragma once
#include <glm/glm.hpp>

#include "Collision.h"
#include "Config.h"
#include "Plan.h"

namespace lumen {

struct MoveInput {
    bool forward = false, back = false, left = false, right = false;
    bool run = false;
    float mouseDX = 0.0f, mouseDY = 0.0f;   // pixels since last frame
};

class Camera {
public:
    Camera(Vec2 startPlanPos, float headingDeg);

    // Applies input, then asks the collider where the visitor actually ends up.
    void update(const MoveInput& in, const Collider& collider, float dt);

    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;

    glm::vec3 eye()     const;
    glm::vec3 forward() const { return forward_; }
    glm::vec3 right()   const { return right_; }
    glm::vec3 up()      const { return up_; }

    Vec2  planPos() const { return plan_; }
    float headingDeg() const { return yaw_; }

    // Used by the tour camera to take over and hand back.
    void teleport(Vec2 planPos, float headingDeg);

private:
    Vec2  plan_;
    float yaw_   = 0.0f;    // degrees, 0 = north (-Z), 90 = east (+X)
    float pitch_ = 0.0f;    // degrees, clamped to avoid gimbal flip
    glm::vec3 forward_{0.0f, 0.0f, -1.0f};
    glm::vec3 right_{1.0f, 0.0f, 0.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};

    void rebuildBasis();
};

} // namespace lumen
