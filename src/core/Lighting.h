// Lighting.h - the carried lamp and the room lights. Owner: Timothy.
//
// The gallery starts dark. The lamp lights what is in front of you, and each
// room's vault light fades up when you walk in. See docs/ALGORITHMS.md.
// No OpenGL here, so it can be tested.

#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Config.h"
#include "Plan.h"

namespace lumen {

// The carried lamp. A cone rather than a bulb, so shadows need one render, not six.
struct Lamp {
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;   // world
    float dirX = 0.0f, dirY = 0.0f, dirZ = -1.0f;  // world, unit
    float innerCos = 0.0f, outerCos = 0.0f;        // cone edges, as cosines
    float intensity = cfg::kLampIntensity;
    float range     = cfg::kLampRange;

    // Follows the camera with a slight lag, so it feels hand held.
    void follow(float eyeX, float eyeY, float eyeZ,
                float fwdX, float fwdY, float fwdZ,
                float rightX, float rightY, float rightZ,
                float dt);
};

// What the lamp sees: view and projection for the shadow map. The cone fits
// inside the projection, with a little margin.
glm::mat4 lampView(const Lamp& lamp);
glm::mat4 lampProjection(const Lamp& lamp);

// One room's light, fading in and out as the visitor enters and leaves.
struct VaultLight {
    std::string roomId;
    float level  = 0.0f;   // current, 0 .. cfg::kRevealLevel
    float target = 0.0f;   // where it is heading
    bool  on() const { return level > 0.001f; }
};

class LightingState {
public:
    explicit LightingState(const Plan& plan);

    // Call once per frame. Works out which room the visitor is in and moves every fade.
    void update(Vec2 playerPlanPos, float dt);

    // How bright a room gets. Changed by the L key.
    void  setRevealLevel(float level);
    float revealLevel() const { return revealLevel_; }

    float levelFor(const std::string& roomId) const;
    const std::string& currentRoomId() const { return current_; }   // "" in the corridor
    const std::vector<VaultLight>& vaults() const { return vaults_; }

    Lamp lamp;

private:
    const Plan*             plan_ = nullptr;
    std::vector<VaultLight> vaults_;
    std::string             current_;
    float                   revealLevel_ = cfg::kRevealLevel;
};

} // namespace lumen
