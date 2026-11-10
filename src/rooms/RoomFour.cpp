// RoomFour.cpp - Gallery Four.  Owner: Erykah Diang'a (168661)
//
// Responsibility: geometry and the floor plan
//
// ============================================================================
// THIS IS YOUR FILE. Nobody else edits it, and you edit nothing outside it.
// Read src/rooms/RoomTwo.cpp first - it is the worked example with every part
// filled in. Then fill this one in the same shape.
// ============================================================================
//
// FOR THIS ROOM:
//   Your room demonstrates Gouraud shading DELIBERATELY. Per-vertex
//   lighting on large wall quads makes the lamp's highlight flicker and
//   vanish between vertices. Label it on the wall text and explain it in
//   techniqueNotes: side by side with the other rooms it is a visible,
//   examinable demonstration of a syllabus concept, not a bug.
//
// THE RULES (full version in src/core/Room.h):
//   - no OpenGL calls in this file, ever
//   - load textures through ctx.texture() / ctx.relief() so they are cached
//     once and the gamma is handled for you
//   - no magic numbers: add them to src/core/Config.h
//   - if you need something core/ does not provide, open an issue - do not
//     edit core/ on your branch
//
// This stub compiles and runs today. The room is simply empty until you fill
// in build(), so nobody is blocked waiting for you.

#include "core/Room.h"

#include <string>
#include <vector>

namespace lumen {
namespace {

class RoomFour final : public Room {
public:
    std::vector<Piece> build(const RoomContext& ctx) override {
        std::vector<Piece> pieces;
        if (!ctx.me) return pieces;

        // --------------------------------------------------------------------
        // TODO (Erykah Diang'a (168661))
        //
        // 1. Hang your works. ctx.me->hangs gives you {hangs} hang points from
        //    the plan - you choose what goes on them, not where they are.
        //
        //        for (const HangPoint& hang : ctx.me->hangs) {
        //            auto work = build::buildFramedWork(hang, 1.4f, 1.05f);
        //            Piece canvas;
        //            canvas.mesh = std::move(work.canvas);
        //            canvas.material.albedo  = ctx.texture("works/your-image.jpg");
        //            canvas.material.normal  = ctx.relief("works/your-image.jpg");
        //            canvas.material.shading = cfg::Shading::Gouraud;
        //            pieces.push_back(std::move(canvas));
        //        }
        //
        // 2. Add something with real depth - a plinth, a sculpture, a bench -
        //    so the vault wash has something to model and the lamp has something
        //    to rake across. build::buildPlinth, buildBox, buildCylinder.
        //
        // 3. Fill in techniqueNotes() below AS YOU GO.
        // --------------------------------------------------------------------

        (void)ctx;
        return pieces;
    }

    bool update(float dt) override {
        time_ += dt;
        return false;   // return true if any transform changed this frame
    }

    std::string techniqueNotes() const override {
        // TODO (Erykah Diang'a (168661)): which techniques does this room demonstrate, and WHY
        // did you choose each one over the alternatives? The brief asks every
        // member to justify this. Write it as you build, not in week 13.
        return "Gallery Four - not yet written up.\n";
    }

private:
    float time_ = 0.0f;
};

} // namespace

RoomPtr makeRoomFour() { return std::make_unique<RoomFour>(); }

} // namespace lumen
