#include "Sculpture.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include "Builders.h"
#include "Label.h"
#include "Model.h"

namespace lumen::sculpture {
namespace {

using json = nlohmann::json;

constexpr float kPi = 3.14159265358979f;

[[noreturn]] void fail(const std::string& path, const std::string& why) {
    throw std::runtime_error("Could not read '" + path + "': " + why);
}

Finish finishFrom(const std::string& s, const std::string& path) {
    Finish f = Finish::Scan;
    if (!parseFinish(s, f)) fail(path, "unknown finish '" + s + "' (use scan, marble or bronze)");
    return f;
}

bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

glm::vec3 world(Vec2 p, float y) { return glm::vec3(worldX(p.x), y, worldZ(p.y)); }

constexpr float kSlabBorder  = 0.12f;   // stone showing round a relief's image
constexpr float kLabelWidth  = 0.44f;   // m, a label card at most
constexpr float kLabelGap    = 0.10f;   // m from the slab to its label
constexpr float kLabelHeight = 1.35f;   // m to the centre of a wall label

// The reading stand in front of the atrium statue: a post with a sloping top.
constexpr float kStandGap    = 0.65f;   // m from the plinth's edge
constexpr float kStandSide   = 0.35f;   // m to the visitor's left, clear of the statue
constexpr float kStandHeight = 0.92f;   // m to the middle of the sloping top
constexpr float kStandSlope  = 30.0f;   // degrees the top tips towards the visitor
constexpr float kStandHalf   = 0.22f;   // m, half its footprint, for collision

// A label card for a statue or relief.
raster::Canvas labelFor(const std::string& title, const std::string& about,
                        const std::string& credit) {
    label::Text t;
    t.title  = title;
    if (!about.empty()) t.lines.push_back(about);
    t.credit = credit;
    return label::draw(t);
}

} // namespace

// The spot for one statue, moved forward if it asks.
static Spot statueSpot(const Plan& plan, const StatueSpec& st) {
    Spot s = spotFor(plan, st.place);
    s.at.x += s.facing.x * st.forward;
    s.at.y += s.facing.y * st.forward;
    return s;
}

// Where a statue's reading stand goes. Only the atrium statue has one; the
// niche statues carry their labels on their plinths.
static bool standFor(const Spot& spot, const StatueSpec& st, Vec2& at) {
    if (spot.zone != "ATRIUM") return false;
    const Vec2 f = spot.facing;
    const Vec2 left{f.y, -f.x};   // the visitor's left, looking at the statue
    const float out = st.plinth.x * 0.5f + kStandGap;
    at = {spot.at.x + f.x * out + left.x * kStandSide, spot.at.y + f.y * out + left.y * kStandSide};
    return true;
}

bool parseFinish(const std::string& name, Finish& out) {
    if (name == "scan")   { out = Finish::Scan;   return true; }
    if (name == "marble") { out = Finish::Marble; return true; }
    if (name == "bronze") { out = Finish::Bronze; return true; }
    return false;
}

gfx::Material statueMaterial(const model::Part& part, Finish finish,
                             const std::string& assetDir, gfx::TextureCache& textures) {
    gfx::Material mat;
    // The scan's normal map keeps its carved detail under any finish.
    if (!part.normalPath.empty() && part.hasTangents && part.hasUVs)
        mat.normal = textures.get(part.normalPath, gfx::Texture::Kind::Data);
    if (finish == Finish::Scan) {
        if (!part.albedoPath.empty()) mat.albedo = textures.get(part.albedoPath);
        mat.tint      = glm::vec3(part.baseColour);
        mat.shading   = mat.normal ? cfg::Shading::NormalMapped : cfg::Shading::BlinnPhong;
        mat.specular  = 0.22f;
        mat.shininess = 40.0f;
    } else if (finish == Finish::Marble) {
        mat.albedo    = textures.get(assetDir + "materials/marble.jpg");
        mat.tint      = glm::vec3(0.96f, 0.94f, 0.90f);
        mat.shading   = cfg::Shading::Triplanar;
        mat.triplanarScale = 1.3f;
        mat.specular  = 0.35f;
        mat.shininess = 60.0f;
    } else {
        mat.albedo    = textures.get(assetDir + "materials/plaster.jpg");
        mat.tint      = glm::vec3(0.15f, 0.14f, 0.09f);
        mat.shading   = cfg::Shading::Triplanar;
        mat.triplanarScale = 1.6f;
        mat.specular  = 0.90f;
        mat.shininess = 48.0f;
    }
    return mat;
}

// ---- the catalogue ------------------------------------------------------------

Catalogue load(const std::string& path) {
    std::ifstream f(path);
    if (!f) fail(path, "the file is missing");
    json j;
    try {
        j = json::parse(f);
    } catch (const std::exception& e) {
        fail(path, std::string("the JSON is broken: ") + e.what());
    }

    Catalogue cat;
    try {
        for (const json& s : j.value("statues", json::array())) {
            StatueSpec st;
            st.title   = s.at("title").get<std::string>();
            st.model   = s.at("model").get<std::string>();
            st.place   = s.at("place").get<std::string>();
            st.about   = s.value("about", std::string());
            st.credit  = s.value("credit", std::string());
            st.height  = s.value("height", st.height);
            st.turnDeg = s.value("turn", 0.0f);
            st.forward = s.value("forward", 0.0f);
            if (s.contains("plinth")) {
                const auto p = s.at("plinth").get<std::vector<float>>();
                if (p.size() != 3) fail(path, st.title + ": plinth needs width, height and depth");
                st.plinth = {p[0], p[1], p[2]};
            }
            st.finish = finishFrom(s.value("finish", std::string("scan")), path);
            cat.statues.push_back(st);
        }
        for (const json& r : j.value("reliefs", json::array())) {
            ReliefSpec rl;
            rl.title  = r.at("title").get<std::string>();
            rl.image  = r.at("image").get<std::string>();
            rl.about  = r.value("about", std::string());
            rl.credit = r.value("credit", std::string());
            const std::string wall = r.at("wall").get<std::string>();
            if (wall.size() != 1 || std::string("NSEW").find(wall[0]) == std::string::npos)
                fail(path, rl.title + ": wall must be N, S, E or W");
            rl.wall  = wall[0];
            rl.at    = r.at("at").get<float>();
            rl.width = r.value("width", rl.width);
            rl.centreHeight = r.value("centre_height", rl.centreHeight);
            cat.reliefs.push_back(rl);
        }
    } catch (const json::exception& e) {
        fail(path, std::string("a required field is missing: ") + e.what());
    }
    return cat;
}

// ---- placing --------------------------------------------------------------------

Spot spotFor(const Plan& plan, const std::string& place) {
    Spot s;
    if (place == "atrium") {
        // Under the oculus, facing the vestibule, so it is the first thing you see.
        s.at   = plan.oculusCentre;
        s.zone = "ATRIUM";
        const Vec2 v = plan.vestibuleRect.centre();
        const float dx = v.x - s.at.x, dy = v.y - s.at.y;
        if (std::fabs(dx) >= std::fabs(dy)) s.facing = {dx < 0.0f ? -1.0f : 1.0f, 0.0f};
        else                                s.facing = {0.0f, dy < 0.0f ? -1.0f : 1.0f};
        return s;
    }

    int n = -1;
    const std::string prefix = "niche ";
    if (place.size() == prefix.size() + 1 && place.compare(0, prefix.size(), prefix) == 0 &&
        place.back() >= '0' && place.back() <= '9')
        n = place.back() - '0';
    if (n < 0 || n >= static_cast<int>(plan.niches.size()))
        throw std::runtime_error("Unknown place '" + place + "'. Use \"atrium\" or \"niche 0\" to \"niche " +
                                 std::to_string(static_cast<int>(plan.niches.size()) - 1) + "\".");

    // A niche is cut into the corridor's outer wall. It faces back across the
    // corridor, through the side it shares with the corridor.
    const Rect& r = plan.niches[static_cast<std::size_t>(n)];
    const Rect& c = plan.corridorOuter;
    s.at   = r.centre();
    s.zone = "NICHE";
    if      (near(r.y0, c.y1)) s.facing = { 0.0f, -1.0f};
    else if (near(r.y1, c.y0)) s.facing = { 0.0f,  1.0f};
    else if (near(r.x1, c.x0)) s.facing = { 1.0f,  0.0f};
    else if (near(r.x0, c.x1)) s.facing = {-1.0f,  0.0f};
    else throw std::runtime_error("Niche " + std::to_string(n) + " does not open onto the corridor.");
    return s;
}

glm::mat4 statueTransform(const Spot& spot, float plinthHeight, float turnDeg) {
    // Turning by a about +Y sends +Z to (sin a, 0, cos a). Pick a so that
    // points along the facing direction in world space.
    const glm::vec3 f = world(spot.facing, 0.0f);
    const float a = std::atan2(f.x, f.z) + turnDeg * kPi / 180.0f;
    return glm::translate(glm::mat4(1.0f), world(spot.at, plinthHeight)) *
           glm::rotate(glm::mat4(1.0f), a, glm::vec3(0.0f, 1.0f, 0.0f));
}

WallSpot wallSpotFor(const Plan& plan, char wall, float at, float centreHeight) {
    const Rect& r = plan.atriumRect;
    const float half = plan.wallThickness * 0.5f;
    Vec2 p, n;
    switch (wall) {
        case 'N': p = {at, r.y1 + half}; n = { 0.0f,  1.0f}; break;
        case 'S': p = {at, r.y0 - half}; n = { 0.0f, -1.0f}; break;
        case 'E': p = {r.x1 + half, at}; n = { 1.0f,  0.0f}; break;
        case 'W': p = {r.x0 - half, at}; n = {-1.0f,  0.0f}; break;
        default: throw std::runtime_error(std::string("Unknown wall '") + wall + "'.");
    }
    WallSpot s;
    s.centre = world(p, centreHeight);
    s.normal = glm::vec3(n.x, 0.0f, -n.y);
    // Looking at the wall means looking along -normal; right is that crossed with up.
    s.right  = glm::cross(-s.normal, glm::vec3(0.0f, 1.0f, 0.0f));
    return s;
}

Panel buildPanel(const WallSpot& spot, float width, float height) {
    constexpr float kDepth  = 0.10f;   // how far the slab stands out
    constexpr float kBorder = kSlabBorder;   // stone showing round the image
    constexpr float kLift   = 0.003f;  // the image just in front of the slab

    // Local x = right, y = up, z = out of the wall, origin on the wall face.
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::mat4 basis(1.0f);
    basis[0] = glm::vec4(spot.right, 0.0f);
    basis[1] = glm::vec4(up, 0.0f);
    basis[2] = glm::vec4(spot.normal, 0.0f);
    basis[3] = glm::vec4(spot.centre, 1.0f);

    Panel p;
    const gfx::MeshData slab = build::buildBox(glm::vec3(0.0f, 0.0f, kDepth * 0.5f),
                                               glm::vec3(width + 2.0f * kBorder,
                                                         height + 2.0f * kBorder, kDepth));
    p.slab.append(slab, basis);

    gfx::MeshData face;
    const float w = width * 0.5f, h = height * 0.5f, z = kDepth + kLift;
    face.addQuad({-w, -h, z}, {w, -h, z}, {w, h, z}, {-w, h, z});
    p.face.append(face, basis);
    return p;
}

std::vector<Segment> plinthOutlines(const Plan& plan, const Catalogue& cat) {
    std::vector<Segment> out;
    const float inset = plan.wallThickness * 0.5f;
    for (const StatueSpec& st : cat.statues) {
        Spot spot;
        try {
            spot = statueSpot(plan, st);
        } catch (const std::exception&) {
            continue;   // build() reports it
        }
        // Width runs along the wall the statue faces away from.
        const bool alongX = std::fabs(spot.facing.y) > 0.5f;
        const float hx = std::max(0.05f, (alongX ? st.plinth.x : st.plinth.z) * 0.5f - inset);
        const float hy = std::max(0.05f, (alongX ? st.plinth.z : st.plinth.x) * 0.5f - inset);
        const Vec2 c = spot.at;
        const Vec2 a{c.x - hx, c.y - hy}, b{c.x + hx, c.y - hy},
                   d{c.x + hx, c.y + hy}, e{c.x - hx, c.y + hy};
        out.push_back({a, b});
        out.push_back({b, d});
        out.push_back({d, e});
        out.push_back({e, a});

        Vec2 at;
        if (standFor(spot, st, at)) {
            const float h = std::max(0.05f, kStandHalf - inset);
            const Vec2 p{at.x - h, at.y - h}, q{at.x + h, at.y - h},
                       r{at.x + h, at.y + h}, t{at.x - h, at.y + h};
            out.push_back({p, q});
            out.push_back({q, r});
            out.push_back({r, t});
            out.push_back({t, p});
        }
    }
    return out;
}

// ---- building -------------------------------------------------------------------

std::vector<Item> build(const Plan& plan, const Catalogue& cat,
                        const std::string& assetDir, gfx::TextureCache& textures) {
    std::vector<Item> items;
    const gfx::Texture* marble  = textures.get(assetDir + "materials/marble.jpg");

    // A grey veined stone for plinths and slabs.
    gfx::Material stone;
    stone.albedo    = marble;
    stone.tint      = glm::vec3(0.50f, 0.48f, 0.45f);
    stone.specular  = 0.20f;
    stone.shininess = 48.0f;

    for (const StatueSpec& st : cat.statues) {
        model::Model m;
        Spot spot;
        try {
            spot = statueSpot(plan, st);
            m = model::load(assetDir + st.model);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[lumen] skipping %s. %s\n", st.title.c_str(), e.what());
            continue;
        }

        const glm::mat4 place = statueTransform(spot, st.plinth.y, st.turnDeg) *
                                model::fitToHeight(m, st.height);
        const std::size_t triangles = m.triangleCount();

        for (model::Part& part : m.parts) {
            Item it;
            it.transform = place;
            it.zone      = spot.zone;
            it.note      = st.title;
            it.material = statueMaterial(part, st.finish, assetDir, textures);
            it.mesh = std::move(part.mesh);
            items.push_back(std::move(it));
        }

        // The plinth: a drum under the oculus, a block in a niche.
        Item plinth;
        plinth.material = stone;
        plinth.zone     = spot.zone;
        plinth.note     = st.title + " plinth";
        const glm::vec3 base = world(spot.at, 0.0f);
        if (spot.zone == "ATRIUM") {
            plinth.mesh = build::buildCylinder(base, st.plinth.x * 0.5f, st.plinth.y, 48);
        } else {
            const bool alongX = std::fabs(spot.facing.y) > 0.5f;   // facing north or south
            plinth.mesh = build::buildPlinth(base, alongX ? st.plinth.x : st.plinth.z,
                                             alongX ? st.plinth.z : st.plinth.x, st.plinth.y);
        }
        items.push_back(std::move(plinth));

        const raster::Canvas card = labelFor(st.title, st.about, st.credit);
        const gfx::Texture* cardTex = textures.fromCanvas("label:" + st.title, card);
        const glm::vec3 front = world(spot.facing, 0.0f) - world({0.0f, 0.0f}, 0.0f);
        const glm::vec3 right = glm::cross(-front, glm::vec3(0.0f, 1.0f, 0.0f));   // the visitor's right
        Vec2 standAt;
        if (standFor(spot, st, standAt)) {
            // A reading stand, as museums put in front of a statue in the round.
            // Built facing +Z, then turned to face the visitor.
            glm::mat4 basis(1.0f);
            basis[0] = glm::vec4(right, 0.0f);
            basis[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
            basis[2] = glm::vec4(front, 0.0f);
            basis[3] = glm::vec4(world(standAt, 0.0f), 1.0f);
            const glm::mat4 top = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, kStandHeight, 0.0f)) *
                                  glm::rotate(glm::mat4(1.0f), glm::radians(kStandSlope),
                                              glm::vec3(1.0f, 0.0f, 0.0f));

            gfx::MeshData stand;
            stand.append(build::buildBox({0.0f, 0.015f, 0.0f}, {0.36f, 0.03f, 0.28f}), basis);
            const float post = kStandHeight - 0.06f;   // stops under the sloping top
            stand.append(build::buildBox({0.0f, post * 0.5f, 0.0f}, {0.08f, post, 0.08f}), basis);
            stand.append(build::buildBox({0.0f, 0.0f, 0.0f}, {0.50f, 0.025f, 0.36f}), basis * top);

            Item body;
            body.mesh = std::move(stand);
            body.material.tint      = glm::vec3(0.13f, 0.12f, 0.11f);   // dark bronze
            body.material.specular  = 0.35f;
            body.material.shininess = 40.0f;
            body.zone = spot.zone;
            body.note = st.title + " label stand";
            items.push_back(std::move(body));

            const glm::vec2 size = label::fit(card, 0.44f, 0.30f);
            Item card3d;
            card3d.mesh.append(label::quad({0.0f, 0.0145f, 0.0f}, {1.0f, 0.0f, 0.0f},
                                           {0.0f, 1.0f, 0.0f}, size), basis * top);
            card3d.material = label::material(cardTex);
            card3d.zone     = spot.zone;
            card3d.note     = st.title + " label";
            items.push_back(std::move(card3d));
        } else {
            // On the front of the plinth, facing the visitor.
            const glm::vec2 size = label::fit(card, std::min(kLabelWidth, st.plinth.x - 0.12f),
                                              st.plinth.y - 0.12f);
            const glm::vec3 centre = base + front * (st.plinth.z * 0.5f + 0.004f) +
                                     glm::vec3(0.0f, st.plinth.y * 0.5f, 0.0f);
            Item card3d;
            card3d.mesh     = label::quad(centre, right, front, size);
            card3d.material = label::material(cardTex);
            card3d.zone     = spot.zone;
            card3d.note     = st.title + " label";
            items.push_back(std::move(card3d));
        }

        std::printf("[lumen] %-36s %7zu triangles, %s\n", st.title.c_str(),
                    triangles, st.place.c_str());
    }

    // A marble circle in the atrium floor, under the oculus.
    {
        Item inlay;
        inlay.mesh = build::buildCylinder(world(plan.oculusCentre, 0.0f),
                                          plan.oculusRadius, 0.006f, 96);
        inlay.material = stone;
        inlay.material.tint = glm::vec3(0.78f, 0.75f, 0.70f);
        inlay.zone = "ATRIUM";
        inlay.note = "oculus floor circle";
        items.push_back(std::move(inlay));
    }

    for (const ReliefSpec& rl : cat.reliefs) {
        const std::string path = assetDir + rl.image;
        const gfx::Texture* img = textures.get(path);
        const float aspect = (img && img->width() > 0)
                                 ? static_cast<float>(img->height()) / static_cast<float>(img->width())
                                 : 1.0f;
        const float height = rl.width * aspect;
        const Panel panel = buildPanel(wallSpotFor(plan, rl.wall, rl.at, rl.centreHeight),
                                       rl.width, height);

        Item face;
        face.mesh = panel.face;
        face.material.albedo    = img;
        face.material.normal    = textures.reliefFrom(path, 2.4f);
        face.material.shading   = cfg::Shading::NormalMapped;
        face.material.specular  = 0.08f;
        face.material.shininess = 20.0f;
        face.note = rl.title;
        items.push_back(std::move(face));

        Item slab;
        slab.mesh     = panel.slab;
        slab.material = stone;
        slab.note     = rl.title + " slab";
        items.push_back(std::move(slab));

        // The label, on the wall to the right of the slab, at reading height.
        const WallSpot spot = wallSpotFor(plan, rl.wall, rl.at, rl.centreHeight);
        const raster::Canvas card = labelFor(rl.title, rl.about, rl.credit);
        const glm::vec2 size = label::fit(card, kLabelWidth, 0.4f);
        const float slabEdge = rl.width * 0.5f + kSlabBorder;
        glm::vec3 centre = spot.centre + spot.right * (slabEdge + kLabelGap + size.x * 0.5f) +
                           spot.normal * 0.004f;
        centre.y = kLabelHeight;
        Item card3d;
        card3d.mesh     = label::quad(centre, spot.right, spot.normal, size);
        card3d.material = label::material(textures.fromCanvas("label:" + rl.image, card));
        card3d.note     = rl.title + " label";
        items.push_back(std::move(card3d));
    }
    return items;
}

} // namespace lumen::sculpture
