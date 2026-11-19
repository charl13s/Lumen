#include "Builders.h"

#include <algorithm>
#include <cmath>

#include "Curves.h"

#include <array>
#include <utility>

namespace lumen::build {
namespace {

constexpr float kPi = 3.14159265358979f;

// The frame moulding profile, from the canvas edge (w = 0) to the outer edge
// (w = kFrameWidth). A lip, a cubic Bezier for the rounded moulding, then a
// flat outer side back to the canvas plane.
std::vector<curves::P2> frameProfile() {
    const float fw = cfg::kFrameWidth;
    const float fd = cfg::kFrameDepth;
    std::vector<curves::P2> out{{0.0f, 0.0f}};
    const std::vector<curves::P2> control{
        {0.0f, 0.35f * fd}, {0.10f * fw, 1.25f * fd}, {0.55f * fw, 1.05f * fd}, {fw, 0.45f * fd}};
    for (const curves::P2& p : curves::sampleBezier(control, 10)) out.push_back(p);
    out.push_back({fw, 0.0f});
    return out;
}

glm::vec3 w3(float planX, float y, float planY) {
    return glm::vec3(worldX(planX), y, worldZ(planY));
}

// A rectangle lying flat at height y, given plan-space corners.
void addFloorQuad(gfx::MeshData& m, const Rect& r, float y, float tiling) {
    const float u = r.width() / tiling;
    const float v = r.height() / tiling;
    m.addQuad(w3(r.x0, y, r.y0), w3(r.x1, y, r.y0),
              w3(r.x1, y, r.y1), w3(r.x0, y, r.y1), u, v);
}

} // namespace

// ---- walls -----------------------------------------------------------------

namespace {

// One wall block from height y0 to y1: both faces, the top, and optionally the
// end caps and the underside (a lintel needs an underside, a wall needs caps).
void addWallBlock(gfx::MeshData& m, const Segment& s, float half,
                  float y0, float h, bool caps, bool underside) {
    // Unit direction along the wall, and its perpendicular, in plan space.
    const float dx = s.b.x - s.a.x;
    const float dy = s.b.y - s.a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-5f) return;

    const float ux = dx / len, uy = dy / len;
    const float px = -uy * half, py = ux * half;   // perpendicular offset

    // Four plan-space corners of the wall's footprint.
    const float ax0 = s.a.x + px, ay0 = s.a.y + py;   // start, one side
    const float ax1 = s.a.x - px, ay1 = s.a.y - py;   // start, other side
    const float bx0 = s.b.x + px, by0 = s.b.y + py;
    const float bx1 = s.b.x - px, by1 = s.b.y - py;

    const float uRepeat = len / 2.0f;   // one texture tile per 2 m
    const float vRepeat = (h - y0) / 2.0f;

    // Winding order matters. addQuad's normal is cross(b - a, d - a), which
    // decides both the culled side and the lighting. Wound backwards, a wall
    // is culled and lit black. A face on the +y (north, world -Z) side must
    // face -Z. Checked in tests/test_core.cpp.
    m.addQuad(w3(ax0, y0, ay0), w3(ax0, h, ay0),
              w3(bx0, h, by0),  w3(bx0, y0, by0), uRepeat, vRepeat);
    m.addQuad(w3(bx1, y0, by1), w3(bx1, h, by1),
              w3(ax1, h, ay1),  w3(ax1, y0, ay1), uRepeat, vRepeat);

    // End caps, so walls at doorways have a visible edge.
    if (caps) {
        m.addQuad(w3(ax1, y0, ay1), w3(ax1, h, ay1),
                  w3(ax0, h, ay0),  w3(ax0, y0, ay0), 0.25f, vRepeat);
        m.addQuad(w3(bx0, y0, by0), w3(bx0, h, by0),
                  w3(bx1, h, by1),  w3(bx1, y0, by1), 0.25f, vRepeat);
    }

    // Top.
    m.addQuad(w3(ax0, h, ay0), w3(ax1, h, ay1),
              w3(bx1, h, by1),   w3(bx0, h, by0), uRepeat, 0.25f);

    // Underside, facing down.
    if (underside)
        m.addQuad(w3(ax0, y0, ay0), w3(bx0, y0, by0),
                  w3(bx1, y0, by1), w3(ax1, y0, ay1), uRepeat, 0.25f);
}

} // namespace

gfx::MeshData extrudeWalls(const Plan& plan) {
    gfx::MeshData m;
    const float h    = plan.wallHeight;
    const float half = plan.wallThickness * 0.5f;

    for (const Segment& s : plan.walls)
        addWallBlock(m, s, half, 0.0f, h, true, false);

    // A lintel over every doorway, so doors are openings in the wall rather
    // than gaps that run up to the ceiling.
    for (const Segment& d : plan.doorways)
        addWallBlock(m, d, half, cfg::kDoorHeight, h, false, true);
    for (const Segment& d : plan.nicheOpenings)
        addWallBlock(m, d, half, cfg::kDoorHeight, h, false, true);
    return m;
}

void splitWallsByRoom(const gfx::MeshData& walls, const Plan& plan,
                      gfx::MeshData& inRooms, gfx::MeshData& shared) {
    for (std::size_t i = 0; i + 2 < walls.indices.size(); i += 3) {
        const gfx::Vertex& a = walls.vertices[walls.indices[i + 0]];
        const gfx::Vertex& b = walls.vertices[walls.indices[i + 1]];
        const gfx::Vertex& c = walls.vertices[walls.indices[i + 2]];

        // Step a little off the face, along its normal, and see where we are.
        const glm::vec3 n = glm::normalize(glm::cross(b.position - a.position, c.position - a.position));
        const glm::vec3 p = (a.position + b.position + c.position) / 3.0f + n * 0.05f;
        const Vec2 at{p.x, -p.z};   // back to plan space

        bool room = false;
        for (const RoomPlan& r : plan.rooms) room = room || r.rect.contains(at);
        (room ? inRooms : shared).addTriangle(a, b, c);
    }
}

void projectUVs(gfx::MeshData& mesh, glm::vec2 tile) {
    for (gfx::Vertex& v : mesh.vertices) {
        const glm::vec3& p = v.position;
        glm::vec2 uv;
        if (std::fabs(v.normal.y) > 0.5f)
            uv = {p.x, p.z};   // floor or ceiling
        else
            uv = {std::fabs(v.normal.x) > std::fabs(v.normal.z) ? p.z : p.x, p.y};   // a wall
        v.uv = uv / tile;
    }
}

gfx::MeshData buildFloors(const Plan& plan) {
    gfx::MeshData m;
    addFloorQuad(m, plan.vestibuleRect, 0.0f, 1.0f);
    addFloorQuad(m, plan.atriumRect,    0.0f, 1.0f);

    // The corridor ring: outer rect minus the atrium, as four strips.
    const Rect& o = plan.corridorOuter;
    const Rect& i = plan.corridorInner;
    addFloorQuad(m, Rect{o.x0, i.y1, o.x1, o.y1}, 0.0f, 1.0f);   // north leg
    addFloorQuad(m, Rect{o.x0, o.y0, o.x1, i.y0}, 0.0f, 1.0f);   // south leg
    addFloorQuad(m, Rect{o.x0, i.y0, i.x0, i.y1}, 0.0f, 1.0f);   // west leg
    addFloorQuad(m, Rect{i.x1, i.y0, o.x1, i.y1}, 0.0f, 1.0f);   // east leg

    for (const Rect& n : plan.niches) addFloorQuad(m, n, 0.0f, 1.0f);
    for (const RoomPlan& r : plan.rooms) addFloorQuad(m, r.rect, 0.0f, 1.0f);
    return m;
}

// ---- ceilings --------------------------------------------------------------

namespace {

// A rectangle at height y, facing down.
void addCeilingQuad(gfx::MeshData& m, const Rect& r, float y, float tiling) {
    const float u = r.width() / tiling;
    const float v = r.height() / tiling;
    m.addQuad(w3(r.x0, y, r.y0), w3(r.x0, y, r.y1),
              w3(r.x1, y, r.y1), w3(r.x1, y, r.y0), v, u);
}

} // namespace

gfx::MeshData buildCeilings(const Plan& plan) {
    gfx::MeshData m;
    const float h = plan.wallHeight;
    const Rect& o = plan.corridorOuter;
    const Rect& i = plan.corridorInner;
    addCeilingQuad(m, Rect{o.x0, i.y1, o.x1, o.y1}, h, 2.0f);   // north leg
    addCeilingQuad(m, Rect{o.x0, o.y0, o.x1, i.y0}, h, 2.0f);   // south leg
    addCeilingQuad(m, Rect{o.x0, i.y0, i.x0, i.y1}, h, 2.0f);   // west leg
    addCeilingQuad(m, Rect{i.x1, i.y0, o.x1, i.y1}, h, 2.0f);   // east leg
    addCeilingQuad(m, plan.vestibuleRect, h, 2.0f);
    for (const Rect& n : plan.niches) addCeilingQuad(m, n, h, 2.0f);
    return m;
}

gfx::MeshData buildOculusCeiling(const Plan& plan, float rimHeight, int segments) {
    gfx::MeshData m;
    const Rect& r = plan.atriumRect;
    const Vec2  c = plan.oculusCentre;
    const float rad = plan.oculusRadius;
    const float h = plan.wallHeight;
    if (segments < 8) segments = 8;

    // Angles round the circle, plus the four corners, so the outer edge follows
    // the rectangle exactly. Each wedge between two angles joins the circle to
    // the rectangle with two triangles.
    std::vector<float> angles;
    for (int k = 0; k < segments; ++k) angles.push_back(2.0f * kPi * k / segments);
    for (const Vec2 corner : {Vec2{r.x1, r.y1}, Vec2{r.x0, r.y1}, Vec2{r.x0, r.y0}, Vec2{r.x1, r.y0}}) {
        float a = std::atan2(corner.y - c.y, corner.x - c.x);
        if (a < 0.0f) a += 2.0f * kPi;
        angles.push_back(a);
    }
    std::sort(angles.begin(), angles.end());

    // Where a ray from the centre at angle a leaves the rectangle.
    auto edge = [&](float a) {
        const float dx = std::cos(a), dy = std::sin(a);
        float t = 1e9f;
        if (dx >  1e-6f) t = std::min(t, (r.x1 - c.x) / dx);
        if (dx < -1e-6f) t = std::min(t, (r.x0 - c.x) / dx);
        if (dy >  1e-6f) t = std::min(t, (r.y1 - c.y) / dy);
        if (dy < -1e-6f) t = std::min(t, (r.y0 - c.y) / dy);
        return Vec2{c.x + dx * t, c.y + dy * t};
    };
    auto ring = [&](float a) { return Vec2{c.x + std::cos(a) * rad, c.y + std::sin(a) * rad}; };

    const glm::vec3 down(0.0f, -1.0f, 0.0f);
    auto vert = [&](Vec2 p) {
        // Texture by plan position, one tile per 2 m, like the walls.
        return gfx::Vertex{w3(p.x, h, p.y), down, {p.x * 0.5f, p.y * 0.5f}, {1.0f, 0.0f, 0.0f}};
    };
    for (std::size_t k = 0; k < angles.size(); ++k) {
        const float a0 = angles[k];
        const float a1 = (k + 1 < angles.size()) ? angles[k + 1] : angles[0] + 2.0f * kPi;
        if (a1 - a0 < 1e-5f) continue;
        const gfx::Vertex i0 = vert(ring(a0)), i1 = vert(ring(a1));
        const gfx::Vertex o0 = vert(edge(a0)), o1 = vert(edge(a1));
        // Counter-clockwise in plan is clockwise seen from below, so wind i0, o1, o0.
        m.addTriangle(i0, o1, o0);
        m.addTriangle(i0, i1, o1);
    }

    // The drum lining the opening, facing in towards the light.
    for (int k = 0; k < segments; ++k) {
        const float a0 = 2.0f * kPi * k / segments;
        const float a1 = 2.0f * kPi * (k + 1) / segments;
        const Vec2 p0 = ring(a0), p1 = ring(a1);
        m.addQuad(w3(p0.x, h, p0.y), w3(p0.x, h + rimHeight, p0.y),
                  w3(p1.x, h + rimHeight, p1.y), w3(p1.x, h, p1.y),
                  1.0f, rimHeight * 0.5f);
    }
    return m;
}

gfx::MeshData buildDisc(const glm::vec3& centre, float radius, bool facingDown, int segments) {
    gfx::MeshData m;
    if (segments < 3) segments = 3;
    const glm::vec3 n(0.0f, facingDown ? -1.0f : 1.0f, 0.0f);
    const gfx::Vertex mid{centre, n, {0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}};
    for (int k = 0; k < segments; ++k) {
        const float a0 = 2.0f * kPi * k / segments;
        const float a1 = 2.0f * kPi * (k + 1) / segments;
        const gfx::Vertex v0{centre + glm::vec3(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius), n,
                             {0.5f + 0.5f * std::cos(a0), 0.5f + 0.5f * std::sin(a0)}, {1.0f, 0.0f, 0.0f}};
        const gfx::Vertex v1{centre + glm::vec3(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius), n,
                             {0.5f + 0.5f * std::cos(a1), 0.5f + 0.5f * std::sin(a1)}, {1.0f, 0.0f, 0.0f}};
        // World angle runs from +X towards +Z, which is clockwise seen from above.
        if (facingDown) m.addTriangle(mid, v0, v1);
        else            m.addTriangle(mid, v1, v0);
    }
    return m;
}

// ---- vaults ----------------------------------------------------------------

gfx::MeshData buildVault(const Segment& crown, float span, float apexHeight,
                         float springHeight, int segments, bool endWalls) {
    gfx::MeshData m;
    if (segments < 4) segments = 4;

    // Direction along the crown, and the perpendicular the arch sweeps across.
    const float dx = crown.b.x - crown.a.x;
    const float dy = crown.b.y - crown.a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-5f) return m;

    const float ux = dx / len, uy = dy / len;
    const float nx = -uy, ny = ux;

    const float radius = span * 0.5f;
    const float rise   = apexHeight - springHeight;

    // The arch is a quadratic Bezier, flatter than a semicircle like Kahn's vaults.
    //   P0 (-radius, 0)             left wall top
    //   P1 (0, rise * kControlLift) control point
    //   P2 (+radius, 0)             right wall top
    // The curve only reaches half the control height at t = 0.5, so the control
    // point is lifted by 2 to put the top of the arch at `rise`.
    constexpr float kControlLift = 2.0f;

    const curves::P2 c0{-radius, 0.0f};
    const curves::P2 c1{0.0f, rise * kControlLift};
    const curves::P2 c2{radius, 0.0f};

    // Leave a gap at the top for the light slot.
    const float slotHalfAngle = 0.08f;   // in t, either side of the crown

    auto pointAt = [&](float t, float along) {
        const curves::P2 p = curves::bezierQuadratic(c0, c1, c2, t);
        const float hgt   = springHeight + p.y;
        const float planX = crown.a.x + ux * along + nx * p.x;
        const float planY = crown.a.y + uy * along + ny * p.x;
        return w3(planX, hgt, planY);
    };

    const int alongSteps = 1;   // the crown is straight, so one span is enough
    for (int a = 0; a < alongSteps; ++a) {
        const float a0 = len * static_cast<float>(a) / alongSteps;
        const float a1 = len * static_cast<float>(a + 1) / alongSteps;

        for (int i = 0; i < segments; ++i) {
            float t0 = static_cast<float>(i) / segments;
            float t1 = static_cast<float>(i + 1) / segments;

            // Skip the strip at the top: that is the slot.
            const float mid = (t0 + t1) * 0.5f;
            if (std::fabs(mid - 0.5f) < slotHalfAngle) continue;

            m.addQuad(pointAt(t0, a0), pointAt(t1, a0),
                      pointAt(t1, a1), pointAt(t0, a1),
                      1.0f, 1.0f);
        }
    }

    // End walls: fill the arch at each end with a fan of triangles from the
    // middle of the spring line, facing into the vault.
    if (endWalls) {
        for (int end = 0; end < 2; ++end) {
            const float along = (end == 0) ? 0.0f : len;
            const float sgn   = (end == 0) ? 1.0f : -1.0f;
            const glm::vec3 inward(worldX(ux) * sgn, 0.0f, worldZ(uy) * sgn);
            const glm::vec3 base = w3(crown.a.x + ux * along, springHeight,
                                      crown.a.y + uy * along);
            const glm::vec3 tangent = glm::normalize(glm::cross(glm::vec3(0, 1, 0), inward));

            for (int i = 0; i < segments; ++i) {
                const float t0 = static_cast<float>(i) / segments;
                const float t1 = static_cast<float>(i + 1) / segments;
                glm::vec3 p0 = pointAt(t0, along);
                glm::vec3 p1 = pointAt(t1, along);
                // Wind it so the front face looks into the vault.
                if (glm::dot(glm::cross(p0 - base, p1 - base), inward) < 0.0f) std::swap(p0, p1);
                m.addTriangle(gfx::Vertex{base, inward, {0.5f, 0.0f}, tangent},
                              gfx::Vertex{p0,   inward, {t0, 1.0f},   tangent},
                              gfx::Vertex{p1,   inward, {t1, 1.0f},   tangent});
            }
        }
    }
    return m;
}

gfx::MeshData buildRoomVaults(const RoomPlan& room, float wallHeight, float apex,
                              float wallThickness) {
    gfx::MeshData m;
    if (room.vaults.empty()) return m;

    // Each vault spans its share of the room's width, measured across the crowns.
    const bool alongY = !room.vaults.front().horizontal();
    const float roomSpan = alongY ? room.rect.width() : room.rect.height();
    const float span = roomSpan / static_cast<float>(room.vaults.size());
    const float half = wallThickness * 0.5f;

    for (Segment crown : room.vaults) {
        // Run the vault right up to the inside face of the end walls, so its
        // closed ends sit on the walls with no gap over the doorway.
        if (alongY) {
            crown.a.y = room.rect.y0 + half;
            crown.b.y = room.rect.y1 - half;
        } else {
            crown.a.x = room.rect.x0 + half;
            crown.b.x = room.rect.x1 - half;
        }
        gfx::MeshData v = buildVault(crown, span, apex, wallHeight);
        m.append(v, glm::mat4(1.0f));
    }
    return m;
}

// ---- framed works ----------------------------------------------------------

FramedWork buildFramedWork(const HangPoint& hang, float widthM, float heightM,
                           float centreHeight) {
    FramedWork out;

    // The wall run the work sits on, and the direction into the room.
    const Vec2 mid{(hang.span.a.x + hang.span.b.x) * 0.5f,
                   (hang.span.a.y + hang.span.b.y) * 0.5f};
    const glm::vec3 n(hang.inwardNormal.x, 0.0f, -hang.inwardNormal.y);   // into room

    // Along-wall unit vector, perpendicular to the inward normal.
    const glm::vec3 along = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), n));
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    const glm::vec3 wallPoint = w3(mid.x, centreHeight, mid.y);
    const glm::vec3 c = wallPoint + n * cfg::kWorkOffset;
    out.centre = c;

    const float hw = widthM * 0.5f;
    const float hh = heightM * 0.5f;

    // The canvas. Wound so its normal faces into the room.
    out.canvas.addQuad(c - along * hw - up * hh,
                       c + along * hw - up * hh,
                       c + along * hw + up * hh,
                       c - along * hw + up * hh);

    // The frame: a moulding profile swept around the canvas. The profile is a
    // cubic Bezier in (w, d): w is the distance out from the canvas edge, d is
    // how far it stands out from the canvas. Each profile point becomes a
    // rectangle around the painting, and neighbouring rectangles are joined
    // with quads. Growing the rectangle by w on both axes mitres the corners.
    const std::vector<curves::P2> profile = frameProfile();

    auto ring = [&](const curves::P2& p) {
        const float x = hw + p.x, y = hh + p.x;
        const glm::vec3 off = n * p.y;
        return std::array<glm::vec3, 4>{c - along * x - up * y + off,    // bottom left
                                        c + along * x - up * y + off,    // bottom right
                                        c + along * x + up * y + off,    // top right
                                        c - along * x + up * y + off};   // top left
    };

    for (std::size_t i = 0; i + 1 < profile.size(); ++i) {
        const auto inner = ring(profile[i]);
        const auto outer = ring(profile[i + 1]);
        for (int side = 0; side < 4; ++side) {
            const int next = (side + 1) % 4;
            // Wound so the face looks away from the wall.
            out.frame.addQuad(inner[side], outer[side], outer[next], inner[next]);
        }
    }

    // The wall label, to the lower right of the painting.
    const glm::vec3 plateCentre = c + along * (hw + 0.22f) - up * (hh - 0.06f)
                                    + n * 0.004f;
    out.plate.addQuad(plateCentre - along * 0.09f - up * 0.055f,
                      plateCentre + along * 0.09f - up * 0.055f,
                      plateCentre + along * 0.09f + up * 0.055f,
                      plateCentre - along * 0.09f + up * 0.055f);
    return out;
}

// ---- primitives ------------------------------------------------------------

gfx::MeshData buildBox(const glm::vec3& centre, const glm::vec3& size) {
    gfx::MeshData m;
    const glm::vec3 h = size * 0.5f;
    const glm::vec3 c = centre;

    const glm::vec3 p000 = c + glm::vec3(-h.x, -h.y, -h.z);
    const glm::vec3 p100 = c + glm::vec3( h.x, -h.y, -h.z);
    const glm::vec3 p110 = c + glm::vec3( h.x,  h.y, -h.z);
    const glm::vec3 p010 = c + glm::vec3(-h.x,  h.y, -h.z);
    const glm::vec3 p001 = c + glm::vec3(-h.x, -h.y,  h.z);
    const glm::vec3 p101 = c + glm::vec3( h.x, -h.y,  h.z);
    const glm::vec3 p111 = c + glm::vec3( h.x,  h.y,  h.z);
    const glm::vec3 p011 = c + glm::vec3(-h.x,  h.y,  h.z);

    m.addQuad(p001, p101, p111, p011);   // +Z
    m.addQuad(p100, p000, p010, p110);   // -Z
    m.addQuad(p101, p100, p110, p111);   // +X
    m.addQuad(p000, p001, p011, p010);   // -X
    m.addQuad(p011, p111, p110, p010);   // +Y
    m.addQuad(p000, p100, p101, p001);   // -Y
    return m;
}

gfx::MeshData buildPlinth(const glm::vec3& centre, float width, float depth, float height) {
    // centre is on the floor; the box sits on top of it.
    return buildBox(centre + glm::vec3(0.0f, height * 0.5f, 0.0f),
                    glm::vec3(width, height, depth));
}

gfx::MeshData buildCylinder(const glm::vec3& base, float radius, float height,
                            int segments) {
    gfx::MeshData m;
    if (segments < 3) segments = 3;

    for (int i = 0; i < segments; ++i) {
        const float t0 = 2.0f * kPi * static_cast<float>(i) / segments;
        const float t1 = 2.0f * kPi * static_cast<float>(i + 1) / segments;

        const glm::vec3 a = base + glm::vec3(std::cos(t0) * radius, 0.0f, std::sin(t0) * radius);
        const glm::vec3 b = base + glm::vec3(std::cos(t1) * radius, 0.0f, std::sin(t1) * radius);
        const glm::vec3 c = b + glm::vec3(0.0f, height, 0.0f);
        const glm::vec3 d = a + glm::vec3(0.0f, height, 0.0f);
        m.addQuad(b, a, d, c);   // wound so the side faces outward

        // Cap, as a fan triangle.
        const glm::vec3 top = base + glm::vec3(0.0f, height, 0.0f);
        gfx::Vertex va{d, glm::vec3(0.0f, 1.0f, 0.0f), {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        gfx::Vertex vb{c, glm::vec3(0.0f, 1.0f, 0.0f), {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        gfx::Vertex vc{top, glm::vec3(0.0f, 1.0f, 0.0f), {0.5f, 1.0f}, {1.0f, 0.0f, 0.0f}};
        m.addTriangle(va, vc, vb);   // wound so the cap faces up
    }
    return m;
}

gfx::MeshData buildCone(const glm::vec3& base, float radius, float height, int segments) {
    gfx::MeshData m;
    if (segments < 3) segments = 3;
    const glm::vec3 tip = base + glm::vec3(0.0f, height, 0.0f);
    const float slope = radius / std::max(height, 1e-6f);
    for (int i = 0; i < segments; ++i) {
        const float t0 = 2.0f * kPi * static_cast<float>(i) / segments;
        const float t1 = 2.0f * kPi * static_cast<float>(i + 1) / segments;
        const glm::vec3 a = base + glm::vec3(std::cos(t0) * radius, 0.0f, std::sin(t0) * radius);
        const glm::vec3 b = base + glm::vec3(std::cos(t1) * radius, 0.0f, std::sin(t1) * radius);
        // Side normals lean up by the cone's slope, so it shades round.
        const glm::vec3 na = glm::normalize(glm::vec3(std::cos(t0), slope, std::sin(t0)));
        const glm::vec3 nb = glm::normalize(glm::vec3(std::cos(t1), slope, std::sin(t1)));
        const glm::vec3 nt = glm::normalize(na + nb);
        // Angles run from +X towards +Z, clockwise seen from above, so b comes before a.
        m.addTriangle(gfx::Vertex{b, nb, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                      gfx::Vertex{a, na, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                      gfx::Vertex{tip, nt, {0.5f, 1.0f}, {1.0f, 0.0f, 0.0f}});
        const glm::vec3 down(0.0f, -1.0f, 0.0f);
        m.addTriangle(gfx::Vertex{a, down, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                      gfx::Vertex{b, down, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                      gfx::Vertex{base, down, {0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}});
    }
    return m;
}

} // namespace lumen::build
