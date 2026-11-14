// Room.h - what every gallery builds on.
//
// Each gallery is one file in src/rooms/, owned by one member, who decides
// what goes in it. A room can:
//   hang paintings, with any images in assets/ (build::buildFramedWork)
//   build any shape, from the build:: helpers or its own triangles
//   load 3D models (ctx.model) and show art drawn by our own code (ctx.canvas)
//   choose each surface's material and shading mode
//   move things every frame (update() and transforms())
//   react when the visitor walks in or out (onEnter, onExit)
//   keep its own constants at the top of its own file
//
// Rooms do not call OpenGL themselves; the program draws what build() returns.
// Change this interface or anything else in src/core/ only with the group.

#pragma once
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Builders.h"
#include "Config.h"
#include "Gfx.h"
#include "Plan.h"
#include "Raster.h"

namespace lumen {

// One thing to draw. build() returns a list of these.
struct Piece {
    gfx::MeshData  mesh;
    gfx::Material  material;
    glm::mat4      transform{1.0f};
    std::string    note;   // shown in the debug overlay
};

// Everything a room can use.
struct RoomContext {
    const Plan*        plan     = nullptr;
    const RoomPlan*    me       = nullptr;   // this room's entry in the plan
    gfx::TextureCache* textures = nullptr;
    const std::string* assetDir = nullptr;   // prefix for asset paths

    // Loads a colour texture from assets/, cached.
    const gfx::Texture* texture(const std::string& relativePath) const;
    // The normal map made from the same image.
    const gfx::Texture* relief(const std::string& relativePath) const;

    // A texture drawn by our own 2D code, such as generated art. The same key
    // gives back the same texture, so draw each one once, in build().
    const gfx::Texture* canvas(const std::string& key, const raster::Canvas& image) const;

    // A 3D model (.gltf or .glb) from assets/, made `height` metres tall and
    // stood at a plan position, facing a compass heading (0 north, 90 east),
    // lifted by `elevation` (to stand it on a plinth). finish is "scan" for the
    // model's own textures, or "marble" or "bronze". One Piece per material;
    // empty, with a message, if the file cannot be read.
    std::vector<Piece> model(const std::string& relativePath, Vec2 at, float headingDeg,
                             float height, float elevation = 0.0f,
                             const std::string& finish = "scan") const;
};

class Room {
public:
    virtual ~Room() = default;

    // Called once at startup. Return everything in the room. Required.
    virtual std::vector<Piece> build(const RoomContext& ctx) = 0;

    // Called every frame. dt is in seconds. Return true if anything moved.
    virtual bool update(float /*dt*/) { return false; }

    // Transforms for moving pieces, in the order build() returned them.
    // Only called when update() returned true.
    virtual void transforms(std::vector<glm::mat4>& /*out*/) {}

    // The visitor entered or left. The room light fade is already handled.
    virtual void onEnter() {}
    virtual void onExit()  {}

    // Which techniques this room uses, printed with F1.
    virtual std::string techniqueNotes() const { return {}; }
};

using RoomPtr = std::unique_ptr<Room>;

// Rooms are registered in Registry.cpp.
RoomPtr makeRoom(const std::string& roomId);

} // namespace lumen
