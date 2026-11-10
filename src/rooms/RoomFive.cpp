// RoomFive.cpp - Gallery Five.  Owner: Priscillah Njoroge (169532)
//
// Responsibility: testing and documentation
//
// ============================================================================
// THIS IS YOUR FILE. Nobody else edits it, and you edit nothing outside it.
// Read src/rooms/RoomTwo.cpp first - it is the worked example with every part
// filled in. Then fill this one in the same shape.
// ============================================================================
//
// FOR THIS ROOM:
//   Matte, chalky surfaces suit a room of works on paper - Lambert with
//   almost no specular is the honest choice there, and the contrast with
//   Gallery Two's varnished oils is worth pointing at in the report.
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

class RoomFive final : public Room {
public:
    std::vector<Piece> build(const RoomContext& ctx) override {
        std::vector<Piece> pieces;
        if (!ctx.me) return pieces;

        // --------------------------------------------------------------------
        // TODO (Priscillah Njoroge (169532))
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
        //            canvas.material.shading = cfg::Shading::Lambert;
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
        // TODO (Priscillah Njoroge (169532)): which techniques does this room demonstrate, and WHY
        // did you choose each one over the alternatives? The brief asks every
        // member to justify this. Write it as you build, not in week 13.
        return "Gallery Five - not yet written up.\n";
    }

private:
    float time_ = 0.0f;
};

} // namespace

RoomPtr makeRoomFive() { return std::make_unique<RoomFive>(); }

} // namespace lumen
