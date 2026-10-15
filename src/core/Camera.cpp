#include "Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace lumen {
namespace {
constexpr float kDeg2Rad = 3.14159265358979f / 180.0f;
}

Camera::Camera(Vec2 startPlanPos, float headingDeg)
    : plan_(startPlanPos), yaw_(headingDeg) {
    rebuildBasis();
}

void Camera::rebuildBasis() {
    // yaw 0 looks north, which is -Z in world space (see Plan.h for the mapping).
    const float y = yaw_ * kDeg2Rad;
    const float p = pitch_ * kDeg2Rad;

    forward_ = glm::normalize(glm::vec3(std::sin(y) * std::cos(p),
                                        std::sin(p),
                                        -std::cos(y) * std::cos(p)));
    // World up is fixed; we never roll, so right is forward x up.
    right_ = glm::normalize(glm::cross(forward_, glm::vec3(0.0f, 1.0f, 0.0f)));
    up_    = glm::normalize(glm::cross(right_, forward_));
}

void Camera::update(const MoveInput& in, const Collider& collider, float dt) {
    // --- look ---
    yaw_   += in.mouseDX * cfg::kMouseSens;
    pitch_ -= in.mouseDY * cfg::kMouseSens;
    pitch_  = std::clamp(pitch_, -85.0f, 85.0f);
    if (yaw_ >= 360.0f) yaw_ -= 360.0f;
    if (yaw_ < 0.0f)    yaw_ += 360.0f;
    rebuildBasis();

    // --- move ---
    // Movement is planar: looking up must not lift the visitor off the floor.
    glm::vec3 flatFwd(forward_.x, 0.0f, forward_.z);
    const float fl = glm::length(flatFwd);
    if (fl > 1e-5f) flatFwd /= fl;

    glm::vec3 wish(0.0f);
    if (in.forward) wish += flatFwd;
    if (in.back)    wish -= flatFwd;
    if (in.right)   wish += right_;
    if (in.left)    wish -= right_;

    const float wl = glm::length(wish);
    if (wl > 1e-5f) {
        wish /= wl;   // normalise so diagonal movement is not faster
        const float speed = in.run ? cfg::kRunSpeed : cfg::kWalkSpeed;

        // World -> plan: +X is +x, -Z is +y.
        const Vec2 desired{plan_.x + wish.x * speed * dt,
                           plan_.y - wish.z * speed * dt};
        plan_ = collider.resolve(desired, cfg::kPlayerRadius);
    }
}

glm::vec3 Camera::eye() const {
    return glm::vec3(worldX(plan_.x), cfg::kEyeHeight, worldZ(plan_.y));
}

glm::mat4 Camera::view() const {
    const glm::vec3 e = eye();
    return glm::lookAt(e, e + forward_, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::projection(float aspect) const {
    if (aspect < 1e-4f) aspect = 1.0f;
    return glm::perspective(glm::radians(cfg::kFovDegrees), aspect,
                            cfg::kNearPlane, cfg::kFarPlane);
}

void Camera::teleport(Vec2 planPos, float headingDeg) {
    plan_ = planPos;
    yaw_  = headingDeg;
    rebuildBasis();
}

} // namespace lumen
