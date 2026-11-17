// RoomFive.cpp - Gallery Five. Owner: Priscillah Njoroge (169532)
//
// Works on paper, in matte Lambert shading with almost no shine.
// The five Simba the Mighty works hang here, with his ancestor on the far
// wall, in the centre, facing anyone who walks in. Three cat statues stand
// on plinths below the vault's light slots.

#include "core/Room.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace lumen {
namespace {

// Settings for this room. Change these, not the code in build().
constexpr float kFillWidth    = 0.70f;   // canvas width as a share of its hang point
constexpr float kCentreFill   = 0.85f;   // the ancestor is a little larger
constexpr float kSolidMargin  = 0.05f;   // m, how far past the plinth you are stopped

struct Hung {
    const char* file;     // inside assets/
    std::size_t hang;     // index into this room's hang points
    float       fill;
    const char* title;
};

// Hang points 0 to 2 are the far wall, 3 and 4 the west wall, 5 and 6 the east.
// The far wall's middle hang point (1) is the centre of the room's view.
const Hung kWorks[] = {
    {"rooms/R5/simba-the-mighty.jpg",    0, kFillWidth,  "Simba the Mighty"},
    {"rooms/R5/simba-the-ancestor.jpg",  1, kCentreFill, "Simba the Mighty's Ancestor"},
    {"rooms/R5/simba-on-his-throne.jpg", 2, kFillWidth,  "Simba the Mighty on his Throne"},
    {"rooms/R5/simba-after-battle.jpg",  3, kFillWidth,  "Simba the Mighty after Battle"},
    {"rooms/R5/simba-innovating.jpg",    5, kFillWidth,  "Simba the Mighty Innovating"},
};

struct Statue {
    const char* model;     // inside assets/
    const char* title;
    float       x, y;      // plan position of the plinth's centre, in metres
    float       heading;   // compass degrees: 0 north (the door), 90 east, 180 south
    float       height;    // m, the statue itself
    float       plinthW;   // m, across the plan's x
    float       plinthD;   // m, across the plan's y
    float       plinthH;   // m
    const char* finish;    // "scan" for its own textures, "marble" or "bronze"
};

// The room runs x 11 to 21, y 0 to 9. The door is in the north wall (y 9) and
// the pictures are on the south wall (y 0). The vault's light slots run along
// x 13.5 and x 18.5, so the two tall cats stand under them, turned in towards
// the path from the door. The small one sits at the ancestor's feet.
const Statue kStatues[] = {
    {"rooms/R5/statue/cat-statue/concrete_cat_statue_4k.gltf",
     "Concrete Cat", 13.5f, 4.6f, 30.0f, 0.90f, 0.95f, 0.95f, 0.80f, "scan"},
    {"rooms/R5/statue/cat-stretching/scene.gltf",
     "Cat Stretching", 18.5f, 4.6f, 330.0f, 0.85f, 0.85f, 0.85f, 0.80f, "marble"},
    {"rooms/R5/statue/chonker/scene.gltf",
     "The Chonker", 16.0f, 2.0f, 0.0f, 0.50f, 0.90f, 0.80f, 0.40f, "scan"},
};

class RoomFive final : public Room {
public:
    std::vector<Piece> build(const RoomContext& ctx) override {
        std::vector<Piece> pieces;
        solid_.clear();
        if (!ctx.me) return pieces;

        // ---- pictures ----
        for (const Hung& work : kWorks) {
            if (work.hang >= ctx.me->hangs.size()) continue;
            const HangPoint& hang = ctx.me->hangs[work.hang];

            const gfx::Texture* image = ctx.texture(work.file);
            if (!image) continue;

            // All five works are square, so the canvas is as tall as it is wide.
            const float w = hang.width() * work.fill;
            const float h = w;

            build::FramedWork framed = build::buildFramedWork(hang, w, h);

            // Matte, with no shine: this room has no varnish.
            Piece canvas;
            canvas.mesh = std::move(framed.canvas);
            canvas.material.albedo   = image;
            canvas.material.shading  = cfg::Shading::Lambert;
            canvas.material.specular = 0.0f;
            canvas.note = std::string("work: ") + work.title;
            pieces.push_back(std::move(canvas));

            Piece frame;
            frame.mesh = std::move(framed.frame);
            frame.material.albedo   = ctx.texture("materials/gilt.jpg");
            frame.material.tint     = glm::vec3(0.86f, 0.72f, 0.42f);
            frame.material.shading  = cfg::Shading::Lambert;
            frame.material.specular = 0.03f;
            frame.note = std::string("frame: ") + work.title;
            pieces.push_back(std::move(frame));

            Piece plate;
            plate.mesh = std::move(framed.plate);
            plate.material.albedo   = ctx.texture("materials/label.png");
            plate.material.shading  = cfg::Shading::Lambert;
            plate.material.specular = 0.02f;
            plate.note = std::string("label: ") + work.title;
            pieces.push_back(std::move(plate));
        }

        // ---- statues on plinths ----
        for (const Statue& s : kStatues) {
            const glm::vec3 floorCentre(worldX(s.x), 0.0f, worldZ(s.y));

            Piece plinth;
            plinth.mesh = build::buildPlinth(floorCentre, s.plinthW, s.plinthD, s.plinthH);
            plinth.material.albedo   = ctx.texture("materials/plaster.jpg");
            plinth.material.tint     = glm::vec3(0.82f);
            plinth.material.shading  = cfg::Shading::Lambert;
            plinth.material.specular = 0.0f;
            plinth.note = std::string("plinth: ") + s.title;
            pieces.push_back(std::move(plinth));

            // Stood on top of the plinth, so lifted by its height.
            for (Piece& p : ctx.model(s.model, Vec2{s.x, s.y}, s.heading, s.height,
                                      s.plinthH, s.finish)) {
                p.note = std::string("statue: ") + s.title;
                pieces.push_back(std::move(p));
            }

            // The plinth is solid, so the visitor walks round it.
            solid_.push_back(Footprint{
                Vec2{s.x, s.y},
                Vec2{s.plinthW * 0.5f + kSolidMargin, s.plinthD * 0.5f + kSolidMargin}});
        }

        return pieces;
    }

    std::vector<Footprint> footprints() const override { return solid_; }

    std::string techniqueNotes() const override {
        return
            "Gallery Five (Priscillah Njoroge)\n"
            "  Lambert shading with almost no shine, because these are works on\n"
            "    paper and screen, with no varnish to catch the lamp.\n"
            "  Five digital works, all square, hung from the plan's hang points.\n"
            "  The ancestor takes the centre of the far wall and is hung larger,\n"
            "    so it is the first thing you see on entering.\n"
            "  Three cat statues on plaster plinths: two under the vault's light\n"
            "    slots, turned towards the path from the door, and a small one at\n"
            "    the ancestor's feet. Each plinth is a solid footprint.\n"
            "  One statue keeps its own scanned textures, one is finished in marble.\n";
    }

private:
    std::vector<Footprint> solid_;
};

} // namespace

RoomPtr makeRoomFive() { return std::make_unique<RoomFive>(); }

} // namespace lumen
