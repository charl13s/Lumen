// RoomFour.cpp - Gallery Four. Owner: Erykah Diang'a (168661)
//
// Magic realism: five paintings by Rob Gonsalves, the Canadian painter of
// magic realism, under a night sky.
//
// Stardust is the centrepiece. It hangs large in the middle of the far wall,
// facing the door, between Arboreal Office and Water Dancing. Dancing Wind is
// on the west wall and Sun Sets Sail on the east. The walls carry a faint
// nebula, drawn by our own code, and the vaults are the night sky it belongs
// to. In the middle of the room a bronze study after Liang Binbin's Heart of
// the Horizon stands on a plinth, its open ring framing Stardust as you walk
// in. It sways slowly, so the light slides round it.
//
// Technique: Gouraud shading. The lamp is worked out at each vertex and
// blended across each triangle, so every surface here is cut into small
// triangles: walls, vaults, canvases and the sculpture's swept tubes.

#include "core/Room.h"
#include "core/Curves.h"
#include "core/Label.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace lumen {
namespace {

using gfx::MeshData;
using gfx::Vertex;

constexpr float kPi = 3.14159265358979f;

// ---- the room's own constants ----------------------------------------------

// The nebula lining on the walls and the night sky on the vaults.
constexpr float kLiningOut   = 0.010f;  // m, the lining off the plan's wall face
constexpr float kSkyIn       = 0.015f;  // m, the sky sits this far inside the plain vault
constexpr float kNebulaTile  = 4.20f;   // m, one repeat of the nebula (the wall's height)
constexpr float kWallCell    = 0.30f;   // m, Gouraud needs small triangles to show the lamp
constexpr float kSkirtingH   = 0.14f;   // m
constexpr float kSkirtingD   = 0.022f;  // m
constexpr int   kNebulaPx    = 512;     // the nebula texture, square
constexpr float kWallNebula  = 0.45f;   // how strongly the clouds show on the walls: subtle
constexpr float kSkyNebula   = 0.95f;   // and on the vaults

// The works.
constexpr float kWorkCentreY   = 1.75f;  // m to the centre of every work
constexpr float kCanvasCell    = 0.10f;  // m, the largest triangle edge on a canvas or frame
constexpr float kLabelW        = 0.36f;  // m
constexpr float kLabelMaxH     = 0.50f;  // m
constexpr int   kLabelPixels   = 420;
constexpr int   kLabelFont     = 3;
constexpr float kLabelGap      = 0.08f;  // m from the frame to its label
constexpr float kLabelCentreY  = 1.30f;  // m, labels sit beside the work, low on the right
constexpr float kStardustGlow  = 0.05f;  // Stardust is lit a little from within

// The sculpture.
constexpr float kPlinthW       = 0.80f;  // m square
constexpr float kPlinthH       = 1.00f;  // m, so the ring frames Stardust at eye height
constexpr float kSolidMargin   = 0.06f;  // m, how far past the plinth you are stopped
constexpr float kRingR         = 0.48f;  // m, the ring's radius
constexpr float kSwayDeg       = 22.0f;  // the sculpture turns this far either way
constexpr float kSwayPeriod    = 26.0f;  // s for one full sway

const glm::vec3 kBronze   {0.21f, 0.12f, 0.045f};   // linear: tints are dark, the lamp is bright
const glm::vec3 kPatina   {0.035f, 0.095f, 0.075f};
const glm::vec3 kDarkBronze{0.07f, 0.05f, 0.03f};
const glm::vec3 kPlinthColour{0.11f, 0.11f, 0.15f};
const glm::vec3 kSkirtingColour{0.06f, 0.06f, 0.09f};
const glm::vec3 kFrameTint{0.80f, 0.66f, 0.40f};

// Where a work hangs: on which wall, and where along it (plan metres: x for
// the south wall, y for the west and east walls).
enum class Wall { South, West, East };

struct Work {
    const char* file;      // inside assets/
    Wall        wall;
    float       at;        // centre along the wall
    float       height;    // m, the canvas; the width follows the image's shape
    bool        centrepiece;
    label::Text text;
};

// The far wall faces the door: Stardust in the middle, the two tall works
// either side. The two wide works have a side wall each.
std::vector<Work> works() {
    const std::string credit = "Reproduced for study. (c) the estate of Rob Gonsalves.";
    return {
        {"rooms/R4/stardust.jpg", Wall::South, 28.0f, 2.30f, true,
         {"Stardust", {"Rob Gonsalves", "Canadian magic realism"},
          "A tower's spiral stair turns into a spiral of stars, with the moon at its eye.",
          credit}},
        {"rooms/R4/arboreal-office.jpg", Wall::South, 25.6f, 1.75f, false,
         {"Arboreal Office", {"Rob Gonsalves", "Canadian magic realism"},
          "Office towers grow out of a pine forest, lit by a campfire.", credit}},
        {"rooms/R4/water-dancing.jpg", Wall::South, 30.4f, 1.75f, false,
         {"Water Dancing", {"Rob Gonsalves, 2011", "Canadian magic realism"},
          "A waterfall falls as a line of dancers.", credit}},
        {"rooms/R4/dancing-wind.jpg", Wall::West, 4.5f, 1.70f, false,
         {"Dancing Wind", {"Rob Gonsalves", "Canadian magic realism"},
          "Curtains blown by the wind waltz with the dancers.", credit}},
        {"rooms/R4/sun-sets-sail.jpg", Wall::East, 4.5f, 1.45f, false,
         {"Sun Sets Sail", {"Rob Gonsalves, 2001", "Canadian magic realism"},
          "Sailing ships and an aqueduct become one another at sunset.", credit}},
    };
}

label::Text sculptureLabel() {
    return {"Heart of the Horizon (study)",
            {"Gallery Four, 2026", "Bronze and verdigris, drawn by our own code"},
            "After Liang Binbin's Heart of the Horizon (2024): a polished arc carried "
            "on a breaking wave of green patina.",
            "Swept tubes, Gouraud shaded."};
}

glm::vec3 w3(float x, float y, float z) { return glm::vec3(worldX(x), y, worldZ(z)); }

// ---- the nebula, drawn by our own code ---------------------------------------
//
// Value noise on a lattice that wraps, summed over octaves (fractal Brownian
// motion), so the texture repeats without a seam. A second noise bends the
// first (domain warping), which pulls the clouds into wisps.

std::uint32_t hash2(int x, int y, std::uint32_t seed) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u +
                      static_cast<std::uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// One octave: smooth noise in [0, 1], repeating every `period` lattice cells.
float valueNoise(float x, float y, int period, std::uint32_t seed) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
    const float tx = x - fx, ty = y - fy;
    const float sx = tx * tx * (3.0f - 2.0f * tx), sy = ty * ty * (3.0f - 2.0f * ty);
    auto at = [&](int i, int j) {
        i = ((i % period) + period) % period;
        j = ((j % period) + period) % period;
        return static_cast<float>(hash2(i, j, seed) & 0xffffu) / 65535.0f;
    };
    const float a = at(ix, iy), b = at(ix + 1, iy), c = at(ix, iy + 1), d = at(ix + 1, iy + 1);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

// u, v in [0, 1): five octaves, each twice as fine and half as strong.
float fbm(float u, float v, std::uint32_t seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    int period = 4;
    for (int o = 0; o < 5; ++o) {
        sum  += amp * valueNoise(u * static_cast<float>(period), v * static_cast<float>(period), period, seed + static_cast<std::uint32_t>(o) * 101u);
        norm += amp;
        amp  *= 0.5f;
        period *= 2;
    }
    return sum / norm;
}

float smooth(float e0, float e1, float x) {
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct NebulaField {
    std::vector<float> violet, blue;   // cloud density, 0 to 1, per pixel
};

NebulaField nebulaField() {
    const int S = kNebulaPx;
    NebulaField f;
    f.violet.resize(static_cast<std::size_t>(S) * S);
    f.blue.resize(f.violet.size());
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            const float u = static_cast<float>(x) / S, v = static_cast<float>(y) / S;
            // The warp is itself periodic, so the bent clouds still tile.
            const float wu = fbm(u, v, 11u), wv = fbm(u, v, 23u);
            const float a = fbm(u + 0.35f * wu, v + 0.35f * wv, 37u);
            const float b = fbm(u - 0.30f * wv, v + 0.30f * wu, 53u);
            const std::size_t i = static_cast<std::size_t>(y) * S + x;
            f.violet[i] = smooth(0.42f, 0.78f, a);
            f.blue[i]   = smooth(0.48f, 0.82f, b);
        }
    }
    return f;
}

// The nebula at a given strength: 0 is plain night, 1 the full sky.
raster::Canvas drawNebula(const NebulaField& f, float strength, int stars, std::uint32_t seed) {
    const int S = kNebulaPx;
    raster::Canvas c(S, S);
    const glm::vec3 night{12.0f, 13.0f, 30.0f};
    const glm::vec3 violet{78.0f, 46.0f, 112.0f};
    const glm::vec3 blue{34.0f, 72.0f, 128.0f};
    const glm::vec3 rose{124.0f, 58.0f, 104.0f};
    auto put = [&](int x, int y, glm::vec3 col) {
        x = ((x % S) + S) % S;   // stars wrap at the edge too
        y = ((y % S) + S) % S;
        col = glm::clamp(col, glm::vec3(0.0f), glm::vec3(255.0f));
        c.set(x, y, raster::Colour{static_cast<std::uint8_t>(col.r), static_cast<std::uint8_t>(col.g),
                                   static_cast<std::uint8_t>(col.b), 255});
    };
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * S + x;
            const float a = f.violet[i] * strength, b = f.blue[i] * strength;
            glm::vec3 col = night + (violet - night) * a * 0.55f + (blue - night) * b * 0.50f +
                            (rose - night) * a * b * 0.45f;
            put(x, y, col);
        }
    }
    // Stars: most a single faint pixel, a few brighter with a small cross.
    std::uint32_t s = seed;
    auto next = [&s]() { s = s * 1664525u + 1013904223u; return s >> 8; };
    for (int k = 0; k < stars; ++k) {
        const int x = static_cast<int>(next() % S), y = static_cast<int>(next() % S);
        const float bright = (60.0f + static_cast<float>(next() % 120)) * (0.45f + 0.55f * strength);
        const glm::vec3 base = glm::vec3(c.get(x, y).r, c.get(x, y).g, c.get(x, y).b);
        const glm::vec3 star = base + glm::vec3(bright, bright, bright * 1.08f);
        put(x, y, star);
        if (next() % 9 == 0) {
            const glm::vec3 halo = base + glm::vec3(bright * 0.35f);
            put(x + 1, y, halo); put(x - 1, y, halo); put(x, y + 1, halo); put(x, y - 1, halo);
        }
    }
    return c;
}

// ---- small triangles, for Gouraud --------------------------------------------

Vertex mixVertex(const Vertex& a, const Vertex& b) {
    Vertex m;
    m.position = (a.position + b.position) * 0.5f;
    m.normal   = glm::normalize(a.normal + b.normal);
    m.uv       = (a.uv + b.uv) * 0.5f;
    m.tangent  = glm::normalize(a.tangent + b.tangent);
    return m;
}

// Splits a triangle into four, again and again, until no edge is longer than
// maxEdge. Every attribute is interpolated, so the surface looks the same,
// but the lamp is now worked out at many more points.
void splitTriangle(const Vertex& a, const Vertex& b, const Vertex& c, float maxEdge, MeshData& out) {
    const float longest = std::max({glm::length(b.position - a.position),
                                    glm::length(c.position - b.position),
                                    glm::length(a.position - c.position)});
    if (longest <= maxEdge) {
        out.addTriangle(a, b, c);
        return;
    }
    const Vertex ab = mixVertex(a, b), bc = mixVertex(b, c), ca = mixVertex(c, a);
    splitTriangle(a, ab, ca, maxEdge, out);
    splitTriangle(ab, b, bc, maxEdge, out);
    splitTriangle(ca, bc, c, maxEdge, out);
    splitTriangle(ab, bc, ca, maxEdge, out);
}

MeshData refine(const MeshData& in, float maxEdge) {
    MeshData out;
    for (std::size_t i = 0; i + 2 < in.indices.size(); i += 3)
        splitTriangle(in.vertices[in.indices[i]], in.vertices[in.indices[i + 1]],
                      in.vertices[in.indices[i + 2]], maxEdge, out);
    return out;
}

// A wall lining from a to b on the plan, facing n, from height y0 to y1, as a
// grid of small cells. Texture coordinates are in nebula tiles, so neighbouring
// runs of wall carry on the pattern.
void wallGrid(Vec2 a, Vec2 b, Vec2 n, float y0, float y1, MeshData& out) {
    const glm::vec3 in(n.x, 0.0f, -n.y);
    glm::vec3 p0 = w3(a.x, 0.0f, a.y) + in * kLiningOut;
    glm::vec3 p1 = w3(b.x, 0.0f, b.y) + in * kLiningOut;
    float u0 = (std::fabs(n.x) > 0.5f) ? a.y : a.x;   // where along the wall a and b are
    float u1 = (std::fabs(n.x) > 0.5f) ? b.y : b.x;
    // Wind it so the face looks into the room.
    if (glm::dot(glm::cross(p1 - p0, glm::vec3(0.0f, 1.0f, 0.0f)), in) < 0.0f) {
        std::swap(p0, p1);
        std::swap(u0, u1);
    }
    const float len = glm::length(p1 - p0);
    if (len < 0.01f || y1 - y0 < 0.01f) return;
    const glm::vec3 along = (p1 - p0) / len;
    const int nu = std::max(1, static_cast<int>(std::ceil(len / kWallCell)));
    const int nv = std::max(1, static_cast<int>(std::ceil((y1 - y0) / kWallCell)));

    const unsigned base = static_cast<unsigned>(out.vertices.size());
    for (int j = 0; j <= nv; ++j) {
        const float y = y0 + (y1 - y0) * static_cast<float>(j) / static_cast<float>(nv);
        for (int i = 0; i <= nu; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(nu);
            Vertex v;
            v.position = p0 + along * (len * t) + glm::vec3(0.0f, y, 0.0f);
            v.normal   = in;
            v.tangent  = along;
            v.uv       = glm::vec2((u0 + (u1 - u0) * t) / kNebulaTile, y / kNebulaTile);
            out.vertices.push_back(v);
        }
    }
    const unsigned row = static_cast<unsigned>(nu + 1);
    for (int j = 0; j < nv; ++j) {
        for (int i = 0; i < nu; ++i) {
            const unsigned k = base + static_cast<unsigned>(j) * row + static_cast<unsigned>(i);
            out.indices.insert(out.indices.end(), {k, k + 1, k + row + 1, k, k + row + 1, k + row});
        }
    }
}

// The night sky: the plan's vaults again, made of small cells with texture
// coordinates in metres, and moved a little way into the room so they sit
// just inside the plain plaster ones. The shape follows build::buildVault:
// a quadratic Bezier arch, with the light slot left open along the crown.
MeshData skyVaults(const RoomPlan& room, float wallHeight, float apex, float wallThickness) {
    MeshData out;
    if (room.vaults.empty()) return out;
    const bool alongY = !room.vaults.front().horizontal();
    const float span = (alongY ? room.rect.width() : room.rect.height()) /
                       static_cast<float>(room.vaults.size());
    const float radius = span * 0.5f, rise = apex - wallHeight;
    const float half = wallThickness * 0.5f;
    const curves::P2 c0{-radius, 0.0f}, c1{0.0f, rise * 2.0f}, c2{radius, 0.0f};
    constexpr int   kAcross   = 24;      // as many steps as the plain vault, so the slot matches
    constexpr float kSlotHalf = 0.08f;
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    for (Segment crown : room.vaults) {
        if (alongY) { crown.a.y = room.rect.y0 + half; crown.b.y = room.rect.y1 - half; }
        else        { crown.a.x = room.rect.x0 + half; crown.b.x = room.rect.x1 - half; }
        const float dx = crown.b.x - crown.a.x, dy = crown.b.y - crown.a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-4f) continue;
        const float ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
        const glm::vec3 crownDir(ux, 0.0f, -uy), acrossDir(nx, 0.0f, -ny);

        // A point on the arch at t, `along` the crown, and its normal into the room.
        auto point = [&](float t, float along) {
            const curves::P2 p = curves::bezierQuadratic(c0, c1, c2, t);
            const float e = 1e-3f;
            const curves::P2 q0 = curves::bezierQuadratic(c0, c1, c2, std::max(0.0f, t - e));
            const curves::P2 q1 = curves::bezierQuadratic(c0, c1, c2, std::min(1.0f, t + e));
            const float ddx = q1.x - q0.x, ddh = q1.y - q0.y;
            glm::vec3 nrm = glm::normalize(acrossDir * ddh - up * ddx);   // down and inwards
            Vertex v;
            v.position = w3(crown.a.x + ux * along + nx * p.x, wallHeight + p.y,
                            crown.a.y + uy * along + ny * p.x) + nrm * kSkyIn;
            v.normal  = nrm;
            v.tangent = crownDir;
            return v;
        };

        // Arc length across the arch, for the texture's second coordinate.
        std::vector<float> arc(kAcross + 1, 0.0f);
        for (int i = 1; i <= kAcross; ++i) {
            const curves::P2 a = curves::bezierQuadratic(c0, c1, c2, static_cast<float>(i - 1) / kAcross);
            const curves::P2 b = curves::bezierQuadratic(c0, c1, c2, static_cast<float>(i) / kAcross);
            arc[static_cast<std::size_t>(i)] = arc[static_cast<std::size_t>(i - 1)] +
                                               std::hypot(b.x - a.x, b.y - a.y);
        }

        const int nAlong = std::max(1, static_cast<int>(std::ceil(len / kWallCell)));
        for (int i = 0; i < kAcross; ++i) {
            const float t0 = static_cast<float>(i) / kAcross, t1 = static_cast<float>(i + 1) / kAcross;
            if (std::fabs((t0 + t1) * 0.5f - 0.5f) < kSlotHalf) continue;   // the light slot
            for (int j = 0; j < nAlong; ++j) {
                const float a0 = len * static_cast<float>(j) / static_cast<float>(nAlong);
                const float a1 = len * static_cast<float>(j + 1) / static_cast<float>(nAlong);
                Vertex v00 = point(t0, a0), v10 = point(t1, a0), v11 = point(t1, a1), v01 = point(t0, a1);
                const float s0 = arc[static_cast<std::size_t>(i)] / kNebulaTile;
                const float s1 = arc[static_cast<std::size_t>(i + 1)] / kNebulaTile;
                v00.uv = {a0 / kNebulaTile, s0}; v10.uv = {a0 / kNebulaTile, s1};
                v11.uv = {a1 / kNebulaTile, s1}; v01.uv = {a1 / kNebulaTile, s0};
                // Front faces look into the room.
                if (glm::dot(glm::cross(v10.position - v00.position, v01.position - v00.position),
                             v00.normal) > 0.0f) {
                    out.addTriangle(v00, v10, v11);
                    out.addTriangle(v00, v11, v01);
                } else {
                    out.addTriangle(v00, v11, v10);
                    out.addTriangle(v00, v01, v11);
                }
            }
        }

        // The half-moons where each vault meets the end walls.
        for (int end = 0; end < 2; ++end) {
            const float along = end == 0 ? 0.0f : len;
            const glm::vec3 inward = crownDir * (end == 0 ? 1.0f : -1.0f);
            const Vec2 foot{crown.a.x + ux * along, crown.a.y + uy * along};
            MeshData fan;
            Vertex centre;
            centre.position = w3(foot.x, wallHeight, foot.y) + inward * kSkyIn;
            centre.normal = inward;
            centre.tangent = acrossDir;
            centre.uv = {0.0f, 0.0f};
            for (int i = 0; i < kAcross; ++i) {
                Vertex p[2];
                for (int k = 0; k < 2; ++k) {
                    const float t = static_cast<float>(i + k) / kAcross;
                    const curves::P2 q = curves::bezierQuadratic(c0, c1, c2, t);
                    p[k].position = w3(foot.x + nx * q.x, wallHeight + q.y, foot.y + ny * q.x) + inward * kSkyIn;
                    p[k].normal = inward;
                    p[k].tangent = acrossDir;
                    p[k].uv = {q.x / kNebulaTile, q.y / kNebulaTile};
                }
                if (glm::dot(glm::cross(p[0].position - centre.position, p[1].position - centre.position),
                             inward) < 0.0f)
                    std::swap(p[0], p[1]);
                fan.addTriangle(centre, p[0], p[1]);
            }
            const MeshData fine = refine(fan, kWallCell * 1.5f);
            out.append(fine, glm::mat4(1.0f));
        }
    }

    // Where two vaults meet, each is moved inwards along its own normal, which
    // opens a crack along the valley. A narrow strip underneath closes it.
    for (std::size_t i = 0; i + 1 < room.vaults.size(); ++i) {
        const Segment& p = room.vaults[i];
        const Segment& q = room.vaults[i + 1];
        const float vx = (p.a.x + q.a.x) * 0.5f, vy = (p.a.y + q.a.y) * 0.5f;
        constexpr float kStrip = 0.08f;   // m wide
        const float h = wallHeight - 0.004f;
        MeshData strip;
        glm::vec3 c[4];
        if (alongY) {
            const float ya = room.rect.y0 + half, yb = room.rect.y1 - half;
            c[0] = w3(vx - kStrip, h, ya); c[1] = w3(vx + kStrip, h, ya);
            c[2] = w3(vx + kStrip, h, yb); c[3] = w3(vx - kStrip, h, yb);
        } else {
            const float xa = room.rect.x0 + half, xb = room.rect.x1 - half;
            c[0] = w3(xa, h, vy - kStrip); c[1] = w3(xb, h, vy - kStrip);
            c[2] = w3(xb, h, vy + kStrip); c[3] = w3(xa, h, vy + kStrip);
        }
        // Facing down, into the room.
        if (glm::cross(c[1] - c[0], c[3] - c[0]).y > 0.0f) std::swap(c[1], c[3]);
        strip.addQuad(c[0], c[1], c[2], c[3], 2.0f * kStrip / kNebulaTile,
                      glm::length(c[3] - c[0]) / kNebulaTile);
        out.append(refine(strip, kWallCell), glm::mat4(1.0f));
    }
    return out;
}

// ---- the sculpture -------------------------------------------------------------
//
// Built round the origin, which is the top of the plinth. The ring stands in
// the x-y plane, so it faces along z: towards the door and Stardust.

// A tube swept along a path. At each point the cross-section is an ellipse:
// `a` across the ring (towards its centre) and `b` along the ring's axis,
// turned by `twist` radians. A flat, twisting ellipse reads as a ribbon.
MeshData sweep(const std::vector<glm::vec3>& path, const std::vector<float>& a,
               const std::vector<float>& b, const std::vector<float>& twist, glm::vec3 axis,
               int around = 20) {
    MeshData m;
    const std::size_t n = path.size();
    if (n < 2) return m;
    float travelled = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        if (i > 0) travelled += glm::length(path[i] - path[i - 1]);
        const glm::vec3 T = glm::normalize(path[std::min(i + 1, n - 1)] - path[i > 0 ? i - 1 : 0]);
        glm::vec3 B = axis - T * glm::dot(axis, T);
        B = glm::length(B) > 1e-4f ? glm::normalize(B) : glm::normalize(glm::cross(T, glm::vec3(0, 1, 0)));
        const glm::vec3 N = glm::cross(B, T);
        const float c = std::cos(twist[i]), s = std::sin(twist[i]);
        const glm::vec3 N2 = N * c + B * s, B2 = B * c - N * s;
        const float ai = std::max(a[i], 1e-4f), bi = std::max(b[i], 1e-4f);
        for (int k = 0; k <= around; ++k) {
            const float u = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(around);
            Vertex v;
            v.position = path[i] + N2 * (ai * std::cos(u)) + B2 * (bi * std::sin(u));
            v.normal   = glm::normalize(N2 * (std::cos(u) / ai) + B2 * (std::sin(u) / bi));
            v.tangent  = T;
            v.uv       = {travelled, static_cast<float>(k) / static_cast<float>(around)};
            m.vertices.push_back(v);
        }
    }
    // Wind the first quad so it faces outwards, and the rest the same way.
    const unsigned row = static_cast<unsigned>(around + 1);
    bool flip = false;
    {
        const Vertex& v0 = m.vertices[0];
        const Vertex& v1 = m.vertices[row];
        const Vertex& v2 = m.vertices[1];
        flip = glm::dot(glm::cross(v1.position - v0.position, v2.position - v0.position), v0.normal) < 0.0f;
    }
    for (std::size_t i = 0; i + 1 < n; ++i) {
        for (int k = 0; k < around; ++k) {
            const unsigned p = static_cast<unsigned>(i) * row + static_cast<unsigned>(k);
            const unsigned q = p + row;
            if (!flip) m.indices.insert(m.indices.end(), {p, q, q + 1, p, q + 1, p + 1});
            else       m.indices.insert(m.indices.end(), {p, q + 1, q, p, p + 1, q + 1});
        }
    }
    return m;
}

struct SculptureMeshes {
    MeshData bronze;   // the polished arc
    MeshData patina;   // the wave, its spray and the stem
    MeshData base;     // the round foot
};

SculptureMeshes buildSculpture() {
    SculptureMeshes s;
    const glm::vec3 axis(0.0f, 0.0f, 1.0f);
    const float footH = 0.05f;
    const glm::vec3 C(0.0f, footH + 0.07f + kRingR, 0.0f);   // the ring's centre
    auto onRing = [&](float deg, float r, float z) {
        const float t = glm::radians(deg);
        return C + glm::vec3(std::cos(t) * r, std::sin(t) * r, z);
    };

    // The arc: from low on the right, over the top, to low on the left. It is
    // thickest in the middle and tapers to a point at each end, and rises a
    // little above the circle, like a body arched backwards.
    {
        const int steps = 160;
        std::vector<glm::vec3> path; std::vector<float> a, b, tw;
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            const float deg = -28.0f + 236.0f * t;
            const float body = std::pow(std::sin(kPi * t), 0.6f);
            const float r = kRingR * (1.0f + 0.07f * std::sin(kPi * t));
            path.push_back(onRing(deg, r, 0.02f * std::sin(2.0f * kPi * t)));
            a.push_back(0.005f + 0.036f * body * (1.0f + 0.15f * std::sin(3.0f * kPi * t)));
            b.push_back(0.005f + 0.050f * body);
            tw.push_back(0.35f * std::sin(2.0f * kPi * t));
        }
        s.bronze = sweep(path, a, b, tw, axis, 24);
    }

    // The wave: from under the arc's left end, down round the bottom, and up
    // into its right end. A flat ribbon that twists as it goes.
    {
        const int steps = 140;
        std::vector<glm::vec3> path; std::vector<float> a, b, tw;
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            const float deg = 196.0f + 150.0f * t;
            const float swell = std::pow(std::sin(kPi * t), 0.45f);
            const float r = kRingR * (1.0f + 0.05f * std::sin(4.0f * kPi * t + 0.6f));
            path.push_back(onRing(deg, r, 0.035f * std::sin(3.0f * kPi * t)));
            a.push_back(0.005f + 0.022f * swell);
            b.push_back(0.006f + 0.10f * swell * (0.75f + 0.25f * std::sin(5.0f * kPi * t)));
            tw.push_back(1.10f * std::sin(3.0f * kPi * t) + 0.4f * t);
        }
        s.patina = sweep(path, a, b, tw, axis, 20);
    }

    // Spray: three tendrils curling off the wave, outwards and towards the door.
    const float tendrils[][3] = {{232.0f, 0.17f, 1.0f}, {300.0f, 0.13f, -1.0f}, {326.0f, 0.20f, 1.0f}};
    for (const auto& td : tendrils) {
        const float deg = td[0], reach = td[1], side = td[2];
        const glm::vec3 p0 = onRing(deg, kRingR, 0.0f);
        const glm::vec3 p1 = onRing(deg + 6.0f * side, kRingR + reach * 0.8f, 0.04f * side);
        const glm::vec3 p2 = onRing(deg + 22.0f * side, kRingR + reach, 0.09f * side);
        const int steps = 40;
        std::vector<glm::vec3> path; std::vector<float> a, b, tw;
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            const float k = 1.0f - t;
            path.push_back(p0 * (k * k) + p1 * (2.0f * k * t) + p2 * (t * t));
            const float r = 0.002f + 0.020f * std::pow(1.0f - t, 0.8f);
            a.push_back(r);
            b.push_back(r * 1.6f);
            tw.push_back(1.5f * t * side);
        }
        s.patina.append(sweep(path, a, b, tw, axis, 12), glm::mat4(1.0f));
    }

    // The stem from the foot to the bottom of the ring, and the foot itself.
    s.patina.append(build::buildCylinder(glm::vec3(0.0f, footH, 0.0f), 0.028f, 0.09f, 20),
                    glm::mat4(1.0f));
    s.base = refine(build::buildCylinder(glm::vec3(0.0f), 0.30f, footH, 48), 0.08f);
    return s;
}

class RoomFour final : public Room {
public:
    std::vector<Piece> build(const RoomContext& ctx) override {
        std::vector<Piece> pieces;
        solid_.clear();
        swaying_.clear();
        base_.clear();
        if (!ctx.me) return pieces;

        const Rect& r = ctx.me->rect;
        const float halfWall = ctx.plan ? ctx.plan->wallThickness * 0.5f : cfg::kWallThickness * 0.5f;
        const float wallH    = ctx.plan ? ctx.plan->wallHeight : cfg::kWallHeight;
        const float apex     = ctx.plan ? ctx.plan->vaultApex : cfg::kVaultApex;
        const float x0 = r.x0 + halfWall, x1 = r.x1 - halfWall;
        const float y0 = r.y0 + halfWall, y1 = r.y1 - halfWall;

        auto add = [&](MeshData mesh, const gfx::Material& mat, const std::string& note,
                       const glm::mat4& transform = glm::mat4(1.0f)) {
            Piece p;
            p.mesh = std::move(mesh);
            p.material = mat;
            p.transform = transform;
            p.note = note;
            pieces.push_back(std::move(p));
            return pieces.size() - 1;
        };
        auto gouraud = [](const gfx::Texture* tex, glm::vec3 tint) {
            gfx::Material m;
            m.albedo  = tex;
            m.tint    = tint;
            m.shading = cfg::Shading::Gouraud;
            return m;
        };

        // ---- 1. the nebula walls and the night sky ----------------------
        {
            const NebulaField field = nebulaField();
            const gfx::Texture* wallTex = ctx.canvas("gallery4:nebula-wall", drawNebula(field, kWallNebula, 260, 168661u));
            const gfx::Texture* skyTex  = ctx.canvas("gallery4:nebula-sky", drawNebula(field, kSkyNebula, 700, 4201u));

            MeshData lining, skirting;
            const Door& d = ctx.me->door;   // north, with the doorway
            struct Run { Vec2 a, b, n; };
            const Run runs[] = {{{x0, y0}, {x1, y0}, {0.0f, 1.0f}},     // south, facing the door
                                {{x0, y0}, {x0, y1}, {1.0f, 0.0f}},     // west
                                {{x1, y0}, {x1, y1}, {-1.0f, 0.0f}},    // east
                                {{x0, y1}, {d.a, y1}, {0.0f, -1.0f}},   // north, either side of the door
                                {{d.b, y1}, {x1, y1}, {0.0f, -1.0f}}};
            for (const Run& run : runs) {
                wallGrid(run.a, run.b, run.n, 0.0f, wallH, lining);
                // The skirting: a dark box along the foot of the wall.
                const glm::vec3 in(run.n.x, 0.0f, -run.n.y);
                const glm::vec3 pa = w3(run.a.x, 0.0f, run.a.y), pb = w3(run.b.x, 0.0f, run.b.y);
                const glm::vec3 mid = (pa + pb) * 0.5f + in * (kLiningOut + kSkirtingD * 0.5f) +
                                      glm::vec3(0.0f, kSkirtingH * 0.5f, 0.0f);
                const glm::vec3 size = glm::abs(pb - pa) + glm::abs(in) * kSkirtingD +
                                       glm::vec3(0.0f, kSkirtingH, 0.0f);
                skirting.append(refine(build::buildBox(mid, size), kWallCell), glm::mat4(1.0f));
            }
            wallGrid({d.a, y1}, {d.b, y1}, {0.0f, -1.0f}, cfg::kDoorHeight, wallH, lining);   // over the door

            add(std::move(lining), gouraud(wallTex, glm::vec3(1.0f)), "nebula wall lining");
            add(std::move(skirting), gouraud(nullptr, kSkirtingColour), "skirting");
            if (ctx.plan)
                add(skyVaults(*ctx.me, wallH, apex, ctx.plan->wallThickness),
                    gouraud(skyTex, glm::vec3(1.0f)), "night sky vaults");
        }

        // ---- 2. the paintings -------------------------------------------
        for (const Work& wk : works()) {
            const gfx::Texture* img = ctx.texture(wk.file);
            if (!img) continue;
            const float aspect = img->height() > 0
                                     ? static_cast<float>(img->width()) / static_cast<float>(img->height())
                                     : 1.0f;
            const float h = wk.height, w = h * aspect;

            // A hang point on the lining, centred on the work.
            HangPoint hang;
            if (wk.wall == Wall::South) {
                const float y = y0 + kLiningOut;
                hang.span = {{wk.at - w * 0.5f, y}, {wk.at + w * 0.5f, y}};
                hang.inwardNormal = {0.0f, 1.0f};
            } else {
                const bool west = wk.wall == Wall::West;
                const float x = west ? x0 + kLiningOut : x1 - kLiningOut;
                hang.span = {{x, wk.at - w * 0.5f}, {x, wk.at + w * 0.5f}};
                hang.inwardNormal = {west ? 1.0f : -1.0f, 0.0f};
            }
            build::FramedWork framed = build::buildFramedWork(hang, w, h, kWorkCentreY);

            gfx::Material canvas = gouraud(img, glm::vec3(1.0f));
            if (wk.centrepiece) canvas.emissive = kStardustGlow;
            add(refine(framed.canvas, kCanvasCell), canvas, wk.text.title);

            add(refine(framed.frame, kCanvasCell * 2.0f),
                gouraud(ctx.texture("materials/gilt.jpg"), kFrameTint), wk.text.title + " frame");

            // The label, low on the right of the frame, like a museum card.
            const glm::vec3 n(hang.inwardNormal.x, 0.0f, -hang.inwardNormal.y);
            const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), n));
            const raster::Canvas card = label::draw(wk.text, kLabelPixels, kLabelFont);
            const glm::vec2 size = label::fit(card, kLabelW, kLabelMaxH);
            const Vec2 mid{(hang.span.a.x + hang.span.b.x) * 0.5f, (hang.span.a.y + hang.span.b.y) * 0.5f};
            const glm::vec3 centre = w3(mid.x, kLabelCentreY, mid.y) +
                                     right * (w * 0.5f + cfg::kFrameWidth + kLabelGap + size.x * 0.5f) +
                                     n * 0.004f;
            Piece plate;
            plate.mesh     = label::quad(centre, right, n, size);
            plate.material = label::material(ctx.canvas(std::string("gallery4:label:") + wk.file, card));
            plate.note     = wk.text.title + " label";
            pieces.push_back(std::move(plate));
        }

        // ---- 3. the sculpture -------------------------------------------
        // In line with Stardust, so its ring frames the painting from the door.
        {
            const Vec2 at{28.0f, 4.6f};
            const glm::vec3 floorCentre = w3(at.x, 0.0f, at.y);
            gfx::Material plinth = gouraud(ctx.texture("materials/plaster.jpg"), kPlinthColour);
            add(refine(build::buildPlinth(floorCentre, kPlinthW, kPlinthW, kPlinthH), 0.12f), plinth, "plinth");

            // Its label on the plinth's face towards the door (north is -z).
            {
                const glm::vec3 front(0.0f, 0.0f, -1.0f);
                const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), front));
                const raster::Canvas card = label::draw(sculptureLabel(), kLabelPixels, kLabelFont);
                const glm::vec2 size = label::fit(card, kPlinthW - 0.16f, kPlinthH - 0.3f);
                const glm::vec3 centre = floorCentre + front * (kPlinthW * 0.5f + 0.004f) +
                                         glm::vec3(0.0f, kPlinthH * 0.62f, 0.0f);
                Piece plate;
                plate.mesh     = label::quad(centre, right, front, size);
                plate.material = label::material(ctx.canvas("gallery4:label:sculpture", card));
                plate.note     = "sculpture label";
                pieces.push_back(std::move(plate));
            }

            top_ = floorCentre + glm::vec3(0.0f, kPlinthH, 0.0f);
            SculptureMeshes sc = buildSculpture();
            const glm::mat4 place = sculptureTransform();
            swaying_.push_back(add(std::move(sc.base), gouraud(nullptr, kDarkBronze), "sculpture: foot", place));
            swaying_.push_back(add(std::move(sc.patina), gouraud(nullptr, kPatina), "sculpture: wave", place));
            swaying_.push_back(add(std::move(sc.bronze), gouraud(nullptr, kBronze), "sculpture: arc", place));

            solid_.push_back(Footprint{at, Vec2{kPlinthW * 0.5f + kSolidMargin, kPlinthW * 0.5f + kSolidMargin}});
        }

        for (const Piece& p : pieces) base_.push_back(p.transform);
        return pieces;
    }

    bool update(float dt) override {
        time_ += dt;
        if (time_ > kSwayPeriod * 1000.0f) time_ -= kSwayPeriod * 1000.0f;
        return !swaying_.empty();
    }

    void transforms(std::vector<glm::mat4>& out) override {
        out = base_;
        const glm::mat4 t = sculptureTransform();
        for (std::size_t k : swaying_)
            if (k < out.size()) out[k] = t;
    }

    std::vector<Footprint> footprints() const override { return solid_; }

    std::string techniqueNotes() const override {
        return
            "Gallery Four: Magic Realism (Erykah Diang'a)\n"
            "  Gouraud shading everywhere: the lamp is worked out per vertex and\n"
            "    blended across each triangle. A wall of two triangles would get the\n"
            "    lamp only at its corners, so walls, vaults, canvases and frames are\n"
            "    cut into small triangles (a grid, or by splitting each triangle in four\n"
            "    until no edge is longer than 10 cm).\n"
            "  Five paintings by Rob Gonsalves. Stardust is the centrepiece, larger, in\n"
            "    the middle of the far wall, and glows a little from within.\n"
            "  A nebula drawn by our own code: value noise on a wrapping lattice, five\n"
            "    octaves of fractal Brownian motion, bent by a second noise (domain\n"
            "    warping); faint on the walls, fuller on the vaults as a night sky.\n"
            "  The vaults are rebuilt from the plan as quadratic Bezier arches, with the\n"
            "    light slot left open, set just inside the plain ones.\n"
            "  The sculpture, after Liang Binbin's Heart of the Horizon: elliptical\n"
            "    tubes swept along a circle, tapering and twisting, so the wave reads as\n"
            "    a ribbon; spray tendrils on quadratic Bezier curves. It sways slowly\n"
            "    about its vertical axis, so Gouraud highlights slide over it.\n"
            "  The plinth is a solid footprint.\n";
    }

private:
    glm::mat4 sculptureTransform() const {
        const float deg = kSwayDeg * std::sin(2.0f * kPi * time_ / kSwayPeriod);
        return glm::translate(glm::mat4(1.0f), top_) *
               glm::rotate(glm::mat4(1.0f), glm::radians(deg), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    std::vector<Footprint>   solid_;
    std::vector<std::size_t> swaying_;
    std::vector<glm::mat4>   base_;
    glm::vec3                top_{0.0f};
    float                    time_ = 0.0f;
};

} // namespace

RoomPtr makeRoomFour() { return std::make_unique<RoomFour>(); }

} // namespace lumen
