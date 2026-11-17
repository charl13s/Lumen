// RoomOne.cpp - Gallery One. Owner: Timothy Kimani (166096)
//
// Light on the Land: six photographs of high and wild places, from Mount
// Kenya to the Arctic Circle. Mount Kenya hangs in the centre of the far
// wall, the first thing you see from the door, with an Alpine lake on each
// side. A bench in the middle of the room faces it.
//
// Technique: the photographs are glossy prints behind glass (Blinn-Phong with
// a small, sharp highlight that slides across them as the lamp moves). The
// aurora is a backlit lightbox (emissive): the only work in the room that
// makes its own light, so it still glows when the room is dark.

#include "core/Room.h"
#include "core/Label.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace lumen {
namespace {

// ---- the room's own constants ----------------------------------------------

constexpr float kWorkCentreY = 1.90f;   // m to the centre of each work
constexpr float kLabelWidth  = 0.50f;   // m
constexpr int   kLabelPixels = 540;     // across the card
constexpr int   kLabelFont   = 4;
constexpr float kLabelGap    = 0.06f;   // m between the frame and its label
constexpr float kOffWall     = 0.004f;  // m, cards sit just proud of the wall

// The room's title card, by the door.
constexpr float kTitleCentreY = 1.60f;
constexpr float kTitleMaxW    = 1.40f;
constexpr float kTitleMaxH    = 1.20f;
constexpr int   kTitlePixels  = 900;

// Thin satin black frames, as photographs are framed.
const glm::vec3 kFrameColour{0.06f, 0.06f, 0.065f};

// Glass over a print: a weak but very tight highlight.
constexpr float kGlassSpecular  = 0.35f;
constexpr float kGlassShininess = 140.0f;
// How brightly the lightbox shines through its own image.
constexpr float kLightboxGlow   = 0.85f;

// The bench, facing Mount Kenya. Plan metres; the room runs x 11 to 21,
// y 27 (the door) to 36 (the far wall).
constexpr float kBenchX = 16.0f, kBenchY = 30.8f;
constexpr float kBenchL = 1.80f, kBenchD = 0.46f, kBenchH = 0.45f;
constexpr float kSeatT  = 0.07f;
constexpr float kLegT   = 0.07f;
const glm::vec3 kBenchWood{0.42f, 0.27f, 0.15f};   // oiled oak
constexpr float kSolidMargin = 0.05f;   // m, how far from the bench you are stopped

enum class Finish { Print, Lightbox };

struct Work {
    std::string image;   // under assets/
    std::size_t hang;    // which of the room's hang points
    float       maxW;    // m, the largest the image may be
    float       maxH;
    Finish      finish;
    label::Text text;
};

// Hang points 0 to 2 are the far (north) wall, west to east. 3 and 4 are the
// west wall and 5 and 6 the east wall, each starting at the door end.
// Hang point 3 holds the room's title card.
std::vector<Work> works() {
    const std::string credit = "Photographer unknown";
    return {
        {"rooms/R1/eibsee-zugspitze.jpg", 0, 1.40f, 1.60f, Finish::Print,
         {"Eibsee and the Zugspitze", {"Bavaria, Germany", "Photograph"},
          "Germany's highest mountain, 2,962 m, above the clear green water of the Eibsee.",
          credit}},
        {"rooms/R1/mount-kenya.jpg", 1, 1.75f, 1.75f, Finish::Print,
         {"Mount Kenya", {"Kenya", "Photograph"},
          "Africa's second highest mountain. Batian, its highest peak, stands 5,199 m "
          "above the sea, almost on the equator.",
          credit}},
        {"rooms/R1/chamonix.jpg", 2, 1.40f, 1.60f, Finish::Print,
         {"Summer above Chamonix", {"Mont Blanc massif, France", "Photograph"},
          "A mountain lake in the French Alps, below glaciers that never melt, "
          "even in summer.",
          credit}},
        {"rooms/R1/socotra.jpg", 4, 1.50f, 1.60f, Finish::Print,
         {"Dragon's Blood Trees", {"Socotra Island, Yemen", "Photograph"},
          "These umbrella-shaped trees grow nowhere else on Earth. They are named "
          "for their dark red sap.",
          credit}},
        {"rooms/R1/rovaniemi-aurora.jpg", 5, 1.40f, 1.60f, Finish::Lightbox,
         {"Northern Lights over Rovaniemi", {"Lapland, Finland", "Photograph, lightbox"},
          "The aurora borealis above the Arctic Circle. Hung as a lightbox, it is "
          "the only work here that makes its own light.",
          credit}},
        {"rooms/R1/nanga-parbat.jpg", 6, 1.40f, 1.60f, Finish::Print,
         {"Nanga Parbat from Fairy Meadows", {"Gilgit-Baltistan, Pakistan", "Photograph"},
          "The ninth highest mountain on Earth, 8,126 m, catching the last of the sun "
          "above the meadows.",
          credit}},
    };
}

constexpr std::size_t kTitleHang = 3;

label::Text titleCard() {
    return {"Light on the Land",
            {"Gallery One", "Timothy Kimani"},
            "Six high and wild places on three continents, from Mount Kenya to the "
            "Arctic Circle. The photographs are glossy prints behind glass: walk past "
            "with the lamp and its highlight slides across them. The aurora is a "
            "lightbox and glows on its own, even in the dark.",
            ""};
}

glm::vec3 w3(float x, float y, float z) { return glm::vec3(worldX(x), y, worldZ(z)); }

// The plan's hang points sit a little in from the wall. Move one back onto
// the wall's inner face, so the frame hangs on the wall.
HangPoint onWall(HangPoint h, const Rect& r, float halfWall) {
    if (h.inwardNormal.x != 0.0f) {
        const float x = h.inwardNormal.x > 0.0f ? r.x0 + halfWall : r.x1 - halfWall;
        h.span.a.x = h.span.b.x = x;
    } else {
        const float y = h.inwardNormal.y > 0.0f ? r.y0 + halfWall : r.y1 - halfWall;
        h.span.a.y = h.span.b.y = y;
    }
    return h;
}

Vec2 middle(const HangPoint& h) {
    return Vec2{(h.span.a.x + h.span.b.x) * 0.5f, (h.span.a.y + h.span.b.y) * 0.5f};
}

// A card drawn by our own 2D code, flat on the wall at a hang point.
Piece wallCard(const RoomContext& ctx, const HangPoint& hang, const label::Text& text,
               const std::string& key, int pixels, float maxW, float maxH, float topY,
               bool topAligned) {
    const glm::vec3 n(hang.inwardNormal.x, 0.0f, -hang.inwardNormal.y);
    const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), n));
    const raster::Canvas card = label::draw(text, pixels, kLabelFont);
    const glm::vec2 size = label::fit(card, maxW, maxH);
    const Vec2 mid = middle(hang);
    // topAligned: topY is the card's top edge; otherwise it is its centre.
    const float y = topAligned ? topY - size.y * 0.5f : topY;
    Piece p;
    p.mesh     = label::quad(w3(mid.x, y, mid.y) + n * kOffWall, right, n, size);
    p.material = label::material(ctx.canvas(key, card));
    p.note     = text.title + " card";
    return p;
}

class RoomOne final : public Room {
public:
    std::vector<Piece> build(const RoomContext& ctx) override {
        std::vector<Piece> pieces;
        if (!ctx.me) return pieces;
        const Rect& r = ctx.me->rect;
        const float halfWall = ctx.plan ? ctx.plan->wallThickness * 0.5f : 0.225f;
        const auto& hangs = ctx.me->hangs;

        // ---- 1. the photographs ------------------------------------------
        for (const Work& wk : works()) {
            if (wk.hang >= hangs.size()) continue;
            const HangPoint hang = onWall(hangs[wk.hang], r, halfWall);

            const gfx::Texture* img = ctx.texture(wk.image);
            if (!img || img->height() <= 0) {
                std::fprintf(stderr, "[Gallery One] could not load %s\n", wk.image.c_str());
                continue;
            }

            // As big as fits, keeping the photograph's own shape.
            const float aspect = static_cast<float>(img->width()) / static_cast<float>(img->height());
            float h = wk.maxH, w = h * aspect;
            if (w > wk.maxW) { w = wk.maxW; h = w / aspect; }

            build::FramedWork framed = build::buildFramedWork(hang, w, h, kWorkCentreY);

            Piece print;
            print.mesh = std::move(framed.canvas);
            print.material.albedo    = img;
            print.material.shading   = cfg::Shading::BlinnPhong;
            print.material.specular  = kGlassSpecular;
            print.material.shininess = kGlassShininess;
            if (wk.finish == Finish::Lightbox) {
                print.material.emissive = kLightboxGlow;   // lit from behind
                print.note = wk.text.title + " (lightbox, emissive)";
            } else {
                print.note = wk.text.title + " (print behind glass)";
            }
            pieces.push_back(std::move(print));

            Piece frame;
            frame.mesh = std::move(framed.frame);
            frame.material.tint      = kFrameColour;
            frame.material.shading   = cfg::Shading::BlinnPhong;
            frame.material.specular  = 0.25f;   // satin
            frame.material.shininess = 40.0f;
            frame.note = wk.text.title + " frame";
            pieces.push_back(std::move(frame));

            // The label, centred under the frame.
            const float labelTop = kWorkCentreY - h * 0.5f - cfg::kFrameWidth - kLabelGap;
            pieces.push_back(wallCard(ctx, hang, wk.text, "gallery1:label:" + wk.image,
                                      kLabelPixels, kLabelWidth, 0.5f, labelTop, true));
        }

        // ---- 2. the title card, by the door --------------------------------
        if (kTitleHang < hangs.size()) {
            const HangPoint hang = onWall(hangs[kTitleHang], r, halfWall);
            pieces.push_back(wallCard(ctx, hang, titleCard(), "gallery1:title", kTitlePixels,
                                      kTitleMaxW, kTitleMaxH, kTitleCentreY, false));
        }

        // ---- 3. the bench ----------------------------------------------------
        {
            const glm::vec3 base = w3(kBenchX, 0.0f, kBenchY);
            const float legH = kBenchH - kSeatT;

            Piece seat;
            seat.mesh = build::buildBox(base + glm::vec3(0.0f, kBenchH - kSeatT * 0.5f, 0.0f),
                                        glm::vec3(kBenchL, kSeatT, kBenchD));
            seat.material.tint      = kBenchWood;
            seat.material.shading   = cfg::Shading::BlinnPhong;
            seat.material.specular  = 0.20f;   // oiled, not varnished
            seat.material.shininess = 30.0f;
            seat.note = "bench seat";
            pieces.push_back(std::move(seat));

            gfx::MeshData legs;
            for (float side : {-1.0f, 1.0f}) {
                const glm::vec3 c = base + glm::vec3(side * (kBenchL * 0.5f - 0.15f), legH * 0.5f, 0.0f);
                legs.append(build::buildBox(c, glm::vec3(kLegT, legH, kBenchD - 0.06f)), glm::mat4(1.0f));
            }
            Piece legPiece;
            legPiece.mesh = std::move(legs);
            legPiece.material.tint      = kFrameColour;
            legPiece.material.shading   = cfg::Shading::BlinnPhong;
            legPiece.material.specular  = 0.30f;
            legPiece.material.shininess = 50.0f;
            legPiece.note = "bench legs";
            pieces.push_back(std::move(legPiece));
        }

        return pieces;
    }

    // The bench is solid, so the visitor walks round it.
    std::vector<Footprint> footprints() const override {
        return {Footprint{Vec2{kBenchX, kBenchY},
                          Vec2{kBenchL * 0.5f + kSolidMargin, kBenchD * 0.5f + kSolidMargin}}};
    }

    std::string techniqueNotes() const override {
        return
            "Gallery One: Light on the Land (Timothy Kimani)\n"
            "  Six photographs, each framed to its own shape from the image's size.\n"
            "  Prints behind glass: Blinn-Phong per pixel with a weak specular and a\n"
            "    high shininess, so the lamp leaves a small sharp highlight that\n"
            "    slides across the glass as you walk.\n"
            "  The aurora is a lightbox: emissive, so it glows in the dark and\n"
            "    still takes the glass highlight on top.\n"
            "  Satin black frames: a Bezier moulding swept round each print.\n"
            "  The title card and labels are drawn by our own 2D code with the\n"
            "    bitmap font, then shown as textures.\n"
            "  The hang points are moved from the plan onto the wall face.\n"
            "  An oak bench faces Mount Kenya, and is a solid footprint.\n";
    }
};

} // namespace

RoomPtr makeRoomOne() { return std::make_unique<RoomOne>(); }

} // namespace lumen
