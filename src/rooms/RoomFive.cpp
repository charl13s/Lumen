// RoomFive.cpp - Gallery Five. Owner: Priscillah Njoroge (169532)
//
// Works on paper, in matte Lambert shading with almost no shine.
// Its works are hung for the final submission. For now the room is built
// from the plan and lights up when you walk in, but has nothing on the walls.

#include "core/Room.h"

#include <string>
#include <vector>

namespace lumen {
namespace {

class RoomFive final : public Room {
public:
    std::vector<Piece> build(const RoomContext&) override { return {}; }

    std::string techniqueNotes() const override {
        return "Gallery Five: works still to be hung.\n";
    }
};

} // namespace

RoomPtr makeRoomFive() { return std::make_unique<RoomFive>(); }

} // namespace lumen
