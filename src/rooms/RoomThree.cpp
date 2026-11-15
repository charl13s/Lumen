// RoomThree.cpp - Gallery Three: the Hall of Champions. Owner: Shawn Kuria (158657)
//
// The largest room, and the furthest from the entrance: the end of the tour.
// Six of the greatest moments in sport, and the same man in every one of them,
// in a hall dressed in gold.
//
//   The hall: gold damask walls (the pattern drawn by our own code) over a
//   darker panel band, with skirting, dado rail and crown moulding; a gilded
//   vault; a black terrazzo floor with gold chips; a gold runner from the door.
//   East wall, facing the door: a portrait printed four ways by our own code
//   (src/core/PopArt.cpp), under the room's title, between two gold athletes.
//   Side walls and the east wall: six champions as glossy prints in double
//   gold frames, each with a brass picture light and a wall label.
//   Under the far vault: five interlocking gold rings. Under both vaults:
//   four chandeliers with lit candles.
//   In the middle: six gold pedestals inside gold stanchions and ropes, each
//   with the trophy from one of the photographs, turning slowly, and spinning
//   faster whenever the visitor walks in.
//
// Techniques: Blinn-Phong on metal (gold, silver, bronze tints, tight
// highlights) against matte labels and carpet; emissive candle flames and
// picture lights; textures drawn by our own raster code (pop art, damask,
// terrazzo, labels, title, medal face); trophies turned on a lathe from cubic
// Bezier profiles; our own meshes for the torus, sphere, ellipsoid, medal, plates
// and swept tubes; ropes that sag along a quadratic Bezier from core/Curves;
// per-frame model transforms for the turntables: T(home) x Ry(angle + phase) x S.

#include "core/Room.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

#include "core/Curves.h"
#include "core/Font.h"
#include "core/PopArt.h"

namespace lumen {
namespace {

constexpr float kPi = 3.14159265358979f;

// ---- this room's numbers ---------------------------------------------------

// Hanging.
constexpr float kChampionCentre  = 1.78f;  // m, centre height of a champion print
constexpr float kChampionMaxH    = 1.45f;  // m, tallest a print may be
constexpr float kWallShare       = 0.78f;  // of the hang width, the widest a print may be
constexpr float kPopArtCentre    = 1.95f;  // m, the pop art hangs higher, it is bigger
constexpr float kPopArtMaxH      = 2.05f;  // m

constexpr float kOuterFrameGap   = 0.105f; // m, where the outer moulding starts, past the canvas
constexpr float kFrameExtent     = kOuterFrameGap + cfg::kFrameWidth;   // m, canvas edge to frame edge
constexpr float kLabelW          = 0.64f;  // m
constexpr float kLabelH          = 0.18f;  // m
constexpr float kLabelGap        = 0.05f;  // m between frame and label
constexpr float kTitleCentre     = 3.74f;  // m, the room title above the pop art
constexpr float kTitleW          = 3.00f;  // m
constexpr float kTitleH          = 0.46f;  // m

// The walls. Every lining stands this far off the plan's wall face, and the
// works are moved onto it.
constexpr float kPanelOut        = 0.01f;  // m
constexpr float kDamaskTile      = 0.90f;  // m, one repeat of the wallpaper
constexpr float kSkirtingTop     = 0.12f;  // m
constexpr float kDadoBottom      = 0.48f;  // m
constexpr float kDadoTop         = 0.53f;  // m
constexpr float kCrownBottom     = 4.05f;  // m
constexpr float kFloorTile       = 2.50f;  // m, one repeat of the terrazzo
constexpr float kRunnerWidth     = 1.60f;  // m
constexpr float kVaultInset      = 0.02f;  // m, the gilded vault sits this far inside the plain one

// The trophies: one per champion, on six pedestals in a curve facing the door.
constexpr float kPedestalSize    = 0.50f;  // m, square
constexpr float kPedestalSpacing = 0.95f;  // m between pedestal centres, across the room
constexpr float kPedestalBow     = 0.55f;  // m the two ends stand nearer the door than the middle
constexpr float kPedestalHeights[6] = {0.95f, 1.00f, 1.08f, 1.08f, 1.00f, 0.95f};
constexpr float kTrophyScale     = 1.40f;  // the trophies are designed life size, then shown larger
constexpr float kSpinDegPerSec   = 30.0f;  // the turntables
constexpr float kCheerDegPerSec  = 360.0f; // extra spin as the visitor walks in
constexpr float kCheerDecay      = 1.2f;   // per second

// The stanchions round the trophies.
constexpr float kBarrierMargin   = 0.60f;  // m from the pedestals to the posts
constexpr float kPostHeight      = 0.95f;  // m
constexpr float kRopeHeight      = 0.88f;  // m, where a rope leaves a post
constexpr float kRopeSag         = 0.055f; // m of sag per metre of rope
constexpr float kRopeRadius      = 0.018f; // m

// The rings, after the Olympic ones, hanging under the far (east) vault, so
// they are the first thing you see from the door.
constexpr float kRingRadius      = 0.50f;  // m, to the middle of the tube
constexpr float kRingTube        = 0.055f; // m
constexpr float kRingsTopCentre  = 4.60f;  // m, height of the top row's centres
constexpr float kRingTilt        = 7.0f;   // degrees, alternate rings lean so they weave
constexpr float kHangTop         = 5.85f;  // m, where rods meet the vault crown

// Four chandeliers, two under each vault, either side of the rings.
constexpr float kChandelierSpread = 4.0f;  // m north and south of the room's middle
constexpr float kChandelierY     = 4.55f;  // m, height of the lower hoop

const glm::vec3 kGold        {1.00f, 0.74f, 0.30f};
const glm::vec3 kAntiqueGold {0.74f, 0.52f, 0.20f};
const glm::vec3 kPaleGold    {1.00f, 0.86f, 0.55f};

const glm::vec3 kMedalGold   {1.00f, 0.66f, 0.12f};

// Ribbon stripes: the Kenyan flag for the marathon, Jamaica's for the sprint.
const glm::vec3 kKenya[3]   = {{0.06f, 0.06f, 0.06f}, {0.72f, 0.06f, 0.08f}, {0.02f, 0.42f, 0.18f}};
const glm::vec3 kJamaica[3] = {{0.02f, 0.45f, 0.18f}, {0.95f, 0.75f, 0.05f}, {0.06f, 0.06f, 0.06f}};

const raster::Colour kLabelBg   {30, 26, 22, 255};
// The gilt texture's average colour. Canvases drawn as gold start from it, so
// once tinted they match the gold that uses the texture itself.
const raster::Colour kGiltBase  {175, 142, 77, 255};
const raster::Colour kGiltDeep  {118, 92, 46, 255};
const raster::Colour kLabelGold {226, 192, 116, 255};
const raster::Colour kLabelText {206, 198, 182, 255};

// ---- the champions ---------------------------------------------------------
//
// Credits for every photograph are in assets/works.json.

struct Champion {
    const char* file;
    const char* name;
    const char* line1;
    const char* line2;
};

// In hanging order, matched to the plan's hang points in turn (the middle of
// the east wall is kept for the pop art). Walking in from the door: the east
// wall ahead, the north wall on the left, the south wall on the right.
const Champion kChampions[] = {
    {"works/hall-of-fame/ushawn-bolt.jpg",      "USHAWN BOLT",      "100 M WORLD RECORD - 9.58 S",   "BERLIN, 2009"},
    {"works/hall-of-fame/leshawn-messi.jpg",    "LESHAWN MESSI",    "BALLON D'OR - EIGHT TIMES",     "AND COUNTING"},
    {"works/hall-of-fame/leshawn-james.jpg",    "LESHAWN JAMES",    "NBA CHAMPION AND FINALS MVP",   "MIAMI, 2013"},
    {"works/hall-of-fame/connor-mckuria.jpg",   "CONNOR MCKURIA",   "UFC CHAMPION, TWO DIVISIONS",   "AT THE SAME TIME"},
    {"works/hall-of-fame/eliud-kipkuria.jpg",   "ELIUD KIPKURIA",   "FIRST MARATHON UNDER 2 HOURS",  "1:59:40.2 - VIENNA, 2019"},
    {"works/hall-of-fame/lewis-hamilshawn.jpg", "LEWIS HAMILSHAWN", "FORMULA ONE WORLD CHAMPION",    "SEVEN TITLES"},
};
constexpr std::size_t kChampionCount = sizeof(kChampions) / sizeof(kChampions[0]);

constexpr const char* kFacePhoto = "works/hall-of-fame/face.jpg";

// Two gold athletes in the east corners, either side of the pop art.
struct Athlete {
    const char* model;
    float       height;   // m
};
const Athlete kAthletes[2] = {
    {"models/discobolus_the_discus_thrower/scene.gltf", 1.45f},
    {"models/apollo_belvedere/scene.gltf",              1.75f},
};
constexpr float kAthletePlinth   = 0.60f;  // m tall
constexpr float kAthleteFromWall = 0.95f;  // m from each wall of the corner

// ---- meshes ------------------------------------------------------------------

using gfx::MeshData;
using gfx::Vertex;

// Adds a quad with its own normals, wound so its front faces the way the
// normals point. Our torus, sphere and tubes are built from these.
void addSmoothQuad(MeshData& m, const std::array<Vertex, 4>& v) {
    // The diagonals' cross product: unlike two edges, it survives a quad that
    // collapses to a triangle at a pole.
    const glm::vec3 face = glm::cross(v[2].position - v[0].position, v[3].position - v[1].position);
    const glm::vec3 want = v[0].normal + v[1].normal + v[2].normal + v[3].normal;
    if (glm::dot(face, want) >= 0.0f) {
        m.addTriangle(v[0], v[1], v[2]);
        m.addTriangle(v[0], v[2], v[3]);
    } else {
        m.addTriangle(v[0], v[2], v[1]);
        m.addTriangle(v[0], v[3], v[2]);
    }
}

// A torus round the x axis: the ring lies in the y-z plane, centred on the
// origin. A point is the ring's centre line, R (0, cos u, sin u), pushed out
// by the tube radius r along (sin v, cos v cos u, cos v sin u).
MeshData buildTorus(float R, float r, int segU = 64, int segV = 16) {
    MeshData m;
    auto at = [&](int i, int j) {
        const float u = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segU);
        const float w = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(segV);
        const glm::vec3 radial(0.0f, std::cos(u), std::sin(u));
        const glm::vec3 n = std::cos(w) * radial + std::sin(w) * glm::vec3(1.0f, 0.0f, 0.0f);
        Vertex v;
        v.position = R * radial + r * n;
        v.normal   = n;
        v.uv       = {static_cast<float>(i) / static_cast<float>(segU) * 6.0f,
                      static_cast<float>(j) / static_cast<float>(segV)};
        v.tangent  = glm::vec3(0.0f, -std::sin(u), std::cos(u));
        return v;
    };
    for (int i = 0; i < segU; ++i)
        for (int j = 0; j < segV; ++j)
            addSmoothQuad(m, {at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1)});
    return m;
}

// A sphere from latitude and longitude bands.
MeshData buildSphere(const glm::vec3& c, float r, int seg = 16) {
    MeshData m;
    auto at = [&](int i, int j) {
        const float lon = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(seg);
        const float lat = kPi * static_cast<float>(j) / static_cast<float>(seg) - kPi * 0.5f;
        const glm::vec3 n(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
        Vertex v;
        v.position = c + r * n;
        v.normal   = n;
        v.uv       = {static_cast<float>(i) / static_cast<float>(seg), static_cast<float>(j) / static_cast<float>(seg)};
        v.tangent  = glm::vec3(-std::sin(lon), 0.0f, std::cos(lon));
        return v;
    };
    for (int i = 0; i < seg; ++i)
        for (int j = 0; j < seg; ++j)
            addSmoothQuad(m, {at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1)});
    return m;
}

// A round tube swept along a path, for ropes and chandelier arms. Each point
// gets a ring of vertices square to the path's direction there.
MeshData buildTube(const std::vector<glm::vec3>& path, float r, int sides = 10,
                   const std::vector<float>& radii = {}) {
    MeshData m;
    if (path.size() < 2) return m;
    sides = std::clamp(sides, 3, 31);
    std::vector<std::array<Vertex, 32>> rings;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const glm::vec3 prev = path[i == 0 ? 0 : i - 1];
        const glm::vec3 next = path[std::min(i + 1, path.size() - 1)];
        const glm::vec3 t = glm::normalize(next - prev);
        const glm::vec3 ref = std::fabs(t.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        const glm::vec3 a = glm::normalize(glm::cross(t, ref));
        const glm::vec3 b = glm::cross(t, a);
        std::array<Vertex, 32> ring{};
        const float ri = i < radii.size() ? radii[i] : r;
        for (int k = 0; k <= sides; ++k) {
            const float ang = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(sides);
            const glm::vec3 n = std::cos(ang) * a + std::sin(ang) * b;
            ring[static_cast<std::size_t>(k)] =
                Vertex{path[i] + ri * n, n,
                       {static_cast<float>(k) / static_cast<float>(sides), static_cast<float>(i)}, t};
        }
        rings.push_back(ring);
    }
    for (std::size_t i = 0; i + 1 < rings.size(); ++i)
        for (int k = 0; k < sides; ++k) {
            const auto kk = static_cast<std::size_t>(k);
            addSmoothQuad(m, {rings[i][kk], rings[i + 1][kk], rings[i + 1][kk + 1], rings[i][kk + 1]});
        }
    return m;
}

// A rope hung between two points: a quadratic Bezier whose control point sits
// below the middle by twice the sag, so the curve's lowest point is `sag` down.
std::vector<glm::vec3> saggingRope(const glm::vec3& a, const glm::vec3& b, float sag, int steps = 18) {
    const float span = glm::length(glm::vec2(b.x - a.x, b.z - a.z));
    // In the rope's own vertical plane: x along the ground, y up.
    const curves::P2 p0{0.0f, a.y}, p2{span, b.y};
    const curves::P2 p1{span * 0.5f, (a.y + b.y) * 0.5f - 2.0f * sag};
    std::vector<glm::vec3> out;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const curves::P2 q = curves::bezierQuadratic(p0, p1, p2, t);
        const float f = span > 0.0f ? q.x / span : t;
        out.push_back(glm::vec3(a.x + (b.x - a.x) * f, q.y, a.z + (b.z - a.z) * f));
    }
    return out;
}

// A disc with both faces and a rim, centred on the origin, facing along x.
// The faces are mapped flat, so a square texture shows as a round face.
MeshData buildMedal(float radius, float thickness, int segments = 48) {
    MeshData m;
    const float hx = thickness * 0.5f;
    auto onRim = [&](int i) {
        const float t = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        return glm::vec2(std::cos(t), std::sin(t));   // (z, y) on the unit circle
    };
    for (int side = 0; side < 2; ++side) {
        // side 0 faces -x (towards the door), side 1 faces +x.
        const float sx = side == 0 ? -1.0f : 1.0f;
        const glm::vec3 nrm(sx, 0.0f, 0.0f);
        const glm::vec3 tangent(0.0f, 0.0f, -sx);
        const Vertex centre{{sx * hx, 0.0f, 0.0f}, nrm, {0.5f, 0.5f}, tangent};
        for (int i = 0; i < segments; ++i) {
            const glm::vec2 a = onRim(i), b = onRim(i + 1);
            // Seen from this side, +z is to the right for -x, and to the left for +x.
            const Vertex va{{sx * hx, a.y * radius, a.x * radius}, nrm,
                            {0.5f - sx * a.x * 0.5f, 0.5f + a.y * 0.5f}, tangent};
            const Vertex vb{{sx * hx, b.y * radius, b.x * radius}, nrm,
                            {0.5f - sx * b.x * 0.5f, 0.5f + b.y * 0.5f}, tangent};
            if (side == 0) m.addTriangle(centre, va, vb);   // counter-clockwise from outside
            else           m.addTriangle(centre, vb, va);
        }
    }
    for (int i = 0; i < segments; ++i) {   // the rim
        const glm::vec2 a = onRim(i), b = onRim(i + 1);
        const glm::vec3 pa(0.0f, a.y * radius, a.x * radius), pb(0.0f, b.y * radius, b.x * radius);
        m.addQuad(pa + glm::vec3(-hx, 0, 0), pa + glm::vec3(hx, 0, 0),
                  pb + glm::vec3(hx, 0, 0), pb + glm::vec3(-hx, 0, 0));
    }
    return m;
}

// ---- trophies: surfaces of revolution and friends ---------------------------

using curves::P2;

// A straight piece of profile, written as a cubic Bezier so it mixes with curved ones.
std::array<P2, 4> straight(P2 a, P2 b) {
    return {a, {a.x + (b.x - a.x) / 3.0f, a.y + (b.y - a.y) / 3.0f},
            {a.x + 2.0f * (b.x - a.x) / 3.0f, a.y + 2.0f * (b.y - a.y) / 3.0f}, b};
}

// A profile, as (radius, height) points from the bottom of an object to the
// top, sampled from cubic Bezier segments with bezierCubic from core/Curves.
std::vector<P2> profileFrom(const std::vector<std::array<P2, 4>>& segments, int steps = 10) {
    std::vector<P2> out;
    for (const auto& s : segments)
        for (int i = out.empty() ? 0 : 1; i <= steps; ++i)
            out.push_back(curves::bezierCubic(s[0], s[1], s[2], s[3],
                                              static_cast<float>(i) / static_cast<float>(steps)));
    return out;
}

// How a lathe surface is mapped and shaped.
struct LatheStyle {
    float uRepeat = 3.0f;    // times the texture goes round
    bool  vAlong  = false;   // v runs 0 to 1 along the profile, for bands and engraving
    // Optional: scales the radius by angle a and position v (0 to 1) along the
    // profile. Flutes and panels come from this.
    std::function<float(float v, float a)> radial;
};

// A surface of revolution: the profile turned round the y axis, as on a lathe.
// Point (r, y) at angle a lands at (r cos a, y, r sin a). Without `radial`, its
// normal is the profile's tangent (dr, dy) turned a quarter, (dy, -dr), swung
// round with it; with it, the normal comes from the cross product of the
// surface's two directions, worked out from the neighbouring points.
// u runs against the angle so that, with the shader's B = N x T, v points up
// and engraving reads left to right from outside.
MeshData buildLathe(const std::vector<P2>& prof, int segments = 40, const LatheStyle& style = {}) {
    MeshData m;
    const std::size_t n = prof.size();
    if (n < 2) return m;
    std::vector<float> vAt(n, 0.0f);
    for (std::size_t i = 1; i < n; ++i)
        vAt[i] = vAt[i - 1] + std::hypot(prof[i].x - prof[i - 1].x, prof[i].y - prof[i - 1].y);
    const float total = vAt.back() > 0.0f ? vAt.back() : 1.0f;
    for (float& v : vAt) v /= total;

    auto point = [&](std::size_t i, float a) {
        const float r = prof[i].x * (style.radial ? style.radial(vAt[i], a) : 1.0f);
        return glm::vec3(r * std::cos(a), prof[i].y, r * std::sin(a));
    };
    const float da = 2.0f * kPi / static_cast<float>(segments);
    auto at = [&](std::size_t i, int k) {
        const float a = da * static_cast<float>(k);
        const float c = std::cos(a), s = std::sin(a);
        const P2 prev = prof[i == 0 ? 0 : i - 1];
        const P2 next = prof[std::min(i + 1, n - 1)];
        glm::vec2 side(next.y - prev.y, -(next.x - prev.x));
        side = glm::length(side) > 1e-6f ? glm::normalize(side) : glm::vec2(1.0f, 0.0f);
        glm::vec3 nrm = glm::normalize(glm::vec3(side.x * c, side.y, side.x * s));
        if (style.radial && prof[i].x > 1e-4f) {
            const glm::vec3 du = point(i, a + da * 0.5f) - point(i, a - da * 0.5f);
            const glm::vec3 dv = point(std::min(i + 1, n - 1), a) - point(i == 0 ? 0 : i - 1, a);
            glm::vec3 g = glm::cross(du, dv);
            if (glm::length(g) > 1e-9f) {
                g = glm::normalize(g);
                nrm = glm::dot(g, nrm) < 0.0f ? -g : g;
            }
        }
        Vertex v;
        v.position = point(i, a);
        v.normal   = nrm;
        v.uv       = {(1.0f - static_cast<float>(k) / static_cast<float>(segments)) * style.uRepeat,
                      style.vAlong ? vAt[i] : prof[i].y * 4.0f};
        v.tangent  = glm::vec3(s, 0.0f, -c);   // the way u grows
        return v;
    };
    for (std::size_t i = 0; i + 1 < n; ++i)
        for (int k = 0; k < segments; ++k)
            addSmoothQuad(m, {at(i, k), at(i + 1, k), at(i + 1, k + 1), at(i, k + 1)});
    return m;
}

// A model matrix from a position and three axes (they must be at right angles).
glm::mat4 frame(const glm::vec3& at, const glm::vec3& x, const glm::vec3& y, const glm::vec3& z) {
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(x, 0.0f);
    m[1] = glm::vec4(y, 0.0f);
    m[2] = glm::vec4(z, 0.0f);
    m[3] = glm::vec4(at, 1.0f);
    return m;
}

// A flat regular polygon with some thickness: a belt plate. It faces -x,
// centred on the origin, `sides` corners on a circle of radius r in the y-z plane.
MeshData buildPlate(int sides, float r, float thickness) {
    MeshData m;
    const float off = kPi / static_cast<float>(sides);   // a flat edge at the bottom
    auto corner = [&](int i, float x) {
        const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides) + off;
        return glm::vec3(x, std::sin(a) * r, std::cos(a) * r);
    };
    for (int face = 0; face < 2; ++face) {
        const float x = face == 0 ? -thickness : 0.0f;
        const glm::vec3 n(face == 0 ? -1.0f : 1.0f, 0.0f, 0.0f);
        const Vertex c{{x, 0.0f, 0.0f}, n, {0.5f, 0.5f}, {0, 0, 1}};
        for (int i = 0; i < sides; ++i) {
            const glm::vec3 a = corner(i, x), b = corner(i + 1, x);
            const Vertex va{a, n, {0.5f + a.z / (2 * r), 0.5f + a.y / (2 * r)}, {0, 0, 1}};
            const Vertex vb{b, n, {0.5f + b.z / (2 * r), 0.5f + b.y / (2 * r)}, {0, 0, 1}};
            const glm::vec3 f = glm::cross(vb.position - c.position, va.position - c.position);
            if (glm::dot(f, n) >= 0.0f) m.addTriangle(c, vb, va);
            else                        m.addTriangle(c, va, vb);
        }
    }
    for (int i = 0; i < sides; ++i)
        m.addQuad(corner(i, 0.0f), corner(i, -thickness), corner(i + 1, -thickness), corner(i + 1, 0.0f));
    return m;
}

// A flat strip from a to b, `width` wide, seen from both sides: a ribbon.
MeshData buildStrip(const glm::vec3& a, const glm::vec3& b, const glm::vec3& facing, float width) {
    MeshData m;
    const glm::vec3 across = glm::normalize(glm::cross(b - a, facing)) * (width * 0.5f);
    m.addQuad(a - across, b - across, b + across, a + across);
    m.addQuad(a + across, b + across, b - across, a - across);   // the back
    return m;
}

// Three great circles round a ball, as seams.
MeshData ballSeams(const glm::vec3& c, float r) {
    MeshData m;
    const MeshData ring = buildTorus(r * 1.012f, r * 0.035f, 48, 6);
    const glm::mat4 I(1.0f);
    m.append(ring, glm::translate(I, c));
    m.append(ring, glm::translate(I, c) * glm::rotate(I, kPi * 0.5f, glm::vec3(0, 1, 0)));
    m.append(ring, glm::translate(I, c) * glm::rotate(I, kPi * 0.5f, glm::vec3(0, 0, 1)));
    return m;
}

// A trophy: its parts, each with its own material. Built round the origin,
// standing on y = 0, its front facing -x; the room turns it on its pedestal.
struct Part {
    MeshData      mesh;
    gfx::Material material;
};

// ---- placing things on a wall ----------------------------------------------

// A wall in world space: a point on it at floor level, the direction into the
// room, and the direction to the visitor's right as they face it.
struct Wall {
    glm::vec3 mid{0.0f};
    glm::vec3 n{0.0f, 0.0f, 1.0f};
    glm::vec3 along{1.0f, 0.0f, 0.0f};
};

Wall wallThrough(Vec2 a, Vec2 b, Vec2 inward) {
    Wall w;
    w.mid   = glm::vec3(worldX((a.x + b.x) * 0.5f), 0.0f, worldZ((a.y + b.y) * 0.5f));
    w.n     = glm::vec3(inward.x, 0.0f, -inward.y);
    w.along = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), w.n));
    return w;
}

Wall wallOf(const HangPoint& hang) { return wallThrough(hang.span.a, hang.span.b, hang.inwardNormal); }

// The plan puts hang points a little way off the wall. This moves one onto the
// gold lining, so the frames sit against it.
HangPoint ontoLining(const HangPoint& hang, const Rect& r, float halfWall) {
    const Vec2 n = hang.inwardNormal;
    const Vec2 mid{(hang.span.a.x + hang.span.b.x) * 0.5f, (hang.span.a.y + hang.span.b.y) * 0.5f};
    float toWallLine;
    if      (n.x < -0.5f) toWallLine = r.x1 - mid.x;
    else if (n.x >  0.5f) toWallLine = mid.x - r.x0;
    else if (n.y < -0.5f) toWallLine = r.y1 - mid.y;
    else                  toWallLine = mid.y - r.y0;
    const float move = toWallLine - halfWall - kPanelOut;   // positive: towards the wall
    HangPoint out = hang;
    out.span.a.x -= n.x * move; out.span.a.y -= n.y * move;
    out.span.b.x -= n.x * move; out.span.b.y -= n.y * move;
    return out;
}

// A flat rectangle on a wall, facing into the room, `out` metres off it, with
// the texture repeated uRep x vRep times. Texture coordinates run left to
// right and bottom to top as seen from the room, so a Canvas reads correctly.
MeshData wallQuad(const Wall& w, float centreHeight, float width, float height,
                  float out, float uRep = 1.0f, float vRep = 1.0f) {
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 c = w.mid + up * centreHeight + w.n * out;
    const float hw = width * 0.5f, hh = height * 0.5f;
    MeshData m;
    m.addQuad(c - w.along * hw - up * hh, c + w.along * hw - up * hh,
              c + w.along * hw + up * hh, c - w.along * hw + up * hh, uRep, vRep);
    return m;
}

// A box lying against a wall: `length` along it, from y0 to y1, `depth` out.
MeshData wallBox(const Wall& w, float length, float y0, float y1, float depth) {
    const glm::vec3 c = w.mid + glm::vec3(0.0f, (y0 + y1) * 0.5f, 0.0f) + w.n * (depth * 0.5f);
    const glm::vec3 size = glm::abs(w.along) * length + glm::vec3(0.0f, y1 - y0, 0.0f)
                         + glm::abs(w.n) * depth;
    return build::buildBox(c, size);
}

// Width over height of an image file, without decoding it. 1.5 if unreadable.
float aspectOf(const std::string& path) {
    int w = 0, h = 0, comp = 0;
    if (!stbi_info(path.c_str(), &w, &h, &comp) || w <= 0 || h <= 0) {
        std::fprintf(stderr, "[Gallery Three] cannot read %s\n", path.c_str());
        return 1.5f;
    }
    return static_cast<float>(w) / static_cast<float>(h);
}

// The largest print of this aspect that fits the box.
void fitInside(float aspect, float maxW, float maxH, float& w, float& h) {
    w = maxW;
    h = w / aspect;
    if (h > maxH) { h = maxH; w = h * aspect; }
}

// ---- textures drawn by our own code ----------------------------------------

// Draws `text` centred on cx, as large as fits `maxWidth`, starting at `scale`.
void fitText(raster::Canvas& c, int cx, int y, const std::string& text,
             raster::Colour col, int scale, int maxWidth) {
    while (scale > 1 && font::textWidth(text, scale) > maxWidth) --scale;
    font::drawTextCentred(c, cx, y, text, col, scale);
}

// A wall label: the name large in gold, two lines underneath, a gold rule.
raster::Canvas drawLabel(const Champion& ch) {
    raster::Canvas c(512, 144);
    c.clear(kLabelBg);
    const int w = c.width();
    fitText(c, w / 2, 16, ch.name, kLabelGold, 5, w - 32);
    raster::lineThick(c, 96, 66, w - 96, 66, 2, kLabelGold);
    fitText(c, w / 2, 82, ch.line1, kLabelText, 2, w - 32);
    fitText(c, w / 2, 108, ch.line2, kLabelText, 2, w - 32);
    return c;
}

// Engraved letters: a light edge a step down and right, then the dark cut.
void engrave(raster::Canvas& c, int cx, int y, const std::string& text, int scale, int maxWidth) {
    while (scale > 1 && font::textWidth(text, scale) > maxWidth) --scale;
    const int off = std::max(1, scale / 3);
    font::drawTextCentred(c, cx + off, y + off, text, raster::Colour{246, 216, 140, 255}, scale);
    font::drawTextCentred(c, cx, y, text, raster::Colour{66, 40, 10, 255}, scale);
}

// The room's title, cut into a gold panel with a double border.
raster::Canvas drawTitle() {
    raster::Canvas c(1200, 200);
    c.clear(raster::Colour{190, 144, 58, 255});
    const raster::Colour edge{120, 84, 28, 255};
    const int W = c.width() - 1, H = c.height() - 1;
    for (int i : {6, 7, 14}) {
        raster::lineBresenham(c, i, i, W - i, i, edge);
        raster::lineBresenham(c, i, H - i, W - i, H - i, edge);
        raster::lineBresenham(c, i, i, i, H - i, edge);
        raster::lineBresenham(c, W - i, i, W - i, H - i, edge);
    }
    engrave(c, c.width() / 2, 34, "HALL OF CHAMPIONS", 11, c.width() - 80);
    engrave(c, c.width() / 2, 136, "EVERY ONE OF THEM SHAWN KURIA", 5, c.width() - 80);
    return c;
}

// The plate on the front of a pedestal: what the trophy is, and whose.
raster::Canvas drawPedestalLabel(const std::string& what, const std::string& who) {
    raster::Canvas c(384, 112);
    c.clear(kLabelBg);
    raster::lineBresenham(c, 4, 4, c.width() - 5, 4, kLabelGold);
    raster::lineBresenham(c, 4, c.height() - 5, c.width() - 5, c.height() - 5, kLabelGold);
    fitText(c, c.width() / 2, 22, what, kLabelGold, 4, c.width() - 24);
    fitText(c, c.width() / 2, 70, who, kLabelText, 2, c.width() - 24);
    return c;
}

// One repeat of the gold damask wallpaper. Anything that crosses the tile's
// edge is drawn again from the other side, so the pattern runs on seamlessly.
//   a diamond trellis of thick Bresenham lines,
//   a medallion in each diamond: filled midpoint ellipses with a ring round them,
//   studs where the trellis crosses, and four seeds round each medallion.
raster::Canvas drawDamask() {
    constexpr int S = 256, H = S / 2;
    raster::Canvas c(S, S);
    const raster::Colour ground{158, 114, 40, 255}, trellis{134, 94, 30, 255},
                         raised{184, 138, 56, 255}, shine{206, 162, 78, 255};
    c.clear(ground);
    const int lines[][4] = {{0, H, H, 0}, {H, 0, S, H}, {S, H, H, S}, {H, S, 0, H}};
    for (const auto& p : lines) raster::lineThick(c, p[0], p[1], p[2], p[3], 3, trellis);
    for (const auto& m : {std::array<int, 2>{H, H}, {0, 0}, {S, 0}, {0, S}, {S, S}}) {
        raster::ellipseFilled(c, m[0], m[1], 24, 38, raised);
        raster::ellipseMidpoint(c, m[0], m[1], 32, 48, raised);
        raster::ellipseFilled(c, m[0], m[1] - 10, 8, 12, shine);
        for (const auto& d : {std::array<int, 2>{0, -62}, {0, 62}, {-44, 0}, {44, 0}})
            raster::circleFilled(c, m[0] + d[0], m[1] + d[1], 5, raised);
    }
    for (const auto& s : {std::array<int, 2>{H, 0}, {H, S}, {0, H}, {S, H}})
        raster::circleFilled(c, s[0], s[1], 9, shine);
    return c;
}

// Black terrazzo: polished stone with chips of gold and brass in it. The chips
// come from a fixed seed, so the floor is the same every run, and each is drawn
// in all nine neighbouring tiles so the floor repeats without a seam.
raster::Canvas drawTerrazzo() {
    constexpr int S = 512;
    raster::Canvas c(S, S);
    c.clear(raster::Colour{16, 14, 13, 255});
    std::uint32_t seed = 158657u;
    auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    const raster::Colour chips[] = {{170, 128, 52, 255}, {112, 84, 40, 255},
                                    {214, 176, 96, 255}, {48, 42, 38, 255}};
    for (int i = 0; i < 900; ++i) {
        const int x = static_cast<int>(next() % S), y = static_cast<int>(next() % S);
        const int r = static_cast<int>(next() % 2);   // 0 is a single pixel
        const raster::Colour col = chips[next() % 4];
        for (int ox : {-S, 0, S})
            for (int oy : {-S, 0, S}) raster::circleFilled(c, x + ox, y + oy, r, col);
    }
    return c;
}

// The pop art: our portrait printed four ways. Empty canvas if the photo is missing.
raster::Canvas drawPopArt(const std::string& path) {
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(0);   // our canvases start at the top row
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &channels, 0);
    if (!px) {
        std::fprintf(stderr, "[Gallery Three] cannot read %s\n", path.c_str());
        return raster::Canvas(0, 0);
    }
    popart::Grey face = popart::luminance(px, w, h, channels);
    stbi_image_free(px);
    popart::stretch(face, 0.02f);
    return popart::warholGrid(face, "SHAWN KURIA - CHAMPION OF EVERYTHING", 2);
}


// ---- materials ----------------------------------------------------------------

// Gold leaf: the gilt texture tinted, with a highlight as sharp as asked.
gfx::Material metal(const RoomContext& ctx, glm::vec3 tint, float specular, float shininess) {
    gfx::Material m;
    m.albedo    = ctx.texture("materials/gilt.jpg");
    m.tint      = tint;
    m.shading   = cfg::Shading::BlinnPhong;
    m.specular  = specular;
    m.shininess = shininess;
    return m;
}

// Something that gives off its own light: flames, the picture lights.
gfx::Material glow(glm::vec3 tint, float strength) {
    gfx::Material m;
    m.tint     = tint;
    m.shading  = cfg::Shading::Emissive;
    m.emissive = strength;
    m.specular = 0.0f;
    return m;
}


// ---- the six trophies ---------------------------------------------------------
//
// Our own designs, one for each sport, rather than copies of the real ones.
// Each is built round the origin on y = 0, front to -x, from lathe profiles,
// tori, leaves, plates, strips and swept tubes, and finished with engraving
// and black enamel drawn by our own code.

struct Trophy {
    const char* what;      // on the pedestal plate
    const char* who;
    const char* engraved;  // cut into the trophy's collar
};
const Trophy kTrophies[6] = {
    {"GOLDEN LAUREL",       "ELIUD KIPKURIA",   "ELIUD KIPKURIA - 1:59:40.2 - VIENNA 2019 - "},
    {"CHAMPION'S CUP",      "LEWIS HAMILSHAWN", "LEWIS HAMILSHAWN - WORLD CHAMPION - "},
    {"GOLDEN BALL",         "LESHAWN MESSI",    "LESHAWN MESSI - EIGHT TIMES - "},
    {"100 M GOLD",          "USHAWN BOLT",      "USHAWN BOLT - 9.58 - BERLIN 2009 - "},
    {"TWO BELTS",           "CONNOR MCKURIA",   "CONNOR MCKURIA - "},
    {"CHAMPIONSHIP TROPHY", "LESHAWN JAMES",    "LESHAWN JAMES - MIAMI 2013 - "},
};

// ---- engraving: height maps drawn by our code, turned into normal maps --------

// A normal map from a height map (white is high), with a Sobel filter: the
// slope across and down gives how far the normal tips. The canvas starts at
// the top row while v grows upwards, hence the sign on the second slope.
raster::Canvas normalsFromHeight(const raster::Canvas& h, float strength) {
    raster::Canvas out(h.width(), h.height());
    auto at = [&](int x, int y) {
        x = (x % h.width() + h.width()) % h.width();   // wrap: the textures repeat
        y = std::clamp(y, 0, h.height() - 1);
        return static_cast<float>(h.get(x, y).r) / 255.0f;
    };
    for (int y = 0; y < h.height(); ++y)
        for (int x = 0; x < h.width(); ++x) {
            const float gx = (at(x - 1, y - 1) + 2.0f * at(x - 1, y) + at(x - 1, y + 1)) -
                             (at(x + 1, y - 1) + 2.0f * at(x + 1, y) + at(x + 1, y + 1));
            const float gy = (at(x - 1, y + 1) + 2.0f * at(x, y + 1) + at(x + 1, y + 1)) -
                             (at(x - 1, y - 1) + 2.0f * at(x, y - 1) + at(x + 1, y - 1));
            const glm::vec3 n = glm::normalize(glm::vec3(gx * strength, gy * strength, 1.0f));
            out.set(x, y, raster::Colour{static_cast<std::uint8_t>((n.x * 0.5f + 0.5f) * 255.0f),
                                         static_cast<std::uint8_t>((n.y * 0.5f + 0.5f) * 255.0f),
                                         static_cast<std::uint8_t>((n.z * 0.5f + 0.5f) * 255.0f), 255});
        }
    return out;
}

// A Canvas as a 24-bit .bmp file in memory, so the texture cache can upload it
// as data (linear, not sRGB), which a normal map needs.
std::vector<unsigned char> bmpBytes(const raster::Canvas& c) {
    const int w = c.width(), h = c.height();
    const int row = (w * 3 + 3) & ~3;
    const std::uint32_t size = 54u + static_cast<std::uint32_t>(row * h);
    std::vector<unsigned char> b(size, 0);
    auto put32 = [&](std::size_t at, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) b[at + static_cast<std::size_t>(i)] = static_cast<unsigned char>(v >> (8 * i));
    };
    b[0] = 'B'; b[1] = 'M';
    put32(2, size); put32(10, 54); put32(14, 40);
    put32(18, static_cast<std::uint32_t>(w)); put32(22, static_cast<std::uint32_t>(h));
    b[26] = 1; b[28] = 24;
    put32(34, static_cast<std::uint32_t>(row * h));
    for (int y = 0; y < h; ++y) {   // .bmp rows run bottom to top
        unsigned char* dst = &b[54 + static_cast<std::size_t>(row) * static_cast<std::size_t>(h - 1 - y)];
        for (int x = 0; x < w; ++x) {
            const raster::Colour p = c.get(x, y);
            dst[x * 3 + 0] = p.b; dst[x * 3 + 1] = p.g; dst[x * 3 + 2] = p.r;
        }
    }
    return b;
}

const gfx::Texture* normalMap(const RoomContext& ctx, const std::string& key,
                              const raster::Canvas& height, float strength) {
    if (!ctx.textures) return nullptr;
    return ctx.textures->fromMemory("hall:normal:" + key, bmpBytes(normalsFromHeight(height, strength)),
                                    gfx::Texture::Kind::Data);
}

// A material with a surface drawn by our code: colour from `look`, bumps from `height`.
gfx::Material engraved(const RoomContext& ctx, const std::string& key, const raster::Canvas& look,
                       const raster::Canvas& height, glm::vec3 tint, float specular, float shininess,
                       float strength = 2.0f) {
    gfx::Material m;
    m.albedo    = ctx.canvas("hall:look:" + key, look);
    m.normal    = normalMap(ctx, key, height, strength);
    m.tint      = tint;
    m.shading   = cfg::Shading::NormalMapped;
    m.specular  = specular;
    m.shininess = shininess;
    return m;
}

// Text repeated to fill a band that goes all the way round, centred on cy.
void textRound(raster::Canvas& c, const std::string& text, int cy, int scale, raster::Colour col) {
    const int w = font::textWidth(text, scale);
    if (w <= 0) return;
    const int copies = std::max(1, c.width() / w);
    const float step = static_cast<float>(c.width()) / static_cast<float>(copies);
    for (int i = 0; i < copies; ++i)
        font::drawText(c, static_cast<int>(step * static_cast<float>(i) + (step - static_cast<float>(w)) * 0.5f),
                       cy - font::kGlyphH * scale / 2, text, col, scale);
}

// An enamel collar: black, with the text in raised gold letters and a gold
// line along each edge. Look and height are drawn from the same shapes.
gfx::Material enamelBand(const RoomContext& ctx, const std::string& key, const std::string& text) {
    raster::Canvas look(1152, 128), height(1152, 128);
    look.clear({24, 21, 19, 255});
    height.clear({0, 0, 0, 255});
    for (raster::Canvas* c : {&look, &height}) {
        const raster::Colour ink = c == &look ? kGiltBase : raster::Colour{255, 255, 255, 255};
        raster::lineThick(*c, 0, 10, c->width() - 1, 10, 6, ink);
        raster::lineThick(*c, 0, 117, c->width() - 1, 117, 6, ink);
        textRound(*c, text, 64, 5, ink);
    }
    return engraved(ctx, "band:" + key, look, height, kGold, 0.8f, 110.0f, 3.0f);
}

// ---- small parts ----------------------------------------------------------------

// A ring of beads, as round a rim or the edge of a foot.
MeshData beads(float radius, float y, int count, float bead) {
    MeshData m;
    for (int i = 0; i < count; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(count);
        m.append(buildSphere({radius * std::cos(a), y, radius * std::sin(a)}, bead, 6), glm::mat4(1.0f));
    }
    return m;
}

// A laurel leaf, lying along +x with its face to +z: pointed at both ends,
// its edge cut into small teeth, and folded along the middle so the midrib
// stands up as a ridge that catches the light. Both faces are built.
MeshData buildLeaf(float length, float width, int teeth = 6) {
    MeshData m;
    const int steps = 18;
    auto halfWidth = [&](float t) {
        const float body = width * 0.5f * std::pow(std::sin(kPi * t), 0.8f);
        const float tooth = std::fabs(std::fmod(t * static_cast<float>(teeth), 1.0f) - 0.5f) * 2.0f;
        return body * (0.88f + 0.12f * tooth);
    };
    auto rib = [&](float t) { return glm::vec3(t * length, 0.0f, width * 0.12f * std::sin(kPi * t)); };
    for (int i = 0; i < steps; ++i) {
        const float t0 = static_cast<float>(i) / steps, t1 = static_cast<float>(i + 1) / steps;
        for (float side : {-1.0f, 1.0f}) {
            const glm::vec3 a0 = rib(t0), a1 = rib(t1);
            const glm::vec3 e0(t0 * length, side * halfWidth(t0), 0.0f), e1(t1 * length, side * halfWidth(t1), 0.0f);
            if (side > 0.0f) { m.addQuad(a0, e0, e1, a1); m.addQuad(a1, e1, e0, a0); }
            else             { m.addQuad(a0, a1, e1, e0); m.addQuad(e0, e1, a1, a0); }
        }
    }
    return m;
}

// A turned base shared by four trophies: a bevelled foot with beads round its
// edge, an enamel collar with the champion's engraving, and a shoulder that
// narrows to the stem. Returns the height where the stem starts.
float turnedBase(const RoomContext& ctx, const std::string& key, const std::string& text,
                 std::vector<Part>& parts, float rFoot, float rCollar, float stemR) {
    const float collar0 = 0.035f, collar1 = 0.095f, top = 0.14f;
    MeshData gold;
    gold.append(buildLathe(profileFrom({straight({0.0f, 0.0f}, {rFoot, 0.0f}),
                                        straight({rFoot, 0.0f}, {rFoot, 0.012f}),
                                        {{{rFoot, 0.012f}, {rFoot - 0.004f, 0.03f}, {rCollar + 0.01f, 0.03f},
                                          {rCollar, collar0}}}})), glm::mat4(1.0f));
    gold.append(buildLathe(profileFrom({straight({rCollar, collar1}, {rCollar + 0.008f, collar1}),
                                        {{{rCollar + 0.008f, collar1}, {rCollar, top - 0.02f},
                                          {stemR * 1.6f, top - 0.01f}, {stemR, top}}}})), glm::mat4(1.0f));
    gold.append(beads(rFoot - 0.004f, 0.016f, 48, 0.0045f), glm::mat4(1.0f));
    gold.append(beads(rCollar + 0.004f, collar1 + 0.004f, 40, 0.0035f), glm::mat4(1.0f));
    parts.push_back({std::move(gold), metal(ctx, kGold, 0.85f, 110.0f)});

    LatheStyle band;
    band.uRepeat = 1.0f;
    band.vAlong  = true;
    parts.push_back({buildLathe(profileFrom({straight({rCollar, collar0}, {rCollar, collar1})}, 2), 64, band),
                     enamelBand(ctx, key, text)});
    return top;
}

gfx::Material jewel(glm::vec3 tint) {   // a cut stone: dark, and the sharpest glint in the room
    gfx::Material m;
    m.tint      = tint;
    m.specular  = 1.0f;
    m.shininess = 240.0f;
    m.emissive  = 0.05f;
    return m;
}

gfx::Material cloth(glm::vec3 tint) {
    gfx::Material m;
    m.tint     = tint;
    m.shading  = cfg::Shading::Lambert;   // cloth does not shine
    m.specular = 0.0f;
    return m;
}

// ---- 1. the laurel wreath ------------------------------------------------------

std::vector<Part> laurelWreath(const RoomContext& ctx) {
    std::vector<Part> parts;
    const float R = 0.15f;
    const float base = turnedBase(ctx, "laurel", kTrophies[0].engraved, parts, 0.10f, 0.075f, 0.014f);
    const glm::vec3 c(0.0f, base + 0.30f + R, 0.0f);   // the wreath stands in the y-z plane
    MeshData gold;
    // A slender stem up to the bottom of the wreath, with a knop half way.
    gold.append(buildLathe(profileFrom({straight({0.014f, base}, {0.010f, base + 0.12f}),
                                        {{{0.010f, base + 0.12f}, {0.03f, base + 0.13f}, {0.03f, base + 0.17f},
                                          {0.010f, base + 0.18f}}},
                                        straight({0.010f, base + 0.18f}, {0.008f, c.y - R}),
                                        straight({0.008f, c.y - R}, {0.0f, c.y - R})}), 20), glm::mat4(1.0f));
    // Two branches, rising from the bottom and nearly meeting at the top.
    std::vector<glm::vec3> branch[2];
    for (int i = 0; i <= 40; ++i) {
        const float phi = glm::radians(178.0f - 160.0f * static_cast<float>(i) / 40.0f);   // from the top
        for (int s = 0; s < 2; ++s) {
            const float side = s == 0 ? 1.0f : -1.0f;
            branch[s].push_back(c + glm::vec3(0.0f, std::cos(phi), side * std::sin(phi)) * R);
        }
    }
    for (const auto& b : branch) gold.append(buildTube(b, 0.005f, 6), glm::mat4(1.0f));
    // Leaves in pairs along each branch, leaning out and in, smaller near the tips.
    MeshData berries;
    const glm::vec3 face(-1.0f, 0.0f, 0.0f);
    for (int s = 0; s < 2; ++s) {
        const float side = s == 0 ? 1.0f : -1.0f;
        for (int i = 0; i < 13; ++i) {
            const float f = static_cast<float>(i) / 12.0f;
            const float phi = glm::radians(170.0f - 150.0f * f);
            const glm::vec3 out(0.0f, std::cos(phi), side * std::sin(phi));
            const glm::vec3 along = glm::normalize(glm::vec3(0.0f, -std::sin(phi), side * std::cos(phi)));
            const float size = 1.0f - 0.35f * f;
            for (float lean : {-0.9f, 0.9f}) {
                const glm::vec3 axis = glm::normalize(along + out * lean);
                const glm::vec3 normal = glm::normalize(face + 0.25f * out * lean);
                const glm::vec3 y = glm::normalize(glm::cross(normal, axis));
                gold.append(buildLeaf(0.055f * size, 0.020f * size), frame(c + out * R, axis, y, glm::cross(axis, y)));
            }
            if (i % 3 == 1) berries.append(buildSphere(c + out * (R - 0.012f) + face * 0.008f, 0.0065f, 8), glm::mat4(1.0f));
        }
    }
    parts.push_back({std::move(gold), metal(ctx, kGold, 0.8f, 80.0f)});
    parts.push_back({std::move(berries), jewel({0.45f, 0.03f, 0.06f})});   // garnets
    // A ribbon in Kenya's colours tied at the bottom.
    for (int k = 0; k < 3; ++k) {
        const glm::vec3 top = c + glm::vec3(-0.012f, -R, 0.012f * static_cast<float>(k - 1));
        parts.push_back({buildStrip(top, top + glm::vec3(0.0f, -0.12f, 0.035f * static_cast<float>(k - 1)),
                                    glm::vec3(-1, 0, 0), 0.012f),
                         cloth(kKenya[k])});
    }
    return parts;
}

// ---- 2. the champion's cup -----------------------------------------------------

std::vector<Part> championsCup(const RoomContext& ctx) {
    std::vector<Part> parts;
    const glm::mat4 I(1.0f);
    const float b = turnedBase(ctx, "cup", kTrophies[1].engraved, parts, 0.12f, 0.085f, 0.025f);
    MeshData gold;
    // The stem with a knop, then a short neck flaring into the bowl.
    gold.append(buildLathe(profileFrom({
        straight({0.025f, b}, {0.022f, b + 0.03f}),
        {{{0.022f, b + 0.03f}, {0.055f, b + 0.035f}, {0.055f, b + 0.065f}, {0.022f, b + 0.07f}}},
        {{{0.022f, b + 0.07f}, {0.03f, b + 0.09f}, {0.05f, b + 0.10f}, {0.08f, b + 0.105f}}}})), I);
    // The bowl in three bands. The lower one is fluted: the lathe's radius
    // swells and dips 18 times round, fading out at both ends of the band.
    LatheStyle flutes;
    flutes.radial = [](float v, float a) { return 1.0f + 0.05f * std::cos(18.0f * a) * std::sin(kPi * v); };
    const float y0 = b + 0.105f;
    gold.append(buildLathe(profileFrom({{{{0.08f, y0}, {0.12f, y0 + 0.01f}, {0.14f, y0 + 0.06f}, {0.145f, y0 + 0.11f}}}}, 16),
                           72, flutes), I);
    gold.append(buildLathe(profileFrom({{{{0.155f, y0 + 0.165f}, {0.156f, y0 + 0.20f}, {0.152f, y0 + 0.23f}, {0.15f, y0 + 0.245f}}},
                                        {{{0.15f, y0 + 0.245f}, {0.156f, y0 + 0.25f}, {0.15f, y0 + 0.258f}, {0.142f, y0 + 0.252f}}},
                                        {{{0.142f, y0 + 0.252f}, {0.14f, y0 + 0.16f}, {0.08f, y0 + 0.08f}, {0.0f, y0 + 0.08f}}}}), 64), I);
    gold.append(beads(0.149f, y0 + 0.11f, 60, 0.004f), I);   // a beaded line under the band
    gold.append(beads(0.153f, y0 + 0.252f, 72, 0.0045f), I); // and round the lip
    // Two handles, each a tube swept along a cubic Bezier, thicker in the
    // middle, with a ball at each end.
    for (float side : {-1.0f, 1.0f}) {
        std::vector<glm::vec3> path;
        std::vector<float> radii;
        for (int i = 0; i <= 20; ++i) {
            const float t = static_cast<float>(i) / 20.0f;
            const P2 q = curves::bezierCubic({0.15f, y0 + 0.21f}, {0.29f, y0 + 0.25f}, {0.30f, y0 + 0.02f},
                                             {0.10f, y0 + 0.02f}, t);
            path.push_back(glm::vec3(0.0f, q.y, side * q.x));
            radii.push_back(0.009f + 0.007f * std::sin(kPi * t));
        }
        gold.append(buildTube(path, 0.01f, 10, radii), I);
        gold.append(buildSphere(glm::vec3(0.0f, y0 + 0.215f, side * 0.162f), 0.013f, 10), I);
    }
    parts.push_back({std::move(gold), metal(ctx, kGold, 0.9f, 130.0f)});
    // The enamel band round the middle of the bowl, with the engraving.
    LatheStyle band;
    band.uRepeat = 1.0f;
    band.vAlong  = true;
    parts.push_back({buildLathe(profileFrom({{{{0.145f, y0 + 0.11f}, {0.150f, y0 + 0.13f}, {0.154f, y0 + 0.15f},
                                               {0.155f, y0 + 0.165f}}}}, 6), 72, band),
                     enamelBand(ctx, "cup-bowl", "WORLD CHAMPION - SEVEN TITLES - ")});
    return parts;
}

// ---- 3. the golden ball --------------------------------------------------------

// Which panel of a football a direction falls in: the 12 corners of an
// icosahedron are the pentagons, its 20 face centres the hexagons. A point
// belongs to the nearest centre; it is on a seam when two are nearly as near.
struct Panels {
    std::vector<glm::vec3> centres;   // pentagons first
    Panels() {
        const float g = (1.0f + std::sqrt(5.0f)) * 0.5f;
        const glm::vec3 v[12] = {{-1, g, 0}, {1, g, 0}, {-1, -g, 0}, {1, -g, 0}, {0, -1, g}, {0, 1, g},
                                 {0, -1, -g}, {0, 1, -g}, {g, 0, -1}, {g, 0, 1}, {-g, 0, -1}, {-g, 0, 1}};
        for (const glm::vec3& p : v) centres.push_back(glm::normalize(p));
        for (int a = 0; a < 12; ++a)   // a face is three corners all a single edge apart
            for (int b = a + 1; b < 12; ++b)
                for (int c = b + 1; c < 12; ++c) {
                    const float e = 2.0f / std::sqrt(g * g + 1.0f) + 0.01f;
                    if (glm::length(centres[a] - centres[b]) < e && glm::length(centres[b] - centres[c]) < e &&
                        glm::length(centres[a] - centres[c]) < e)
                        centres.push_back(glm::normalize(centres[a] + centres[b] + centres[c]));
                }
    }
    // The nearest centre, and how much nearer it is than the next one.
    void nearest(const glm::vec3& d, int& which, float& margin) const {
        float best = -2.0f, second = -2.0f;
        which = 0;
        for (std::size_t i = 0; i < centres.size(); ++i) {
            const float s = glm::dot(d, centres[i]);
            if (s > best) { second = best; best = s; which = static_cast<int>(i); }
            else if (s > second) second = s;
        }
        margin = best - second;
    }
};

std::vector<Part> goldenBall(const RoomContext& ctx) {
    std::vector<Part> parts;
    const glm::mat4 I(1.0f);
    const float b = turnedBase(ctx, "ball", kTrophies[2].engraved, parts, 0.10f, 0.075f, 0.022f);
    MeshData gold;
    // A fluted stem rising into a cup that holds the ball.
    LatheStyle fluted;
    fluted.radial = [](float v, float a) { return 1.0f + 0.18f * std::cos(8.0f * a) * std::sin(kPi * v); };
    gold.append(buildLathe(profileFrom({{{{0.022f, b}, {0.018f, b + 0.05f}, {0.02f, b + 0.08f}, {0.03f, b + 0.10f}}}}, 12),
                           48, fluted), I);
    gold.append(buildLathe(profileFrom({{{{0.03f, b + 0.10f}, {0.07f, b + 0.10f}, {0.08f, b + 0.12f}, {0.075f, b + 0.135f}}},
                                        straight({0.075f, b + 0.135f}, {0.0f, b + 0.12f})})), I);
    gold.append(beads(0.077f, b + 0.132f, 40, 0.004f), I);
    parts.push_back({std::move(gold), metal(ctx, kGold, 0.85f, 110.0f)});

    // The ball: a sphere whose surface sinks into a groove along every seam and
    // swells a little in the middle of each panel. Pentagons are black enamel.
    const float r = 0.11f;
    const glm::vec3 c(0.0f, b + 0.12f + r * 0.95f, 0.0f);
    static const Panels panels;
    MeshData goldPanels, blackPanels;
    const int lonN = 200, latN = 100;
    auto at = [&](int i, int j) {
        const float lon = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(lonN);
        const float lat = kPi * static_cast<float>(j) / static_cast<float>(latN) - kPi * 0.5f;
        const glm::vec3 d(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
        int which = 0; float margin = 0.0f;
        panels.nearest(d, which, margin);
        const float groove = 1.0f - std::clamp(margin / 0.035f, 0.0f, 1.0f);
        const float rr = r * (1.0f - 0.03f * groove * groove + 0.006f * std::clamp(margin / 0.2f, 0.0f, 1.0f));
        Vertex v;
        v.position = c + d * rr;
        v.normal   = d;   // refined below from the neighbours
        v.uv       = {static_cast<float>(i) / static_cast<float>(lonN), static_cast<float>(j) / static_cast<float>(latN)};
        v.tangent  = glm::vec3(-std::sin(lon), 0.0f, std::cos(lon));
        return std::make_pair(v, which);
    };
    // Normals from the bumpy surface: the cross product of its two directions.
    std::vector<std::vector<std::pair<Vertex, int>>> grid(lonN + 1, std::vector<std::pair<Vertex, int>>(latN + 1));
    for (int i = 0; i <= lonN; ++i)
        for (int j = 0; j <= latN; ++j) grid[i][j] = at(i, j);
    for (int i = 0; i <= lonN; ++i)
        for (int j = 1; j < latN; ++j) {
            const glm::vec3 du = grid[(i + 1) % lonN][j].first.position - grid[(i + lonN - 1) % lonN][j].first.position;
            const glm::vec3 dv = grid[i][j + 1].first.position - grid[i][j - 1].first.position;
            glm::vec3 n = glm::cross(du, dv);
            if (glm::length(n) > 1e-9f) {
                n = glm::normalize(n);
                grid[i][j].first.normal = glm::dot(n, grid[i][j].first.normal) < 0.0f ? -n : n;
            }
        }
    for (int i = 0; i < lonN; ++i)
        for (int j = 0; j < latN; ++j) {
            const auto& q = grid[i][j];
            const bool pentagon = q.second < 12;
            addSmoothQuad(pentagon ? blackPanels : goldPanels,
                          {q.first, grid[i + 1][j].first, grid[i + 1][j + 1].first, grid[i][j + 1].first});
        }
    parts.push_back({std::move(goldPanels), metal(ctx, kGold, 0.9f, 140.0f)});
    gfx::Material enamel = metal(ctx, glm::vec3(0.06f, 0.05f, 0.05f), 0.9f, 160.0f);   // glossy black
    parts.push_back({std::move(blackPanels), enamel});
    return parts;
}

// ---- 4. the sprint medal -------------------------------------------------------

std::vector<Part> sprintMedal(const RoomContext& ctx) {
    std::vector<Part> parts;
    const glm::mat4 I(1.0f);
    const float b = turnedBase(ctx, "sprint", kTrophies[3].engraved, parts, 0.10f, 0.075f, 0.012f);
    // A stand like a little gallows: post, and a bar with ball ends across the top.
    MeshData stand;
    const float top = b + 0.44f;
    stand.append(buildLathe(profileFrom({straight({0.012f, b}, {0.009f, top}), straight({0.009f, top}, {0.0f, top})}), 16), I);
    stand.append(build::buildBox({0.0f, top + 0.006f, 0.0f}, {0.018f, 0.018f, 0.24f}), I);
    for (float side : {-1.0f, 1.0f}) stand.append(buildSphere({0.0f, top + 0.006f, side * 0.125f}, 0.014f, 10), I);
    parts.push_back({std::move(stand), metal(ctx, kAntiqueGold, 0.6f, 48.0f)});

    // The medal. Its face, drawn by our own code, is both the colour and the
    // relief: a rim, a ring of laurel leaves filled with our scanline fill,
    // the time, and the initials.
    raster::Canvas look(256, 256), height(256, 256);
    look.clear(kGiltBase);
    height.clear({90, 90, 90, 255});
    for (raster::Canvas* cv : {&look, &height}) {
        const bool h = cv == &height;
        const raster::Colour line = h ? raster::Colour{255, 255, 255, 255} : kGiltDeep;
        for (int rr = 118; rr <= 127; ++rr) raster::circleMidpoint(*cv, 128, 128, rr, line);
        raster::circleMidpoint(*cv, 128, 128, 104, line);
        for (int i = 0; i < 28; ++i) {   // leaves round the inside of the rim
            const float a = 2.0f * kPi * static_cast<float>(i) / 28.0f;
            const float cx = 128.0f + 92.0f * std::cos(a), cy = 128.0f + 92.0f * std::sin(a);
            const float tx = -std::sin(a), ty = std::cos(a);   // along the ring
            const float lx = std::cos(a), ly = std::sin(a);    // across it
            std::vector<raster::Point> leaf = {
                {static_cast<int>(cx + tx * 9.0f), static_cast<int>(cy + ty * 9.0f)},
                {static_cast<int>(cx + lx * 4.0f), static_cast<int>(cy + ly * 4.0f)},
                {static_cast<int>(cx - tx * 9.0f), static_cast<int>(cy - ty * 9.0f)},
                {static_cast<int>(cx - lx * 4.0f), static_cast<int>(cy - ly * 4.0f)}};
            raster::fillPolygonScanline(*cv, leaf, line);
        }
        fitText(*cv, 128, 84, "9.58", line, 9, 170);
        font::drawTextCentred(*cv, 128, 166, "S.K.", line, 4);
    }
    gfx::Material face = engraved(ctx, "sprint-medal", look, height, kGold, 0.85f, 120.0f, 3.0f);
    const float r = 0.085f;
    const glm::vec3 m(-0.02f, b + 0.20f, 0.0f);
    MeshData disc;
    disc.append(buildMedal(r, 0.012f), glm::translate(I, m));
    parts.push_back({std::move(disc), face});
    MeshData ring;   // a beaded edge
    for (int i = 0; i < 40; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / 40.0f;
        ring.append(buildSphere(m + glm::vec3(0.0f, std::sin(a), std::cos(a)) * r, 0.0055f, 6), I);
    }
    ring.append(buildTorus(0.012f, 0.0035f, 16, 6), glm::translate(I, m + glm::vec3(0.0f, r + 0.01f, 0.0f)));   // the loop
    parts.push_back({std::move(ring), metal(ctx, kMedalGold, 0.85f, 120.0f)});
    // Ribbon in Jamaica's colours: a V from the bar's ends down to the loop.
    for (float side : {-1.0f, 1.0f})
        for (int k = 0; k < 3; ++k) {
            const float o = 0.009f * static_cast<float>(k - 1);
            const glm::vec3 a(-0.02f, top, side * (0.09f + o));
            const glm::vec3 z(-0.02f, m.y + r + 0.016f, side * (0.012f + o));
            parts.push_back({buildStrip(a, z, glm::vec3(-1, 0, 0), 0.009f), cloth(kJamaica[k])});
        }
    return parts;
}

// ---- 5. the two belts ----------------------------------------------------------

std::vector<Part> twoBelts(const RoomContext& ctx) {
    std::vector<Part> parts;
    const glm::mat4 I(1.0f);
    const float R = 0.13f, H = 0.30f;
    // A velvet drum to wrap the belts round, with a gold beaded rim at each end.
    parts.push_back({buildLathe(profileFrom({straight({0.0f, 0.0f}, {R, 0.0f}), straight({R, 0.0f}, {R, H}),
                                             straight({R, H}, {0.0f, H})}), 48),
                     cloth({0.16f, 0.03f, 0.05f})});
    MeshData trim;
    trim.append(beads(R + 0.002f, 0.006f, 56, 0.006f), I);
    trim.append(beads(R + 0.002f, H - 0.006f, 56, 0.006f), I);

    // Leather, drawn by our own code: a grain of small random dimples, and a
    // row of stitches along each edge. The same drawing gives colour and relief.
    raster::Canvas look(512, 64), height(512, 64);
    look.clear({44, 42, 46, 255});
    height.clear({128, 128, 128, 255});
    std::uint32_t seed = 4206u;
    auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    for (int i = 0; i < 2600; ++i) {
        const int x = static_cast<int>(next() % 512), y = static_cast<int>(next() % 64);
        const std::uint8_t d = static_cast<std::uint8_t>(96 + next() % 40);
        height.set(x, y, {d, d, d, 255});
    }
    for (int x = 2; x < 512; x += 12)
        for (int y : {9, 54}) {
            raster::lineThick(look, x, y, x + 7, y, 2, {170, 160, 140, 255});
            raster::lineThick(height, x, y, x + 7, y, 2, {255, 255, 255, 255});
        }
    gfx::Material leather = engraved(ctx, "belt-leather", look, height, glm::vec3(1.0f), 0.3f, 28.0f, 1.5f);

    // A plate face: engraved CHAMPION, the initials, a border. Gold with black enamel letters.
    raster::Canvas plook(256, 256), pheight(256, 256);
    plook.clear(kGiltBase);
    pheight.clear({200, 200, 200, 255});
    for (raster::Canvas* cv : {&plook, &pheight}) {
        const bool h = cv == &pheight;
        const raster::Colour ink = h ? raster::Colour{40, 40, 40, 255} : raster::Colour{30, 26, 24, 255};
        for (int rr = 104; rr <= 108; ++rr) raster::circleMidpoint(*cv, 128, 128, rr, ink);
        fitText(*cv, 128, 92, "CHAMPION", ink, 4, 190);
        fitText(*cv, 128, 138, "S.K.", ink, 6, 160);
    }
    gfx::Material plateFace = engraved(ctx, "belt-plate", plook, pheight, kGold, 0.85f, 120.0f, 2.5f);

    MeshData strap, plates, faces, studs, gems;
    for (float y : {0.085f, 0.215f}) {
        // The strap, all the way round, standing just off the drum.
        const float half = 0.042f, rb = R + 0.006f;
        for (int i = 0; i < 64; ++i) {
            const float a0 = 2.0f * kPi * static_cast<float>(i) / 64.0f;
            const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / 64.0f;
            auto v = [&](float a, float dy, float u, float vv) {
                const glm::vec3 d(-std::cos(a), 0.0f, std::sin(a));
                return Vertex{d * rb + glm::vec3(0.0f, y + dy, 0.0f), d, {u, vv},
                              glm::vec3(std::sin(a), 0.0f, std::cos(a))};
            };
            const float u0 = static_cast<float>(i) / 64.0f * 6.0f, u1 = static_cast<float>(i + 1) / 64.0f * 6.0f;
            addSmoothQuad(strap, {v(a0, -half, u0, 0.0f), v(a1, -half, u1, 0.0f), v(a1, half, u1, 1.0f), v(a0, half, u0, 1.0f)});
        }
        // The main plate at the front: a raised octagon behind an engraved face,
        // a twisted rope rim, studs at its corners and a garnet at the top.
        const glm::vec3 front(-rb, y, 0.0f);
        plates.append(buildPlate(8, 0.085f, 0.012f), glm::translate(I, front));
        faces.append(buildPlate(8, 0.068f, 0.004f), glm::translate(I, front + glm::vec3(-0.012f, 0.0f, 0.0f)));
        plates.append(buildTorus(0.072f, 0.004f, 48, 6), glm::translate(I, front + glm::vec3(-0.014f, 0.0f, 0.0f)));
        for (int k = 0; k < 8; ++k) {
            const float a = 2.0f * kPi * (static_cast<float>(k) + 0.5f) / 8.0f;
            studs.append(buildSphere(front + glm::vec3(-0.014f, std::sin(a) * 0.079f, std::cos(a) * 0.079f), 0.006f, 8), I);
        }
        gems.append(buildSphere(front + glm::vec3(-0.018f, 0.062f, 0.0f), 0.009f, 10), I);
        // Side plates, round the drum either side, each with four studs.
        for (float side : {-1.0f, 1.0f}) {
            const glm::mat4 turn = glm::rotate(I, glm::radians(50.0f) * side, glm::vec3(0, 1, 0));
            plates.append(buildPlate(6, 0.04f, 0.01f), turn * glm::translate(I, {-rb, y, 0.0f}));
            for (int k = 0; k < 4; ++k) {
                const float a = kPi * 0.5f * static_cast<float>(k) + kPi * 0.25f;
                studs.append(buildSphere({-rb - 0.012f, y + std::sin(a) * 0.022f, std::cos(a) * 0.022f}, 0.004f, 6), turn);
            }
        }
        // A buckle at the back: a rectangular frame and its prong.
        const glm::mat4 back = glm::rotate(I, kPi, glm::vec3(0, 1, 0));
        const float bx = -rb - 0.008f;
        plates.append(build::buildBox({bx, y + half, 0.0f}, {0.01f, 0.008f, 0.07f}), back);
        plates.append(build::buildBox({bx, y - half, 0.0f}, {0.01f, 0.008f, 0.07f}), back);
        plates.append(build::buildBox({bx, y, 0.035f}, {0.01f, 2.0f * half, 0.008f}), back);
        plates.append(build::buildBox({bx, y, -0.035f}, {0.01f, 2.0f * half, 0.008f}), back);
        plates.append(build::buildBox({bx - 0.004f, y, 0.0f}, {0.006f, 0.006f, 0.07f}), back);
    }
    parts.push_back({std::move(strap), leather});
    parts.push_back({std::move(trim), metal(ctx, kAntiqueGold, 0.6f, 48.0f)});
    parts.push_back({std::move(plates), metal(ctx, kGold, 0.9f, 130.0f)});
    parts.push_back({std::move(faces), plateFace});
    parts.push_back({std::move(studs), metal(ctx, kPaleGold, 1.0f, 180.0f)});
    parts.push_back({std::move(gems), jewel({0.50f, 0.02f, 0.05f})});
    return parts;
}

// ---- 6. the basketball trophy --------------------------------------------------

std::vector<Part> basketballTrophy(const RoomContext& ctx) {
    std::vector<Part> parts;
    const glm::mat4 I(1.0f);
    const float b = turnedBase(ctx, "basketball", kTrophies[5].engraved, parts, 0.115f, 0.085f, 0.03f);
    MeshData gold;
    gold.append(buildLathe(profileFrom({{{{0.03f, b}, {0.026f, b + 0.03f}, {0.03f, b + 0.05f}, {0.045f, b + 0.06f}}},
                                        straight({0.045f, b + 0.06f}, {0.0f, b + 0.06f})})), I);
    // Three arms of scrollwork: each tapers as it curls up and out, ends in a
    // small spiral, and carries two leaves.
    const float r = 0.12f;
    const glm::vec3 ball(0.0f, b + 0.06f + 0.19f, 0.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    for (int k = 0; k < 3; ++k) {
        const float a = 2.0f * kPi * static_cast<float>(k) / 3.0f;
        const glm::vec3 d(std::cos(a), 0.0f, std::sin(a));
        std::vector<glm::vec3> path;
        std::vector<float> radii;
        for (int i = 0; i <= 24; ++i) {
            const float t = static_cast<float>(i) / 24.0f;
            const P2 q = curves::bezierCubic({0.035f, b + 0.05f}, {0.13f, b + 0.06f}, {0.13f, b + 0.16f}, {0.10f, b + 0.22f}, t);
            path.push_back(d * q.x + up * q.y);
            radii.push_back(0.011f - 0.005f * t);
        }
        for (int i = 1; i <= 16; ++i) {   // the curl: a shrinking spiral turning outwards
            const float t = static_cast<float>(i) / 16.0f;
            const float ang = t * 1.6f * kPi, rad = 0.025f * (1.0f - 0.6f * t);
            const glm::vec3 c0 = path[24] + d * 0.025f;
            path.push_back(c0 - d * (rad * std::cos(ang)) + up * (rad * std::sin(ang)));
            radii.push_back(0.006f - 0.003f * t);
        }
        gold.append(buildTube(path, 0.01f, 8, radii), I);
        for (int j : {8, 15}) {
            const glm::vec3 p = path[static_cast<std::size_t>(j)];
            const glm::vec3 along = glm::normalize(path[static_cast<std::size_t>(j) + 1] - p);
            const glm::vec3 axis = glm::normalize(d * 0.8f + along * 0.6f);
            const glm::vec3 y = glm::normalize(glm::cross(up, axis));
            gold.append(buildLeaf(0.045f, 0.018f), frame(p, axis, y, glm::cross(axis, y)));
        }
    }
    parts.push_back({std::move(gold), metal(ctx, kGold, 0.9f, 120.0f)});
    // The ball: pebbled leather in gold, the pebbles a relief our code scatters.
    raster::Canvas look(512, 256), height(512, 256);
    look.clear({196, 104, 34, 255});   // orange, to stand out from all the gold
    height.clear({60, 60, 60, 255});
    std::uint32_t seed = 2013u;
    auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    for (int i = 0; i < 6000; ++i)
        raster::circleFilled(height, static_cast<int>(next() % 512), static_cast<int>(next() % 256), 1, {150, 150, 150, 255});
    gfx::Material pebbled = engraved(ctx, "basketball", look, height, glm::vec3(1.0f), 0.35f, 40.0f, 0.6f);
    parts.push_back({buildSphere(ball, r, 40), pebbled});
    parts.push_back({ballSeams(ball, r), metal(ctx, {0.08f, 0.05f, 0.03f}, 0.4f, 24.0f)});
    return parts;
}

std::vector<Part> buildTrophy(int which, const RoomContext& ctx) {
    switch (which) {
        case 0:  return laurelWreath(ctx);
        case 1:  return championsCup(ctx);
        case 2:  return goldenBall(ctx);
        case 3:  return sprintMedal(ctx);
        case 4:  return twoBelts(ctx);
        default: return basketballTrophy(ctx);
    }
}


// ---- the room ------------------------------------------------------------------

class RoomThree final : public Room {
public:
    std::vector<Piece> build(const RoomContext& ctx) override {
        std::vector<Piece> pieces;
        spinners_.clear();
        footprints_.clear();
        if (!ctx.me) return pieces;
        const std::string dir = ctx.assetDir ? *ctx.assetDir : std::string("assets/");
        const Rect& rect     = ctx.me->rect;
        const Vec2  centre   = rect.centre();
        const float halfWall = (ctx.plan ? ctx.plan->wallThickness : cfg::kWallThickness) * 0.5f;

        // Adds a piece and returns where it is in the list.
        Add add = [&pieces](MeshData mesh, const gfx::Material& mat, const std::string& note) {
            Piece p;
            p.mesh     = std::move(mesh);
            p.material = mat;
            p.note     = note;
            pieces.push_back(std::move(p));
            return pieces.size() - 1;
        };

        buildHall(ctx, add);

        // Brass picture lights collect here, one piece for all of them.
        MeshData lampBodies, lampGlows;

        // ---- 1. the pop art portrait, in the middle of the east wall --------
        // The east wall faces the door; its hang points look west (-x).
        std::size_t heroHang = ctx.me->hangs.size();
        float best = 1e9f;
        for (std::size_t i = 0; i < ctx.me->hangs.size(); ++i) {
            const HangPoint& h = ctx.me->hangs[i];
            if (h.inwardNormal.x > -0.5f) continue;
            const float d = std::fabs((h.span.a.y + h.span.b.y) * 0.5f - centre.y);
            if (d < best) { best = d; heroHang = i; }
        }

        if (heroHang < ctx.me->hangs.size()) {
            const HangPoint hang = ontoLining(ctx.me->hangs[heroHang], rect, halfWall);
            const Wall wall = wallOf(hang);
            const raster::Canvas art = drawPopArt(dir + kFacePhoto);
            if (art.width() > 0) {
                float w = 0.0f, h = 0.0f;
                fitInside(static_cast<float>(art.width()) / static_cast<float>(art.height()),
                          hang.width() * kWallShare, kPopArtMaxH, w, h);
                gfx::Material print;
                print.albedo   = ctx.canvas("hall:popart", art);
                print.shading  = cfg::Shading::BlinnPhong;
                print.specular = 0.04f;   // a matte screen print
                hangWork(ctx, hang, w, h, kPopArtCentre, print, "pop art", add, lampBodies, lampGlows);
            }

            // The room's title above it, cut into a gold panel on a ledge.
            gfx::Material title = metal(ctx, glm::vec3(1.0f), 0.30f, 40.0f);
            title.albedo = ctx.canvas("hall:title", drawTitle());
            add(wallQuad(wall, kTitleCentre, kTitleW, kTitleH, 0.006f), title, "title");
            add(wallBox(wall, kTitleW + 0.08f, kTitleCentre - kTitleH * 0.5f - 0.04f,
                        kTitleCentre - kTitleH * 0.5f, 0.03f),
                metal(ctx, kAntiqueGold, 0.5f, 48.0f), "title ledge");
        }

        // ---- 2. the champions -------------------------------------------------
        std::size_t next = 0;
        for (std::size_t i = 0; i < ctx.me->hangs.size(); ++i) {
            if (i == heroHang) continue;
            const HangPoint hang = ontoLining(ctx.me->hangs[i], rect, halfWall);
            const Champion& ch = kChampions[next++ % kChampionCount];

            float w = 0.0f, h = 0.0f;
            fitInside(aspectOf(dir + ch.file), hang.width() * kWallShare, kChampionMaxH, w, h);

            gfx::Material print;
            print.albedo    = ctx.texture(ch.file);
            print.shading   = cfg::Shading::BlinnPhong;
            print.specular  = 0.30f;   // a glossy photographic print
            print.shininess = 90.0f;
            hangWork(ctx, hang, w, h, kChampionCentre, print, ch.name, add, lampBodies, lampGlows);

            gfx::Material label;
            label.albedo   = ctx.canvas(std::string("hall:label:") + ch.name, drawLabel(ch));
            label.shading  = cfg::Shading::Lambert;
            label.specular = 0.02f;
            const float labelY = kChampionCentre - h * 0.5f - kFrameExtent - kLabelGap - kLabelH * 0.5f;
            add(wallQuad(wallOf(hang), labelY, kLabelW, kLabelH, 0.006f), label,
                std::string("label: ") + ch.name);
        }

        add(std::move(lampBodies), metal(ctx, kAntiqueGold, 0.6f, 64.0f), "picture lights");
        add(std::move(lampGlows), glow(glm::vec3(1.0f, 0.86f, 0.62f), 2.2f), "picture light glow");

        // ---- 3. the trophies on their pedestals, and the stanchions -----------
        buildTrophies(ctx, centre, add);

        // ---- 4. the rings and the chandeliers ----------------------------------
        buildCeilingPieces(ctx, centre, add);

        // ---- 5. two gold athletes, in the east corners ------------------------
        const float ax = rect.x1 - halfWall - kAthleteFromWall;
        const Vec2 spots[2] = {{ax, rect.y0 + halfWall + kAthleteFromWall},
                               {ax, rect.y1 - halfWall - kAthleteFromWall}};
        for (int k = 0; k < 2; ++k) {
            const Vec2 at = spots[k];
            // Face the middle of the room. Heading 0 is north (+y), 90 is east (+x).
            const float heading = glm::degrees(std::atan2(centre.x - at.x, centre.y - at.y));
            add(build::buildPlinth(glm::vec3(worldX(at.x), 0.0f, worldZ(at.y)), 0.72f, 0.72f, kAthletePlinth),
                metal(ctx, kGold, 0.55f, 56.0f), "athlete plinth");
            footprints_.push_back({at, {0.36f, 0.36f}});
            std::vector<Piece> statue = ctx.model(kAthletes[k].model, at, heading,
                                                  kAthletes[k].height, kAthletePlinth, "bronze");
            for (Piece& p : statue) {
                // The bronze finish, regilded: same triplanar mapping, gold leaf instead.
                p.material.albedo    = ctx.texture("materials/gilt.jpg");
                p.material.tint      = kGold;
                p.material.specular  = 0.75f;
                p.material.shininess = 64.0f;
                p.note = "gold athlete";
                pieces.push_back(std::move(p));
            }
        }

        // Where everything starts. Most pieces never move.
        base_.clear();
        for (const Piece& p : pieces) base_.push_back(p.transform);
        for (const Spinner& sp : spinners_)
            for (std::size_t k : sp.pieces) base_[k] = spinTransform(sp);
        for (std::size_t k = 0; k < pieces.size(); ++k) pieces[k].transform = base_[k];
        return pieces;
    }

    bool update(float dt) override {
        time_  += dt;
        cheer_ *= std::exp(-kCheerDecay * dt);
        angle_ += (kSpinDegPerSec + kCheerDegPerSec * cheer_) * dt;
        if (angle_ > 360.0f) angle_ -= 360.0f;
        return !spinners_.empty();
    }

    void transforms(std::vector<glm::mat4>& out) override {
        out = base_;
        for (const Spinner& sp : spinners_)
            for (std::size_t k : sp.pieces)
                if (k < out.size()) out[k] = spinTransform(sp);
    }

    void onEnter() override { cheer_ = 1.0f; }   // the trophies spin up to greet you

    // The roped-off trophies and the two athletes' plinths are solid.
    std::vector<Footprint> footprints() const override { return footprints_; }

    std::string techniqueNotes() const override {
        return
            "Gallery Three: the Hall of Champions (Shawn Kuria)\n"
            "  The roped-off trophies and the athletes' plinths are solid (footprints).\n"
            "  Pop art made by our own code (PopArt.cpp): Rec. 601 luminance, a histogram\n"
            "    contrast stretch, then posterise, halftone dots (midpoint filled circles,\n"
            "    area = brightness), Floyd-Steinberg error diffusion and 4x4 Bayer dithering.\n"
            "  Gold damask and black terrazzo textures drawn by our own code: a Bresenham\n"
            "    trellis, midpoint ellipses and circles, chips from a seeded generator,\n"
            "    all wrapping at the tile edge so they repeat without a seam.\n"
            "  Trophies turned on a lathe: cubic Bezier profiles (r, y) revolved about y,\n"
            "    with normals from the profile tangent turned a quarter.\n"
            "  Our own meshes: torus (rings, wreath, seams), sphere, ellipsoid (laurel\n"
            "    leaves, with correct normals), medal, belt plates, strips, and tubes swept\n"
            "    along a path (cup handles, cradle prongs); ropes sag along a quadratic Bezier.\n"
            "  Metal: Blinn-Phong with a tinted albedo and a tight highlight (gold, silver,\n"
            "    bronze); emissive candle flames and picture lights; matte labels and carpet.\n"
            "  The trophies turn on their pedestals: model = T(home) x Ry(angle + phase) x S.\n"
            "    Walking in sets a cheer value that decays exponentially, so they spin up\n"
            "    and settle.\n";
    }

private:
    using Add = std::function<std::size_t(MeshData, const gfx::Material&, const std::string&)>;

    // Something on a turntable: where it stands, where in the turn it starts,
    // and which pieces it is made of.
    struct Spinner {
        glm::vec3 home{0.0f};
        float     phase = 0.0f;   // radians
        std::vector<std::size_t> pieces;
    };

    // The hall itself: wall linings, mouldings, the gilded vault, the floor, the runner.
    void buildHall(const RoomContext& ctx, const Add& add) {
        const Rect& r = ctx.me->rect;
        const float halfWall = (ctx.plan ? ctx.plan->wallThickness : cfg::kWallThickness) * 0.5f;
        const float wallTop  = ctx.plan ? ctx.plan->wallHeight : cfg::kWallHeight;
        const float apex     = ctx.plan ? ctx.plan->vaultApex : cfg::kVaultApex;

        // The four inner wall faces, each with the direction into the room.
        const float x0 = r.x0 + halfWall, x1 = r.x1 - halfWall;
        const float y0 = r.y0 + halfWall, y1 = r.y1 - halfWall;
        struct Face { Vec2 a, b, n; Side side; };
        const Face faces[4] = {{{x0, y0}, {x1, y0}, { 0.0f,  1.0f}, Side::South},
                               {{x0, y1}, {x1, y1}, { 0.0f, -1.0f}, Side::North},
                               {{x0, y0}, {x0, y1}, { 1.0f,  0.0f}, Side::West},
                               {{x1, y0}, {x1, y1}, {-1.0f,  0.0f}, Side::East}};
        const Door& door = ctx.me->door;
        const glm::mat4 I(1.0f);

        MeshData damask, panel, mouldings;
        for (const Face& f : faces) {
            const bool alongX = f.a.y == f.b.y;
            const float s0 = alongX ? f.a.x : f.a.y, s1 = alongX ? f.b.x : f.b.y;
            auto point = [&](float s) { return alongX ? Vec2{s, f.a.y} : Vec2{f.a.x, s}; };

            // Runs of solid wall, split where the door is.
            std::vector<std::array<float, 2>> runs;
            if (door.side == f.side) {
                runs.push_back({s0, door.a});
                runs.push_back({door.b, s1});
            } else {
                runs.push_back({s0, s1});
            }
            for (const auto& run : runs) {
                const float len = run[1] - run[0];
                if (len <= 0.01f) continue;
                const Wall w = wallThrough(point(run[0]), point(run[1]), f.n);
                const float hp = kCrownBottom - kDadoTop;
                damask.append(wallQuad(w, kDadoTop + hp * 0.5f, len, hp, kPanelOut,
                                       len / kDamaskTile, hp / kDamaskTile), I);
                panel.append(wallQuad(w, (kSkirtingTop + kDadoBottom) * 0.5f, len,
                                      kDadoBottom - kSkirtingTop, kPanelOut, len, 1.0f), I);
                mouldings.append(wallBox(w, len, 0.0f, kSkirtingTop, 0.025f), I);                 // skirting
                mouldings.append(wallBox(w, len, kDadoBottom, kDadoTop, 0.032f), I);              // dado rail
                mouldings.append(wallBox(w, len, kCrownBottom, wallTop, 0.07f), I);               // crown
                mouldings.append(wallBox(w, len, kCrownBottom - 0.06f, kCrownBottom - 0.04f, 0.035f), I);
            }
            // Over the door: wallpaper above the door head, the crown, and the head itself.
            if (door.side == f.side) {
                const Wall w = wallThrough(point(door.a), point(door.b), f.n);
                const float len = door.b - door.a;
                const float hp = kCrownBottom - cfg::kDoorHeight;
                damask.append(wallQuad(w, cfg::kDoorHeight + hp * 0.5f, len, hp, kPanelOut,
                                       len / kDamaskTile, hp / kDamaskTile), I);
                mouldings.append(wallBox(w, len, kCrownBottom, wallTop, 0.07f), I);
                mouldings.append(wallBox(w, len + 0.16f, cfg::kDoorHeight - 0.01f,
                                         cfg::kDoorHeight + 0.12f, 0.05f), I);
            }
        }

        gfx::Material paper;
        paper.albedo    = ctx.canvas("hall:damask", drawDamask());
        paper.shading   = cfg::Shading::BlinnPhong;
        paper.specular  = 0.28f;   // silk with a gold thread: a soft sheen
        paper.shininess = 20.0f;
        add(std::move(damask), paper, "gold damask walls");
        add(std::move(panel), metal(ctx, kAntiqueGold * 0.8f, 0.35f, 32.0f), "panel band");
        add(std::move(mouldings), metal(ctx, kGold, 0.6f, 56.0f), "mouldings");

        // The gilded vault: the plan's vaults again, pulled in a little towards
        // the room's middle (a scale about it) so they sit just inside the plain ones.
        if (ctx.plan) {
            const Vec2 c = r.centre();
            const glm::vec3 pivot(worldX(c.x), 0.0f, worldZ(c.y));
            const glm::mat4 inset =
                glm::translate(I, pivot) *
                glm::scale(I, glm::vec3(1.0f - kVaultInset / (r.width() * 0.5f),
                                        1.0f - kVaultInset / apex,
                                        1.0f - kVaultInset / (r.height() * 0.5f))) *
                glm::translate(I, -pivot);
            MeshData vault;
            vault.append(build::buildRoomVaults(*ctx.me, wallTop, apex, ctx.plan->wallThickness), inset);
            add(std::move(vault), metal(ctx, kPaleGold, 0.30f, 24.0f), "gilded vault");
        }

        // The floor: black terrazzo, polished, so the lamp leaves a highlight in it.
        {
            MeshData floor;
            const float h = 0.004f;
            const float u = (x1 - x0) / kFloorTile, v = (y1 - y0) / kFloorTile;
            floor.addQuad({worldX(x0), h, worldZ(y0)}, {worldX(x1), h, worldZ(y0)},
                          {worldX(x1), h, worldZ(y1)}, {worldX(x0), h, worldZ(y1)}, u, v);
            gfx::Material stone;
            stone.albedo    = ctx.canvas("hall:terrazzo", drawTerrazzo());
            stone.shading   = cfg::Shading::BlinnPhong;
            stone.specular  = 0.65f;
            stone.shininess = 90.0f;
            add(std::move(floor), stone, "terrazzo floor");
        }

        // A gold runner from the door to the stanchions. Matte: carpet does not shine.
        {
            const Vec2 c = r.centre();
            const float end = pedestalAt(c, 0).x - kPedestalSize * 0.5f - kBarrierMargin - 0.15f;
            gfx::Material carpet;
            carpet.albedo   = ctx.texture("materials/concrete.jpg");   // just for the weave
            carpet.tint     = glm::vec3(0.50f, 0.28f, 0.07f);
            carpet.shading  = cfg::Shading::Lambert;
            carpet.specular = 0.0f;
            add(build::buildBox(glm::vec3(worldX((x0 + end) * 0.5f), 0.006f, worldZ(c.y)),
                                glm::vec3(end - x0, 0.012f, kRunnerWidth)),
                carpet, "runner");
        }
    }

    // A framed work on the lining: the print, an inner gold frame, an outer
    // antique-gold frame, and a brass picture light above it.
    void hangWork(const RoomContext& ctx, const HangPoint& hang, float w, float h, float centreY,
                  const gfx::Material& print, const std::string& name, const Add& add,
                  MeshData& lampBodies, MeshData& lampGlows) {
        build::FramedWork inner = build::buildFramedWork(hang, w, h, centreY);
        build::FramedWork outer = build::buildFramedWork(hang, w + 2.0f * kOuterFrameGap,
                                                         h + 2.0f * kOuterFrameGap, centreY);
        add(std::move(inner.canvas), print, name);
        add(std::move(inner.frame), metal(ctx, kGold, 0.60f, 64.0f), "frame: " + name);
        add(std::move(outer.frame), metal(ctx, kAntiqueGold, 0.45f, 40.0f), "outer frame: " + name);

        const Wall wall = wallOf(hang);
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const float top  = centreY + h * 0.5f + kFrameExtent;
        const float hood = std::min(0.65f, w * 0.45f);
        lampBodies.append(wallBox(wall, 0.03f, top + 0.11f, top + 0.14f, 0.26f), glm::mat4(1.0f));   // arm
        const glm::vec3 hc = wall.mid + up * (top + 0.13f) + wall.n * 0.27f;
        lampBodies.append(build::buildBox(hc, glm::abs(wall.along) * hood + up * 0.05f + glm::abs(wall.n) * 0.075f),
                          glm::mat4(1.0f));
        lampGlows.append(build::buildBox(hc - up * 0.027f,
                                         glm::abs(wall.along) * (hood - 0.04f) + up * 0.006f +
                                             glm::abs(wall.n) * 0.045f),
                         glm::mat4(1.0f));
    }

    // Where the six pedestals stand on the plan: a row across the room, bowed
    // so the ends come forward to meet the visitor. Left to right from the door.
    static Vec2 pedestalAt(Vec2 centre, int i) {
        const float off = (2.5f - static_cast<float>(i)) * kPedestalSpacing;   // north is +y, the visitor's left
        const float t = off / (2.5f * kPedestalSpacing);
        return {centre.x + kPedestalBow * 0.5f - kPedestalBow * t * t, centre.y + off};
    }

    void buildTrophies(const RoomContext& ctx, Vec2 centre, const Add& add) {
        gfx::Material stoneTop;
        stoneTop.albedo    = ctx.canvas("hall:terrazzo", drawTerrazzo());   // the same texture as the floor
        stoneTop.specular  = 0.6f;
        stoneTop.shininess = 90.0f;

        for (int i = 0; i < 6; ++i) {
            const Vec2 at = pedestalAt(centre, i);
            const float height = kPedestalHeights[i];
            const glm::vec3 base(worldX(at.x), 0.0f, worldZ(at.y));
            add(build::buildPlinth(base, kPedestalSize, kPedestalSize, height),
                metal(ctx, kGold, 0.50f, 48.0f), std::string("pedestal: ") + kTrophies[i].who);
            add(build::buildBox(base + glm::vec3(0.0f, height + 0.012f, 0.0f),
                                glm::vec3(kPedestalSize + 0.04f, 0.024f, kPedestalSize + 0.04f)),
                stoneTop, "pedestal top");

            // The plate on its west face, towards the door.
            const glm::vec3 c = base + glm::vec3(-kPedestalSize * 0.5f - 0.004f, height - 0.16f, 0.0f);
            const glm::vec3 right(0.0f, 0.0f, 1.0f), up(0.0f, 1.0f, 0.0f);   // as seen facing east
            const float w = 0.42f, h = w * 112.0f / 384.0f;
            MeshData plate;
            plate.addQuad(c - right * (w * 0.5f) - up * (h * 0.5f), c + right * (w * 0.5f) - up * (h * 0.5f),
                          c + right * (w * 0.5f) + up * (h * 0.5f), c - right * (w * 0.5f) + up * (h * 0.5f));
            gfx::Material label;
            label.albedo   = ctx.canvas(std::string("hall:pedestal:") + kTrophies[i].who,
                                        drawPedestalLabel(kTrophies[i].what, kTrophies[i].who));
            label.shading  = cfg::Shading::Lambert;
            label.specular = 0.02f;
            add(std::move(plate), label, "pedestal plate");

            // The trophy, built at the origin, turned on its pedestal each frame.
            Spinner spin;
            spin.home  = base + glm::vec3(0.0f, height + 0.024f, 0.0f);
            spin.phase = static_cast<float>(i) * 1.3f;
            for (Part& part : buildTrophy(i, ctx))
                spin.pieces.push_back(add(std::move(part.mesh), part.material,
                                          std::string("trophy: ") + kTrophies[i].what));
            spinners_.push_back(std::move(spin));
        }

        // Stanchions: six posts round the pedestals, gold ropes sagging between
        // them. The posts stand at the corners and in the middle of the short
        // sides, so none stands in front of a trophy.
        const float x0 = pedestalAt(centre, 0).x - kPedestalSize * 0.5f - kBarrierMargin;
        const float x1 = pedestalAt(centre, 2).x + kPedestalSize * 0.5f + kBarrierMargin;
        const float y0 = pedestalAt(centre, 5).y - kPedestalSize * 0.5f - kBarrierMargin;
        const float y1 = pedestalAt(centre, 0).y + kPedestalSize * 0.5f + kBarrierMargin;
        const float xm = (x0 + x1) * 0.5f;
        const Vec2 posts[6] = {{x0, y0}, {x0, y1}, {xm, y1}, {x1, y1}, {x1, y0}, {xm, y0}};
        // The ropes close the trophies off: the whole rectangle, out to the
        // edge of the posts' feet, is solid.
        const float foot = 0.16f;
        footprints_.push_back({{(x0 + x1) * 0.5f, (y0 + y1) * 0.5f},
                               {(x1 - x0) * 0.5f + foot, (y1 - y0) * 0.5f + foot}});
        const glm::mat4 I(1.0f);
        MeshData brass, ropes;
        for (const Vec2& p : posts) {
            const glm::vec3 b(worldX(p.x), 0.0f, worldZ(p.y));
            brass.append(build::buildCylinder(b, 0.16f, 0.03f, 28), I);
            brass.append(build::buildCylinder(b + glm::vec3(0, 0.03f, 0), 0.022f, kPostHeight - 0.03f, 14), I);
            brass.append(buildSphere(b + glm::vec3(0, kPostHeight + 0.03f, 0), 0.05f, 14), I);
        }
        for (int i = 0; i < 6; ++i) {
            const Vec2 a = posts[i], b = posts[(i + 1) % 6];
            const glm::vec3 pa(worldX(a.x), kRopeHeight, worldZ(a.y)), pb(worldX(b.x), kRopeHeight, worldZ(b.y));
            const float span = glm::length(pb - pa);
            ropes.append(buildTube(saggingRope(pa, pb, kRopeSag * span, 24), kRopeRadius, 10), I);
        }
        add(std::move(brass), metal(ctx, kGold, 0.70f, 80.0f), "stanchions");
        add(std::move(ropes), metal(ctx, kGold * 0.9f, 0.25f, 16.0f), "ropes");   // soft twisted cord
    }

    void buildCeilingPieces(const RoomContext& ctx, Vec2 centre, const Add& add) {
        // The rings go under the vault furthest from the door; chandeliers
        // hang under every vault.
        std::vector<float> vaultX;
        for (const Segment& v : ctx.me->vaults) vaultX.push_back((v.a.x + v.b.x) * 0.5f);
        if (vaultX.empty()) vaultX.push_back(centre.x);
        const float ringX = *std::max_element(vaultX.begin(), vaultX.end());
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const glm::mat4 I(1.0f);

        // ---- the five rings, facing the door -----------------------------------
        // Top row of three, bottom row of two between them. Alternate rings lean
        // a few degrees about the vertical, so each passes in front of one
        // neighbour and behind the other, and they weave together.
        {
            const float gap = kRingRadius * 2.25f;
            const glm::vec3 c(worldX(ringX), kRingsTopCentre, worldZ(centre.y));
            const glm::vec3 across(0.0f, 0.0f, 1.0f);   // to the right as you walk in
            const glm::vec3 offsets[5] = {-gap * across, glm::vec3(0.0f), gap * across,
                                          -gap * 0.5f * across - up * kRingRadius,
                                          gap * 0.5f * across - up * kRingRadius};
            MeshData rings, rods;
            const MeshData torus = buildTorus(kRingRadius, kRingTube);
            for (int i = 0; i < 5; ++i) {
                const float lean = (i % 2 == 0 ? 1.0f : -1.0f) * kRingTilt;
                rings.append(torus, glm::translate(I, c + offsets[i]) * glm::rotate(I, glm::radians(lean), up));
            }
            for (int i = 0; i < 3; ++i) {
                const glm::vec3 top = c + offsets[i] + up * (kRingRadius + kRingTube);
                rods.append(build::buildCylinder(top, 0.008f, kHangTop - top.y, 8), I);
            }
            add(std::move(rings), metal(ctx, kGold, 0.75f, 96.0f), "gold rings");
            add(std::move(rods), metal(ctx, kAntiqueGold, 0.4f, 32.0f), "ring rods");
        }

        // ---- two chandeliers ---------------------------------------------------
        MeshData body, wax, flames, crystal;
        const glm::mat4 flat = glm::rotate(I, glm::radians(90.0f), glm::vec3(0, 0, 1));   // torus axis up
        struct Tier { float y, R, r; int candles; };
        const Tier tiers[2] = {{0.0f, 0.62f, 0.030f, 12}, {0.30f, 0.36f, 0.022f, 8}};
        for (float lampX : vaultX)
        for (float dy : {-kChandelierSpread, kChandelierSpread}) {
            const glm::vec3 c(worldX(lampX), kChandelierY, worldZ(centre.y + dy));
            // A rod from the vault, a column, a ball under it and one on top.
            body.append(build::buildCylinder(c + up * 0.40f, 0.012f, kHangTop - c.y - 0.40f, 8), I);
            body.append(build::buildCylinder(c - up * 0.12f, 0.045f, 0.52f, 16), I);
            body.append(buildSphere(c - up * 0.15f, 0.08f, 14), I);
            body.append(buildSphere(c + up * 0.42f, 0.06f, 12), I);
            for (const Tier& t : tiers) {
                const glm::vec3 hoop = c + up * t.y;
                body.append(buildTorus(t.R, t.r, 48, 10), glm::translate(I, hoop) * flat);
                for (int k = 0; k < t.candles; ++k) {
                    const float a = 2.0f * kPi * (static_cast<float>(k) + 0.5f) / static_cast<float>(t.candles);
                    const glm::vec3 p = hoop + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * t.R;
                    body.append(build::buildCylinder(p, 0.032f, 0.03f, 12), I);              // the cup
                    wax.append(build::buildCylinder(p + up * 0.03f, 0.016f, 0.12f, 10), I);
                    flames.append(build::buildCone(p + up * 0.155f, 0.016f, 0.07f, 8), I);
                    // A crystal drop hanging under each cup.
                    crystal.append(buildSphere(p - up * 0.09f, 0.022f, 8), I);
                    crystal.append(build::buildCylinder(p - up * 0.07f, 0.002f, 0.07f, 4), I);
                }
                // Four arms, each a quadratic Bezier dipping from the column to the hoop.
                for (int k = 0; k < 4; ++k) {
                    const float a = kPi * 0.5f * static_cast<float>(k) + kPi * 0.25f;
                    const glm::vec3 out(std::cos(a), 0.0f, std::sin(a));
                    std::vector<glm::vec3> arm;
                    for (int s = 0; s <= 10; ++s) {
                        const curves::P2 q = curves::bezierQuadratic({0.04f, 0.08f}, {t.R * 0.5f, -0.12f},
                                                                     {t.R, 0.0f}, static_cast<float>(s) / 10.0f);
                        arm.push_back(hoop + out * q.x + up * q.y);
                    }
                    body.append(buildTube(arm, 0.012f, 8), I);
                }
            }
        }
        add(std::move(body), metal(ctx, kGold, 0.70f, 80.0f), "chandeliers");
        gfx::Material candle;
        candle.tint     = glm::vec3(0.96f, 0.92f, 0.82f);
        candle.shading  = cfg::Shading::Lambert;
        candle.specular = 0.05f;
        candle.emissive = 0.15f;   // a little light passes through the wax
        add(std::move(wax), candle, "candles");
        add(std::move(flames), glow(glm::vec3(1.0f, 0.72f, 0.32f), 4.0f), "candle flames");
        gfx::Material glass;   // cut crystal: almost white, the sharpest highlight in the room
        glass.tint      = glm::vec3(0.92f, 0.90f, 0.86f);
        glass.specular  = 1.0f;
        glass.shininess = 220.0f;
        glass.emissive  = 0.25f;   // catching the candle light
        add(std::move(crystal), glass, "crystal drops");
    }

    glm::mat4 spinTransform(const Spinner& s) const {
        return glm::translate(glm::mat4(1.0f), s.home) *
               glm::rotate(glm::mat4(1.0f), glm::radians(angle_) + s.phase, glm::vec3(0.0f, 1.0f, 0.0f)) *
               glm::scale(glm::mat4(1.0f), glm::vec3(kTrophyScale));
    }

    std::vector<Spinner>   spinners_;
    std::vector<Footprint> footprints_;
    std::vector<glm::mat4> base_;
    float time_  = 0.0f;
    float angle_ = 0.0f;
    float cheer_ = 0.0f;
};

} // namespace

RoomPtr makeRoomThree() { return std::make_unique<RoomThree>(); }

} // namespace lumen
