// RoomFour.cpp - Gallery Four. Owner: Erykah Diang'a (168661)
//
// Gouraud shading, to show how a highlight can fall between vertices.
// Its works are hung for the final submission. For now the room is built
// from the plan and lights up when you walk in, but has nothing on the walls.

#include "core/Room.h"

#include <string>
#include <vector>

namespace lumen {
namespace {

class RoomFour final : public Room {
public:
    std::vector<Piece> build(const RoomContext&) override { return {}; }

    std::string techniqueNotes() const override {
        return "Gallery Four: works still to be hung.\n";
    }
};

} // namespace

RoomPtr makeRoomFour() { return std::make_unique<RoomFour>(); }

} // namespace lumen
