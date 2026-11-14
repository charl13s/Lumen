// RoomOne.cpp - Gallery One. Owner: Timothy Kimani (166096)
//
// The first room off the corridor.
// Its works are hung for the final submission. For now the room is built
// from the plan and lights up when you walk in, but has nothing on the walls.

#include "core/Room.h"

#include <string>
#include <vector>

namespace lumen {
namespace {

class RoomOne final : public Room {
public:
    std::vector<Piece> build(const RoomContext&) override { return {}; }

    std::string techniqueNotes() const override {
        return "Gallery One: works still to be hung.\n";
    }
};

} // namespace

RoomPtr makeRoomOne() { return std::make_unique<RoomOne>(); }

} // namespace lumen
