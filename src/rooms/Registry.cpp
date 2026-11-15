// Registry.cpp - the list of rooms, and how a room loads its textures. Owner: Shawn.

#include "core/Room.h"

#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>

#include "core/Model.h"
#include "core/Sculpture.h"

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

const gfx::Texture* RoomContext::canvas(const std::string& key, const raster::Canvas& image) const {
    if (!textures) return nullptr;
    return textures->fromCanvas("room:" + key, image);
}

std::vector<Piece> RoomContext::model(const std::string& relativePath, Vec2 at, float headingDeg,
                                      float height, float elevation,
                                      const std::string& finish) const {
    std::vector<Piece> out;
    if (!textures) return out;
    const std::string dir = assetDir ? *assetDir : std::string("assets/");
    sculpture::Finish f = sculpture::Finish::Scan;
    if (!sculpture::parseFinish(finish, f)) {
        std::fprintf(stderr, "[rooms] unknown finish '%s'; use scan, marble or bronze.\n", finish.c_str());
        return out;
    }
    model::Model m;
    try {
        m = model::load(dir + relativePath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[rooms] %s\n", e.what());
        return out;
    }
    // A model's front is +Z; turning by 180 - heading points it along the heading.
    const glm::mat4 place =
        glm::translate(glm::mat4(1.0f), glm::vec3(worldX(at.x), elevation, worldZ(at.y))) *
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f - headingDeg), glm::vec3(0.0f, 1.0f, 0.0f)) *
        model::fitToHeight(m, height);
    for (model::Part& part : m.parts) {
        Piece p;
        p.material  = sculpture::statueMaterial(part, f, dir, *textures);
        p.mesh      = std::move(part.mesh);
        p.transform = place;
        p.note      = relativePath;
        out.push_back(std::move(p));
    }
    return out;
}

// ---- the registry ----------------------------------------------------------
//
// One line per room. An unfinished room just shows up empty.

RoomPtr makeRoomOne();     // Timothy Kimani     - Gallery One
RoomPtr makeRoomTwo();     // Charles Murage     - Gallery Two
RoomPtr makeRoomThree();   // Shawn Kuria        - Gallery Three
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
