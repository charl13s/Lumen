#include "Lighting.h"

#include <algorithm>
#include <cmath>

namespace lumen {

void Lamp::follow(float eyeX, float eyeY, float eyeZ,
                  float fwdX, float fwdY, float fwdZ,
                  float rightX, float rightY, float rightZ,
                  float dt) {
    // The lamp sits slightly to the right of, and below, the eye. The right
    // vector is used in full (not just x and z) so the offset stays correct if
    // the camera ever rolls or looks steeply up or down.
    posX = eyeX + rightX * cfg::kLampOffsetRight;
    posY = eyeY + rightY * cfg::kLampOffsetRight - cfg::kLampOffsetDown;
    posZ = eyeZ + rightZ * cfg::kLampOffsetRight;

    // Exponential smoothing toward the view direction. Frame-rate independent:
    // the same lag at 30 fps and 144 fps.
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

LightingState::LightingState(const Plan& plan) : plan_(&plan) {
    vaults_.reserve(plan.rooms.size());
    for (const RoomPlan& r : plan.rooms)
        vaults_.push_back(VaultLight{r.id, 0.0f, 0.0f});
}

void LightingState::update(Vec2 playerPlanPos, float dt) {
    // Which room are we in? The margin means the light does not flicker while
    // the visitor is standing exactly on the sill.
    const RoomPlan* inside = plan_ ? plan_->roomAt(playerPlanPos, cfg::kRevealMargin)
                                   : nullptr;
    current_ = inside ? inside->id : std::string{};

    for (VaultLight& v : vaults_) {
        v.target = (v.roomId == current_) ? cfg::kRevealLevel : 0.0f;

        // Fading out is quicker than fading in: the room should feel like it is
        // waking slowly and letting go promptly behind you.
        const float seconds = (v.target > v.level) ? cfg::kRevealFadeIn
                                                   : cfg::kRevealFadeOut;
        const float step = (seconds > 1e-4f) ? (cfg::kRevealLevel / seconds) * dt
                                             : cfg::kRevealLevel;

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
