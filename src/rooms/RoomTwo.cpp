// RoomTwo.cpp - Gallery Two.  Owner: Charles Murage (165948)
//
// ============================================================================
// THIS FILE IS THE WORKED EXAMPLE. If you are starting your own room, read this
// one first, then copy its shape into yours. It is the only room file that is
// filled in; the other four are stubs waiting for their owners.
// ============================================================================
//
// Everything a room does, it does through RoomContext and the build:: helpers.
// There is not a single OpenGL call below, and there must not be one in yours.
//
// Technique chosen for this room: Blinn-Phong per fragment, with the relief map
// derived from each painting's own luminance. Justification is in
// techniqueNotes() at the bottom - write yours as you go, because the brief
// (section 7, "Understanding") asks each of us to justify our choices, and
// nobody remembers their reasoning in week 13.

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
        // The plan gives us seven hang points. We do not choose where they are:
        // they come from assets/plan.json so the drawing on the slide and the
        // build cannot disagree. We choose what hangs on them.
        //
        // Replace these with real public-domain images from the Art Institute of
        // Chicago or Rijksmuseum open-access collections, and put the credit
        // line in assets/works.json. See docs/ARTWORK.md.
        const char* images[] = {
            "works/placeholder-01.jpg", "works/placeholder-02.jpg",
            "works/placeholder-03.jpg", "works/placeholder-04.jpg",
            "works/placeholder-05.jpg", "works/placeholder-06.jpg",
            "works/placeholder-07.jpg",
        };

        for (std::size_t i = 0; i < ctx.me->hangs.size(); ++i) {
            const HangPoint& hang = ctx.me->hangs[i];
            const std::string image = images[i % 7];

            // The hang point tells us how much wall we have; size the canvas to
            // sit inside it with a margin.
            const float w = hang.width() * 0.78f;
            const float h = w * 0.74f;   // a plausible landscape ratio

            build::FramedWork work = build::buildFramedWork(hang, w, h);

            // The canvas. Blinn-Phong plus the relief map: the painting's own
            // brightness becomes fake impasto, so the brushwork catches the lamp
            // as the visitor walks past. This is the cheapest big win in the
            // project - no extra asset, about fifteen lines in Gfx.cpp.
            Piece canvas;
            canvas.mesh     = std::move(work.canvas);
            canvas.material.albedo    = ctx.texture(image);
            canvas.material.normal    = ctx.relief(image);
            canvas.material.shading   = cfg::Shading::NormalMapped;
            canvas.material.specular  = 0.22f;   // varnish sheen, not a mirror
            canvas.material.shininess = 48.0f;
            canvas.note = "painting " + std::to_string(i) + " - relief from luminance";
            pieces.push_back(std::move(canvas));

            // The moulding. Matte gilt: a broad, weak specular reads as old gold
            // far better than a tight bright one.
            Piece frame;
            frame.mesh     = std::move(work.frame);
            frame.material.albedo    = ctx.texture("materials/gilt.jpg");
            frame.material.tint      = glm::vec3(0.86f, 0.72f, 0.42f);
            frame.material.shading   = cfg::Shading::BlinnPhong;
            frame.material.specular  = 0.35f;
            frame.material.shininess = 16.0f;
            frame.note = "frame " + std::to_string(i);
            pieces.push_back(std::move(frame));

            // The wall label. Nearly matte, so it never catches a highlight and
            // distracts from the work.
            Piece plate;
            plate.mesh     = std::move(work.plate);
            plate.material.albedo   = ctx.texture("materials/label.png");
            plate.material.shading  = cfg::Shading::Lambert;
            plate.material.specular = 0.02f;
            plate.note = "label " + std::to_string(i);
            pieces.push_back(std::move(plate));
        }

        // ---- 2. a sculptural piece ----------------------------------------
        // One object with real depth in the middle of the room, so the vault
        // wash has something to model and the lamp has something to rake across.
        const Vec2 c = ctx.me->rect.centre();
        const glm::vec3 floorCentre(worldX(c.x), 0.0f, worldZ(c.y));

        Piece plinth;
        plinth.mesh = build::buildPlinth(floorCentre, 0.75f, 0.75f, 0.95f);
        plinth.material.albedo    = ctx.texture("materials/plaster.jpg");
        plinth.material.tint      = glm::vec3(0.82f);
        plinth.material.shading   = cfg::Shading::BlinnPhong;
        plinth.material.specular  = 0.05f;   // matte: plinths must not compete
        plinth.note = "plinth";
        pieces.push_back(std::move(plinth));

        Piece bust;
        bust.mesh = build::buildCylinder(floorCentre + glm::vec3(0.0f, 0.95f, 0.0f),
                                         0.26f, 0.62f, 28);
        bust.material.albedo    = ctx.texture("materials/marble.jpg");
        bust.material.shading   = cfg::Shading::BlinnPhong;
        bust.material.specular  = 0.30f;
        bust.material.shininess = 64.0f;
        bust.note = "sculpture - replace with a real mesh";
        pieces.push_back(std::move(bust));

        return pieces;
    }

    bool update(float dt) override {
        time_ += dt;
        return false;   // nothing in this room moves yet
    }

    void onEnter() override {
        // The vault fade is handled for you by LightingState - do NOT re-implement
        // it here. Use this hook only for things specific to your room.
    }

    std::string techniqueNotes() const override {
        return
            "Gallery Two - Charles Murage\n"
            "  Blinn-Phong, per fragment, as the room default.\n"
            "    Chosen over Lambert because oil paintings under varnish have a\n"
            "    visible sheen that shifts as you move, and over per-vertex\n"
            "    Gouraud because the lamp's highlight is small relative to the\n"
            "    wall quads and would fall between vertices (see Gallery Four,\n"
            "    which demonstrates exactly that artefact deliberately).\n"
            "  Normal mapping on every canvas, derived from the image's own\n"
            "    luminance by a Sobel pass in TextureCache::reliefFrom. Fake\n"
            "    impasto: costs no extra asset and is most visible precisely when\n"
            "    the carried lamp rakes across at a grazing angle.\n"
            "  Specular deliberately low and broad on the frames (shininess 16)\n"
            "    and higher and tighter on the marble (64), because gilt gesso\n"
            "    scatters and polished stone does not.\n"
            "  Geometry: polygon extrusion for the frame mouldings, a swept\n"
            "    cylinder for the sculpture.\n";
    }

private:
    float time_ = 0.0f;
};

} // namespace

RoomPtr makeRoomTwo() { return std::make_unique<RoomTwo>(); }

} // namespace lumen
