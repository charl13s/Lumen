// Room.h - THE FROZEN INTERFACE.
//
// ============================================================================
// This file is the contract between the five of us. Once it is agreed it does
// not change without the whole team agreeing, because every room is written
// against it. If you need something it does not give you, open an issue; do not
// edit this file on your own branch.
// ============================================================================
//
// WHAT A ROOM MAY DO
//   - describe geometry it wants built (walls furniture, sculpture, plinths)
//   - hang works at the hang points the plan gives it
//   - choose a shading mode per surface, from cfg::Shading
//   - run its own animation in update()
//   - react to the visitor arriving and leaving
//
// WHAT A ROOM MAY NOT DO
//   - call OpenGL. Not one function. The core draws; you describe.
//   - touch another room's files
//   - edit anything in src/core/
//   - load a texture directly: ask ctx.textures, so it is cached and gamma is right
//   - invent its own constants: put them in core/Config.h
//
// WHY SO STRICT
//   Five people editing shared rendering code produces a merge conflict a week
//   and five copies of every bug. With this split, your room is one file nobody
//   else opens, so your git history is clean, your contribution is unambiguous
//   (see the brief, section 4.1), and a room that is half finished in week 11
//   does not stop anybody else's room from working.

#pragma once
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Builders.h"
#include "Config.h"
#include "Gfx.h"
#include "Plan.h"

namespace lumen {

// Everything a room is allowed to reach. Passed in; never a global.
struct RoomContext {
    const Plan*        plan     = nullptr;
    const RoomPlan*    me       = nullptr;   // your own room's plan entry
    gfx::TextureCache* textures = nullptr;
    const std::string* assetDir = nullptr;   // prefix for asset paths

    // Convenience: load a colour texture from assets/, cached.
    const gfx::Texture* texture(const std::string& relativePath) const;
    // Convenience: the derived relief map for the same image.
    const gfx::Texture* relief(const std::string& relativePath) const;
};

// One thing to draw. A room returns a list of these from build().
struct Piece {
    gfx::MeshData  mesh;
    gfx::Material  material;
    glm::mat4      transform{1.0f};
    std::string    note;   // shows in the debug overlay; say WHY you chose the shading
};

// The interface every room implements. Five methods, four of them optional.
class Room {
public:
    virtual ~Room() = default;

    // Called once at startup. Return everything your room contains.
    // This is the only method you must implement.
    virtual std::vector<Piece> build(const RoomContext& ctx) = 0;

    // Called every frame, whether or not the visitor is inside. Keep it cheap.
    // dt is seconds. Return true if any piece's transform changed this frame.
    virtual bool update(float /*dt*/) { return false; }

    // Transforms for pieces that move. Indexed as build() returned them.
    // Only called when update() returned true.
    virtual void transforms(std::vector<glm::mat4>& /*out*/) {}

    // The visitor crossed your threshold / left it. The vault fade is handled
    // for you by LightingState; use these only for things specific to your room.
    virtual void onEnter() {}
    virtual void onExit()  {}

    // For the debug overlay and the final report: which techniques does your room
    // demonstrate, and why did you choose them? The brief asks each of us to
    // justify our techniques, so write this as you go rather than in week 13.
    virtual std::string techniqueNotes() const { return {}; }
};

using RoomPtr = std::unique_ptr<Room>;

// ---- registry --------------------------------------------------------------
//
// Adding a room is ONE line in Registry.cpp. Nothing else in the codebase needs
// to know your room exists.

RoomPtr makeRoom(const std::string& roomId);

} // namespace lumen
