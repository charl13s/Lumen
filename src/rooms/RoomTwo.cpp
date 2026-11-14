// RoomTwo.cpp - Gallery Two.  Owner: Charles Murage (165948)
//
// The finished example room: paintings, frames, labels and a sculpture.
// Everything goes through RoomContext and build::, with no OpenGL.
//
// Technique: Blinn-Phong per pixel, with normal maps made from each painting.

#include "core/Room.h"

#include <string>
#include <vector>

namespace lumen {
namespace {

class RoomTwo final : public Room {
public:
    std::vector<Piece> build(const RoomContext& ctx) override {
        std::vector<Piece> pieces;
        if (!ctx.me) return pieces;

        // ---- 1. the works -------------------------------------------------
        // Seven hang points come from the plan.
        // Placeholders for now. Real public domain images go here, with their
        // credits in assets/works.json.
        const char* images[] = {
            "works/placeholder-01.jpg", "works/placeholder-02.jpg",
            "works/placeholder-03.jpg", "works/placeholder-04.jpg",
            "works/placeholder-05.jpg", "works/placeholder-06.jpg",
            "works/placeholder-07.jpg",
        };

        for (std::size_t i = 0; i < ctx.me->hangs.size(); ++i) {
            const HangPoint& hang = ctx.me->hangs[i];
            const std::string image = images[i % 7];

            // Size the canvas to fit the wall with a margin.
            const float w = hang.width() * 0.78f;
            const float h = w * 0.74f;   // landscape

            build::FramedWork work = build::buildFramedWork(hang, w, h);

            // The canvas, with a normal map made from the painting itself.
            Piece canvas;
            canvas.mesh     = std::move(work.canvas);
            canvas.material.albedo    = ctx.texture(image);
            canvas.material.normal    = ctx.relief(image);
            canvas.material.shading   = cfg::Shading::NormalMapped;
            canvas.material.specular  = 0.22f;   // light varnish shine
            canvas.material.shininess = 48.0f;
            canvas.note = "painting " + std::to_string(i) + " (normal mapped)";
            pieces.push_back(std::move(canvas));

            // The frame. Soft, weak shine looks like old gold.
            Piece frame;
            frame.mesh     = std::move(work.frame);
            frame.material.albedo    = ctx.texture("materials/gilt.jpg");
            frame.material.tint      = glm::vec3(0.86f, 0.72f, 0.42f);
            frame.material.shading   = cfg::Shading::BlinnPhong;
            frame.material.specular  = 0.35f;
            frame.material.shininess = 16.0f;
            frame.note = "frame " + std::to_string(i);
            pieces.push_back(std::move(frame));

            // The wall label, matte so it does not shine.
            Piece plate;
            plate.mesh     = std::move(work.plate);
            plate.material.albedo   = ctx.texture("materials/label.png");
            plate.material.shading  = cfg::Shading::Lambert;
            plate.material.specular = 0.02f;
            plate.note = "label " + std::to_string(i);
            pieces.push_back(std::move(plate));
        }

        // ---- 2. a sculpture ----------------------------------------------
        // Something 3D in the middle of the room.
        const Vec2 c = ctx.me->rect.centre();
        const glm::vec3 floorCentre(worldX(c.x), 0.0f, worldZ(c.y));

        Piece plinth;
        plinth.mesh = build::buildPlinth(floorCentre, 0.75f, 0.75f, 0.95f);
        plinth.material.albedo    = ctx.texture("materials/plaster.jpg");
        plinth.material.tint      = glm::vec3(0.82f);
        plinth.material.shading   = cfg::Shading::BlinnPhong;
        plinth.material.specular  = 0.05f;   // matte
        plinth.note = "plinth";
        pieces.push_back(std::move(plinth));

        Piece bust;
        bust.mesh = build::buildCylinder(floorCentre + glm::vec3(0.0f, 0.95f, 0.0f),
                                         0.26f, 0.62f, 28);
        bust.material.albedo    = ctx.texture("materials/marble.jpg");
        bust.material.shading   = cfg::Shading::BlinnPhong;
        bust.material.specular  = 0.30f;
        bust.material.shininess = 64.0f;
        bust.note = "sculpture: a marble drum";
        pieces.push_back(std::move(bust));

        return pieces;
    }

    bool update(float dt) override {
        time_ += dt;
        return false;   // nothing in this room moves yet
    }

    void onEnter() override {
        // The room light fade is already handled. Use this for anything else.
    }

    std::string techniqueNotes() const override {
        return
            "Gallery Two (Charles Murage)\n"
            "  Blinn-Phong per pixel. Gouraud would lose the lamp's small highlight.\n"
            "  Normal mapping on the paintings, made from each image's brightness,\n"
            "    so brushstrokes show when the lamp passes.\n"
            "  Soft shine on the gold frames, sharper shine on the marble.\n"
            "  Frames are a cubic Bezier moulding swept round each painting.\n";
    }

private:
    float time_ = 0.0f;
};

} // namespace

RoomPtr makeRoomTwo() { return std::make_unique<RoomTwo>(); }

} // namespace lumen
