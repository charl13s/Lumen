// Builders.h - shared geometry for the building and the rooms.
// Returns MeshData (plain CPU data), so it can be tested without a window.

#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Config.h"
#include "Gfx.h"
#include "Plan.h"

namespace lumen::build {

// ---- the building ----------------------------------------------------------

// Raises each 2D wall in the plan into a 3D wall, with a lintel over each doorway.
gfx::MeshData extrudeWalls(const Plan& plan);

// Floor slabs for the rooms, corridor, atrium and vestibule.
gfx::MeshData buildFloors(const Plan& plan);

// Flat ceilings over the corridor, vestibule and niches, at wall height.
gfx::MeshData buildCeilings(const Plan& plan);

// The atrium ceiling with the round oculus cut out of it, like the Pantheon,
// and a short drum of height rimHeight lining the opening.
gfx::MeshData buildOculusCeiling(const Plan& plan, float rimHeight, int segments = 96);

// A flat disc, facing up or down. The sky seen through the oculus.
gfx::MeshData buildDisc(const glm::vec3& centre, float radius, bool facingDown,
                        int segments = 64);

// A curved vault ceiling with a light slot along the top, like Kahn's Kimbell Museum.
// endWalls closes each end of the arch.
gfx::MeshData buildVault(const Segment& crown, float span, float apexHeight,
                         float springHeight, int segments = 24, bool endWalls = true);

// All the vaults for one room, from its plan entry, running wall to wall.
gfx::MeshData buildRoomVaults(const RoomPlan& room, float wallHeight, float apex,
                              float wallThickness = cfg::kWallThickness);

// ---- furniture -------------------------------------------------------------

struct FramedWork {
    gfx::MeshData canvas;   // the painting itself, textured
    gfx::MeshData frame;    // the moulding around it
    gfx::MeshData plate;    // the wall label
    glm::vec3     centre{0.0f};
};

// A painting with its frame and label, at a hang point from the plan.
// widthM and heightM are the canvas size; the frame goes around it.
FramedWork buildFramedWork(const HangPoint& hang, float widthM, float heightM,
                           float centreHeight = cfg::kHangCentreHeight);

// A plinth for sculpture.
gfx::MeshData buildPlinth(const glm::vec3& centre, float width, float depth, float height);

gfx::MeshData buildBox(const glm::vec3& centre, const glm::vec3& size);
gfx::MeshData buildCylinder(const glm::vec3& base, float radius, float height,
                            int segments = 24);
// A cone standing on its base, closed underneath.
gfx::MeshData buildCone(const glm::vec3& base, float radius, float height, int segments = 24);

} // namespace lumen::build
