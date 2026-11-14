#include "Lighting.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace lumen {

void Lamp::follow(float eyeX, float eyeY, float eyeZ,
                  float fwdX, float fwdY, float fwdZ,
                  float rightX, float rightY, float rightZ,
                  float dt) {
    // The lamp is held a little right of and below the eye.
    posX = eyeX + rightX * cfg::kLampOffsetRight;
    posY = eyeY + rightY * cfg::kLampOffsetRight - cfg::kLampOffsetDown;
    posZ = eyeZ + rightZ * cfg::kLampOffsetRight;

    // Ease toward the view direction. Same lag at any frame rate.
    const float k = 1.0f - std::exp(-cfg::kLampLagHz * std::max(dt, 0.0f));
    dirX += (fwdX - dirX) * k;
    dirY += (fwdY - dirY) * k;
    dirZ += (fwdZ - dirZ) * k;

    const float len = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (len > 1e-5f) {
        dirX /= len;
        dirY /= len;
        dirZ /= len;
    }

    constexpr float kDeg2Rad = 3.14159265358979f / 180.0f;
    innerCos = std::cos(cfg::kLampInnerDeg * kDeg2Rad);
    outerCos = std::cos(cfg::kLampOuterDeg * kDeg2Rad);
}

glm::mat4 lampView(const Lamp& lamp) {
    const glm::vec3 pos(lamp.posX, lamp.posY, lamp.posZ);
    const glm::vec3 dir(lamp.dirX, lamp.dirY, lamp.dirZ);
    // Pick an up vector that is not parallel to the lamp.
    const glm::vec3 up = (std::fabs(dir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    return glm::lookAt(pos, pos + dir, up);
}

glm::mat4 lampProjection(const Lamp& lamp) {
    const float fov = glm::radians(2.0f * cfg::kLampOuterDeg + 4.0f);
    return glm::perspective(fov, 1.0f, cfg::kShadowNear, lamp.range);
}

LightingState::LightingState(const Plan& plan) : plan_(&plan) {
    vaults_.reserve(plan.rooms.size());
    for (const RoomPlan& r : plan.rooms)
        vaults_.push_back(VaultLight{r.id, 0.0f, 0.0f});
}

void LightingState::setRevealLevel(float level) {
    revealLevel_ = std::clamp(level, 0.0f, 1.0f);
}

void LightingState::update(Vec2 playerPlanPos, float dt) {
    // Which room are we in? The margin stops flicker in the doorway.
    const RoomPlan* inside = plan_ ? plan_->roomAt(playerPlanPos, cfg::kRevealMargin)
                                   : nullptr;
    current_ = inside ? inside->id : std::string{};

    for (VaultLight& v : vaults_) {
        v.target = (v.roomId == current_) ? revealLevel_ : 0.0f;

        // Fade out is quicker than fade in.
        const float seconds = (v.target > v.level) ? cfg::kRevealFadeIn
                                                   : cfg::kRevealFadeOut;

        // Base the step on the larger of the current and target level, not the
        // cap. With a cap of zero the step would be zero and the room would
        // never fade out.
        const float span = std::max(revealLevel_, v.level);
        const float step = (seconds > 1e-4f && span > 1e-5f) ? (span / seconds) * dt
                                                             : 1.0f;

        if (v.level < v.target)      v.level = std::min(v.target, v.level + step);
        else if (v.level > v.target) v.level = std::max(v.target, v.level - step);
    }
}

float LightingState::levelFor(const std::string& roomId) const {
    for (const VaultLight& v : vaults_)
        if (v.roomId == roomId) return v.level;
    return 0.0f;
}

} // namespace lumen
