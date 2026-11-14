// RoomThree.cpp - Gallery Three. Owner: Shawn Kuria (158657)
//
// The largest room, and the furthest from the entrance.
// Its works are hung for the final submission. For now the room is built
// from the plan and lights up when you walk in, but has nothing on the walls.

#include "core/Room.h"

#include <string>
#include <vector>

namespace lumen {
namespace {

class RoomThree final : public Room {
public:
    std::vector<Piece> build(const RoomContext&) override { return {}; }

    std::string techniqueNotes() const override {
        return "Gallery Three: works still to be hung.\n";
    }
};

} // namespace

RoomPtr makeRoomThree() { return std::make_unique<RoomThree>(); }

} // namespace lumen
