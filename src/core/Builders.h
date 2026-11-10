// Builders.h - shared geometry. Nobody writes their own box.
//
// These are the pieces every room needs. Five people building five rooms would
// otherwise produce five frame builders with five different bugs; here there is
// one, used about thirty-five times.
//
// buildFramedWork is the important one. Hand it a wall position and a texture and
// it produces the canvas, the moulding, the stand-off from the wall and the label
// plate. That is the whole of "hanging a painting" as far as a room file is
// concerned.
//
// Everything here is pure geometry on the CPU - it returns MeshData, not GL
// objects - so it can be unit tested without a window.

#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Config.h"
#include "Gfx.h"
#include "Plan.h"

namespace lumen::build {

// ---- the building ----------------------------------------------------------

// Extrudes the plan's wall segments to wallHeight. This is the week 4-5
// "polygon extrusion from a 2D plan" the proposal promised, and it is why the
// plan file is the single source of truth.
gfx::MeshData extrudeWalls(const Plan& plan);

// Floor slabs for the rooms, corridor, atrium and vestibule.
gfx::MeshData buildFloors(const Plan& plan);

// A Kimbell-style barrel vault over a room: a half-cylinder with a slot along
// its crown. The slot is where the light comes from, so the geometry and the
// lighting agree with each other by construction.
gfx::MeshData buildVault(const Segment& crown, float span, float apexHeight,
                         float springHeight, int segments = 24);

// All the vaults for one room, from its plan entry.
gfx::MeshData buildRoomVaults(const RoomPlan& room, float wallHeight, float apex);

// ---- furniture -------------------------------------------------------------

struct FramedWork {
    gfx::MeshData canvas;   // the painting itself, textured
    gfx::MeshData frame;    // the moulding around it
    gfx::MeshData plate;    // the wall label
    glm::vec3     centre{0.0f};
};

// Hangs a work at a hang point from the plan. widthM/heightM are the visible
// canvas size; the frame is added outside that.
FramedWork buildFramedWork(const HangPoint& hang, float widthM, float heightM,
                           float centreHeight = cfg::kHangCentreHeight);

// A plinth for sculpture. Sits in a corridor niche or on a room floor.
gfx::MeshData buildPlinth(const glm::vec3& centre, float width, float depth, float height);

// A box and a cylinder, because somebody always needs one.
gfx::MeshData buildBox(const glm::vec3& centre, const glm::vec3& size);
gfx::MeshData buildCylinder(const glm::vec3& base, float radius, float height,
                            int segments = 24);

} // namespace lumen::build
