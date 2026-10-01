#include "Room.h"

#include <cstdio>

namespace lumen {

const gfx::Texture* RoomContext::texture(const std::string& relativePath) const {
    if (!textures) return nullptr;
    const std::string dir = assetDir ? *assetDir : std::string("assets/");
    return textures->get(dir + relativePath, gfx::Texture::Kind::Colour);
}

const gfx::Texture* RoomContext::relief(const std::string& relativePath) const {
    if (!textures) return nullptr;
    const std::string dir = assetDir ? *assetDir : std::string("assets/");
    return textures->reliefFrom(dir + relativePath);
}

// ---- the registry ----------------------------------------------------------
//
// Each member declares their factory here. One line each. If your room is not
// written yet, leave the line - makeRoom returns nullptr and the app runs with
// your room empty, so nobody is blocked waiting for you.

RoomPtr makeRoomOne();     // Shawn Kuria        - Gallery One
RoomPtr makeRoomTwo();     // Charles Murage     - Gallery Two
RoomPtr makeRoomThree();   // Timothy Kimani     - Gallery Three (generated artwork)
RoomPtr makeRoomFour();    // Erykah Diang'a     - Gallery Four
RoomPtr makeRoomFive();    // Priscillah Njoroge - Gallery Five

RoomPtr makeRoom(const std::string& roomId) {
    if (roomId == "R1") return makeRoomOne();
    if (roomId == "R2") return makeRoomTwo();
    if (roomId == "R3") return makeRoomThree();
    if (roomId == "R4") return makeRoomFour();
    if (roomId == "R5") return makeRoomFive();

    std::fprintf(stderr, "[rooms] No factory registered for room id '%s'.\n",
                 roomId.c_str());
    return nullptr;
}

} // namespace lumen
