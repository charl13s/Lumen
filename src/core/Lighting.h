// Lighting.h - the gallery's two light sources and the reveal that connects them.
//
// THE CONCEPT, stated once so it does not drift:
//
//   Nothing in LUMEN is lit by default. There are no lights on when you arrive.
//   The visitor is the only cause of light in the building:
//     - directly, through the lamp they carry (hard, raking, moves with you)
//     - indirectly, by entering a room, which wakes that room's vault slot
//       (soft, from above, bounced off the ceiling in the manner of Kahn's
//        Kimbell Art Museum)
//
//   The two show you different things, which is the point. The vault wash gives
//   you a painting's composition from across the room; the lamp gives you its
//   surface when you walk up to it. See docs/LIGHTING.md.
//
// The reveal never reaches full brightness - cfg::kRevealLevel caps it. That one
// float is the dial between "pure lamp, maximum atmosphere, cannot see the art"
// and "ordinary gallery lighting, concept gone".
//
// This file is pure state: no OpenGL. Tested in tests/test_core.cpp.

#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "Config.h"
#include "Plan.h"

namespace lumen {

// The cone the visitor carries. Deliberately a spotlight and not a point light:
// a cone is the shape a camera already sees, so a shadow pass is ONE render.
// A bare bulb would need six, as a cube map. See docs/LIGHTING.md.
struct Lamp {
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;   // world
    float dirX = 0.0f, dirY = 0.0f, dirZ = -1.0f;  // world, unit
    float innerCos = 0.0f, outerCos = 0.0f;        // cosines, precomputed for the shader
    float intensity = cfg::kLampIntensity;
    float range     = cfg::kLampRange;

    // Follows the camera with a little rotational lag, so it reads as something
    // held in a hand rather than welded to the eye.
    void follow(float eyeX, float eyeY, float eyeZ,
                float fwdX, float fwdY, float fwdZ,
                float rightX, float rightY, float rightZ,
                float dt);
};

// One room's vault slot, fading in and out as the visitor crosses the threshold.
struct VaultLight {
    std::string roomId;
    float level  = 0.0f;   // current, 0 .. cfg::kRevealLevel
    float target = 0.0f;   // where it is heading
    bool  on() const { return level > 0.001f; }
};

class LightingState {
public:
    explicit LightingState(const Plan& plan);

    // Call once per frame with the visitor's plan position. Handles the
    // threshold test and advances every fade.
    void update(Vec2 playerPlanPos, float dt);

    float levelFor(const std::string& roomId) const;
    const std::string& currentRoomId() const { return current_; }   // "" in the corridor
    const std::vector<VaultLight>& vaults() const { return vaults_; }

    Lamp lamp;

private:
    const Plan*             plan_ = nullptr;
    std::vector<VaultLight> vaults_;
    std::string             current_;
};

} // namespace lumen
