// RoomTwo.cpp - Gallery Two.  Owner: Charles Murage (165948)
//
// Things I like, hung like a proper gallery: my favourite meme, my favourite
// painting, my own painting and my idol. Two works on each side wall, the
// walls and vaults painted green, and a label under everything. The front wall
// is still to come.
//
// Technique: Blinn-Phong per pixel. The painting is normal mapped from its
// own brightness; the photographs are glossy prints with a sharper shine.

#include "core/Room.h"
#include "core/Label.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace lumen {
namespace {

// ---- the room's own constants ----------------------------------------------

// The lining painted on every wall, and the skirting board along its foot.
const glm::vec3 kWallColour    {0.10f, 0.21f, 0.16f};   // deep gallery green
const glm::vec3 kSkirtingColour{0.05f, 0.09f, 0.07f};

constexpr float kLiningOut     = 0.010f;   // m, the lining off the wall face
constexpr float kSkirtingH     = 0.16f;
constexpr float kSkirtingDepth = 0.025f;

constexpr float kWorkMaxW    = 1.80f;   // m, the widest a work can be
constexpr float kWorkMaxH    = 1.25f;   // m, the tallest
constexpr float kWorkCentreY = 1.75f;   // m to the centre of each work
constexpr float kLabelWidth  = 0.64f;   // m
constexpr int   kLabelPixels = 540;     // across the card
constexpr int   kLabelFont   = 4;       // font scale, one up from the corridor labels
constexpr float kLabelGap    = 0.06f;   // m between the frame and its label

constexpr float kPlinth  = 0.75f;   // m square
constexpr float kPlinthH = 0.95f;

// One work: its image, where it hangs, and what its label says.
struct Work {
    std::string image;      // under assets/
    bool        west;       // the west wall (left from the door) or the east
    float       along;      // how far along that wall from the door end, 0 to 1
    bool        painting;   // paint is normal mapped; a photograph is a glossy print
    label::Text text;
};

// In the order you meet them: up the right wall, then back down the left.
// Two to a wall, at its quarter points, so each has room to breathe.
std::vector<Work> works() {
    return {
        {"works/Room 2/monkey.jpg", false, 0.27f, false,
         {"The Thinking Monkey", {}, "", ""}},
        {"works/Room 2/banana.png", false, 0.73f, false,
         {"Banana and Duct Tape", {"Maurizio Cattelan, 2019"}, "", ""}},
        {"works/Room 2/MonaErykah.jpg", true, 0.73f, true,
         {"Mona Erykah", {"Charles Murage, 2026"}, "", ""}},
        {"works/Room 2/Jensen.jpg", true, 0.27f, false,
         {"My Idol", {"Jensen Huang, co-founder of NVIDIA"}, "", ""}},
    };
}

label::Text drumLabel() {
    return {"Untitled (Cylinder)", {"Charles Murage, 2026", "Marble"}, "", ""};
}

glm::vec3 w3(float x, float y, float z) { return glm::vec3(worldX(x), y, worldZ(z)); }

// A hang point on the lining of a side wall, centred `along` its length.
HangPoint sideWallHang(const Rect& r, float halfWall, bool west, float along, float width) {
    const float y0 = r.y0 + halfWall, y1 = r.y1 - halfWall;
    const float y  = y0 + (y1 - y0) * along;
    const float x  = west ? r.x0 + halfWall + kLiningOut : r.x1 - halfWall - kLiningOut;
    HangPoint h;
    h.span.a = Vec2{x, y - width * 0.5f};
    h.span.b = Vec2{x, y + width * 0.5f};
    h.inwardNormal = Vec2{west ? 1.0f : -1.0f, 0.0f};
    return h;
}

// A run of lining and its skirting, from a to b on the plan, facing n.
void lineWall(Vec2 a, Vec2 b, Vec2 n, float height, gfx::MeshData& lining, gfx::MeshData& skirting) {
    const glm::vec3 in(n.x, 0.0f, -n.y);   // into the room, in world space
    glm::vec3 p0 = w3(a.x, 0.0f, a.y) + in * kLiningOut;
    glm::vec3 p1 = w3(b.x, 0.0f, b.y) + in * kLiningOut;
    // Wind it so the face looks into the room.
    if (glm::dot(glm::cross(p1 - p0, glm::vec3(0.0f, 1.0f, 0.0f)), in) < 0.0f) std::swap(p0, p1);
    const glm::vec3 up(0.0f, height, 0.0f);
    lining.addQuad(p0, p1, p1 + up, p0 + up, glm::length(p1 - p0) / 2.0f, height / 2.0f);

    const glm::vec3 mid = (p0 + p1) * 0.5f + in * (kSkirtingDepth * 0.5f) +
                          glm::vec3(0.0f, kSkirtingH * 0.5f, 0.0f);
    const glm::vec3 along = p1 - p0;
    const glm::vec3 size(std::fabs(along.x) + std::fabs(in.x) * kSkirtingDepth, kSkirtingH,
                         std::fabs(along.z) + std::fabs(in.z) * kSkirtingDepth);
    skirting.append(build::buildBox(mid, size), glm::mat4(1.0f));
}

class RoomTwo final : public Room {
public:
    std::vector<Piece> build(const RoomContext& ctx) override {
        std::vector<Piece> pieces;
        if (!ctx.me) return pieces;
        const Rect& r = ctx.me->rect;
        const float halfWall = ctx.plan ? ctx.plan->wallThickness * 0.5f : 0.225f;
        const float wallH    = ctx.plan ? ctx.plan->wallHeight : 4.2f;

        // ---- 1. the walls ------------------------------------------------
        // A matte painted lining on all four walls, with a gap for the door.
        {
            const float x0 = r.x0 + halfWall, x1 = r.x1 - halfWall;
            const float y0 = r.y0 + halfWall, y1 = r.y1 - halfWall;
            gfx::MeshData lining, skirting;
            lineWall({x0, y1}, {x1, y1}, {0.0f, -1.0f}, wallH, lining, skirting);   // north
            lineWall({x0, y0}, {x0, y1}, {1.0f, 0.0f}, wallH, lining, skirting);    // west
            lineWall({x1, y0}, {x1, y1}, {-1.0f, 0.0f}, wallH, lining, skirting);   // east
            const Door& d = ctx.me->door;   // south, with the doorway
            lineWall({x0, y0}, {d.a, y0}, {0.0f, 1.0f}, wallH, lining, skirting);
            lineWall({d.b, y0}, {x1, y0}, {0.0f, 1.0f}, wallH, lining, skirting);

            Piece paint;
            paint.mesh = std::move(lining);
            paint.material.albedo   = ctx.texture("materials/plaster.jpg");
            paint.material.tint     = kWallColour;
            paint.material.shading  = cfg::Shading::Lambert;   // flat paint
            paint.material.specular = 0.03f;
            paint.note = "wall lining";
            pieces.push_back(std::move(paint));

            Piece skirt;
            skirt.mesh = std::move(skirting);
            skirt.material.tint      = kSkirtingColour;
            skirt.material.shading   = cfg::Shading::BlinnPhong;
            skirt.material.specular  = 0.25f;   // satin paint on wood
            skirt.material.shininess = 40.0f;
            skirt.note = "skirting";
            pieces.push_back(std::move(skirt));
        }

        // ---- 2. the ceiling ----------------------------------------------
        // The same green on the vaults. A copy of the building's vaults, pulled
        // in to the lining and dropped by the same gap, so it sits just inside
        // them with no flicker where the two meet.
        {
            const float apex = ctx.plan ? ctx.plan->vaultApex : 6.0f;
            Piece vault;
            vault.mesh.append(build::buildRoomVaults(*ctx.me, wallH, apex,
                                                     2.0f * (halfWall + kLiningOut)),
                              glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -kLiningOut, 0.0f)));
            vault.material.albedo   = ctx.texture("materials/plaster.jpg");
            vault.material.tint     = kWallColour;
            vault.material.shading  = cfg::Shading::Lambert;
            vault.material.specular = 0.02f;
            vault.note = "painted vaults";
            pieces.push_back(std::move(vault));
        }

        // ---- 3. the works ------------------------------------------------
        for (const Work& wk : works()) {
            // As big as fits, keeping the image's shape.
            const gfx::Texture* img = ctx.texture(wk.image);
            const float aspect = (img && img->height() > 0)
                                     ? static_cast<float>(img->width()) / static_cast<float>(img->height())
                                     : 1.5f;
            float w = kWorkMaxW, h = w / aspect;
            if (h > kWorkMaxH) { h = kWorkMaxH; w = h * aspect; }

            const HangPoint hang = sideWallHang(r, halfWall, wk.west, wk.along, w);
            build::FramedWork work = build::buildFramedWork(hang, w, h, kWorkCentreY);

            Piece canvas;
            canvas.mesh = std::move(work.canvas);
            canvas.material.albedo  = img;
            canvas.material.shading = cfg::Shading::BlinnPhong;
            if (wk.painting) {
                // Brushstrokes from the painting's own brightness.
                canvas.material.normal    = ctx.relief(wk.image);
                canvas.material.shading   = cfg::Shading::NormalMapped;
                canvas.material.specular  = 0.22f;   // light varnish
                canvas.material.shininess = 48.0f;
            } else {
                canvas.material.specular  = 0.30f;   // a glossy print
                canvas.material.shininess = 90.0f;
            }
            canvas.note = wk.text.title;
            pieces.push_back(std::move(canvas));

            // The frame. Soft, weak shine looks like old gold.
            Piece frame;
            frame.mesh = std::move(work.frame);
            frame.material.albedo    = ctx.texture("materials/gilt.jpg");
            frame.material.tint      = glm::vec3(0.86f, 0.72f, 0.42f);
            frame.material.shading   = cfg::Shading::BlinnPhong;
            frame.material.specular  = 0.35f;
            frame.material.shininess = 16.0f;
            frame.note = wk.text.title + " frame";
            pieces.push_back(std::move(frame));

            // The label, centred on the lining below the frame.
            const glm::vec3 n(hang.inwardNormal.x, 0.0f, -hang.inwardNormal.y);
            const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), n));
            const raster::Canvas card = label::draw(wk.text, kLabelPixels, kLabelFont);
            const glm::vec2 size = label::fit(card, kLabelWidth, 0.5f);
            const Vec2 mid{(hang.span.a.x + hang.span.b.x) * 0.5f, (hang.span.a.y + hang.span.b.y) * 0.5f};
            const float labelY = kWorkCentreY - h * 0.5f - cfg::kFrameWidth - kLabelGap - size.y * 0.5f;
            const glm::vec3 centre = w3(mid.x, labelY, mid.y) + n * 0.004f;
            Piece plate;
            plate.mesh     = label::quad(centre, right, n, size);
            plate.material = label::material(ctx.canvas("gallery2:label:" + wk.image, card));
            plate.note     = wk.text.title + " label";
            pieces.push_back(std::move(plate));
        }

        // ---- 4. a sculpture ----------------------------------------------
        // Something 3D in the middle of the room.
        const Vec2 c = r.centre();
        const glm::vec3 floorCentre = w3(c.x, 0.0f, c.y);

        Piece plinth;
        plinth.mesh = build::buildPlinth(floorCentre, kPlinth, kPlinth, kPlinthH);
        plinth.material.albedo    = ctx.texture("materials/plaster.jpg");
        plinth.material.tint      = glm::vec3(0.82f);
        plinth.material.shading   = cfg::Shading::BlinnPhong;
        plinth.material.specular  = 0.05f;   // matte
        plinth.note = "plinth";
        pieces.push_back(std::move(plinth));

        Piece bust;
        bust.mesh = build::buildCylinder(floorCentre + glm::vec3(0.0f, kPlinthH, 0.0f),
                                         0.26f, 0.62f, 28);
        bust.material.albedo    = ctx.texture("materials/marble.jpg");
        bust.material.shading   = cfg::Shading::BlinnPhong;
        bust.material.specular  = 0.30f;
        bust.material.shininess = 64.0f;
        bust.note = "sculpture: a marble drum";
        pieces.push_back(std::move(bust));

        // Its label on the plinth, facing the door.
        {
            const glm::vec3 front(0.0f, 0.0f, 1.0f);   // south, towards the door
            const glm::vec3 right(1.0f, 0.0f, 0.0f);   // the visitor's right, looking north
            const raster::Canvas card = label::draw(drumLabel(), kLabelPixels, kLabelFont);
            const glm::vec2 size = label::fit(card, 0.46f, kPlinthH - 0.2f);
            const glm::vec3 centre = floorCentre + front * (kPlinth * 0.5f + 0.004f) +
                                     glm::vec3(0.0f, kPlinthH * 0.62f, 0.0f);
            Piece plate;
            plate.mesh     = label::quad(centre, right, front, size);
            plate.material = label::material(ctx.canvas("gallery2:label:drum", card));
            plate.note     = "drum label";
            pieces.push_back(std::move(plate));
        }

        centre_ = c;
        return pieces;
    }

    bool update(float dt) override {
        time_ += dt;
        return false;   // nothing in this room moves yet
    }

    // The plinth is solid.
    std::vector<Footprint> footprints() const override {
        return {Footprint{centre_, Vec2{kPlinth * 0.5f, kPlinth * 0.5f}}};
    }

    std::string techniqueNotes() const override {
        return
            "Gallery Two (Charles Murage)\n"
            "  Blinn-Phong per pixel. Gouraud would lose the lamp's small highlight.\n"
            "  The painting is normal mapped from its own brightness, so brushstrokes\n"
            "    show when the lamp passes. The photographs are glossy prints instead.\n"
            "  Soft shine on the gold frames, sharper shine on the marble.\n"
            "  Frames are a cubic Bezier moulding swept round each painting.\n"
            "  Labels are drawn by our own 2D code with the bitmap font.\n";
    }

private:
    float time_ = 0.0f;
    Vec2  centre_{};
};

} // namespace

RoomPtr makeRoomTwo() { return std::make_unique<RoomTwo>(); }

} // namespace lumen
