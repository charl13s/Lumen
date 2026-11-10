#include "Builders.h"

#include <cmath>

namespace lumen::build {
namespace {

constexpr float kPi = 3.14159265358979f;

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

gfx::MeshData extrudeWalls(const Plan& plan) {
    gfx::MeshData m;
    const float h    = plan.wallHeight;
    const float half = plan.wallThickness * 0.5f;

    for (const Segment& s : plan.walls) {
        // Unit direction along the wall, and its perpendicular, in plan space.
        const float dx = s.b.x - s.a.x;
        const float dy = s.b.y - s.a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-5f) continue;

        const float ux = dx / len, uy = dy / len;
        const float px = -uy * half, py = ux * half;   // perpendicular offset

        // Four plan-space corners of the wall's footprint.
        const float ax0 = s.a.x + px, ay0 = s.a.y + py;   // start, one side
        const float ax1 = s.a.x - px, ay1 = s.a.y - py;   // start, other side
        const float bx0 = s.b.x + px, by0 = s.b.y + py;
        const float bx1 = s.b.x - px, by1 = s.b.y - py;

        const float uRepeat = len / 2.0f;   // one texture tile per 2 m
        const float vRepeat = h / 2.0f;

        // Both faces, wound so each points outward from the wall body.
        m.addQuad(w3(ax0, 0.0f, ay0), w3(bx0, 0.0f, by0),
                  w3(bx0, h, by0),    w3(ax0, h, ay0), uRepeat, vRepeat);
        m.addQuad(w3(bx1, 0.0f, by1), w3(ax1, 0.0f, ay1),
                  w3(ax1, h, ay1),    w3(bx1, h, by1), uRepeat, vRepeat);

        // End caps, so a wall that terminates at a doorway has a visible reveal
        // rather than an infinitely thin edge.
        m.addQuad(w3(ax1, 0.0f, ay1), w3(ax0, 0.0f, ay0),
                  w3(ax0, h, ay0),    w3(ax1, h, ay1), 0.25f, vRepeat);
        m.addQuad(w3(bx0, 0.0f, by0), w3(bx1, 0.0f, by1),
                  w3(bx1, h, by1),    w3(bx0, h, by0), 0.25f, vRepeat);

        // Top.
        m.addQuad(w3(ax0, h, ay0), w3(bx0, h, by0),
                  w3(bx1, h, by1), w3(ax1, h, ay1), uRepeat, 0.25f);
    }
    return m;
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

// ---- vaults ----------------------------------------------------------------

gfx::MeshData buildVault(const Segment& crown, float span, float apexHeight,
                         float springHeight, int segments) {
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

    // Sweep a half-ellipse from one springing line to the other. The crown slot
    // is the gap we leave at the top: the arch stops short of vertical on each
    // side, and that opening is the light source.
    const float slotHalfAngle = 0.08f;   // radians either side of the crown

    auto pointAt = [&](float t, float along) {
        // t in [0, 1] sweeps from one side to the other.
        const float angle = kPi * t;
        const float off   = -std::cos(angle) * radius;
        const float hgt   = springHeight + std::sin(angle) * rise;
        const float planX = crown.a.x + ux * along + nx * off;
        const float planY = crown.a.y + uy * along + ny * off;
        return w3(planX, hgt, planY);
    };

    const int alongSteps = 1;   // the crown is straight, so one span is enough
    for (int a = 0; a < alongSteps; ++a) {
        const float a0 = len * static_cast<float>(a) / alongSteps;
        const float a1 = len * static_cast<float>(a + 1) / alongSteps;

        for (int i = 0; i < segments; ++i) {
            float t0 = static_cast<float>(i) / segments;
            float t1 = static_cast<float>(i + 1) / segments;

            // Skip the strip that straddles the crown: that gap is the slot.
            const float mid = (t0 + t1) * 0.5f;
            if (std::fabs(mid - 0.5f) * kPi < slotHalfAngle) continue;

            m.addQuad(pointAt(t0, a0), pointAt(t1, a0),
                      pointAt(t1, a1), pointAt(t0, a1),
                      1.0f, 1.0f);
        }
    }
    return m;
}

gfx::MeshData buildRoomVaults(const RoomPlan& room, float wallHeight, float apex) {
    gfx::MeshData m;
    if (room.vaults.empty()) return m;

    // Each vault spans its share of the room's width, measured across the crowns.
    const bool alongY = !room.vaults.front().horizontal();
    const float roomSpan = alongY ? room.rect.width() : room.rect.height();
    const float span = roomSpan / static_cast<float>(room.vaults.size());

    for (const Segment& crown : room.vaults) {
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

    // The moulding: four boxes around the canvas, standing proud of it. The
    // ornament lives in a normal map, not in geometry - a modelled baroque frame
    // would be tens of thousands of triangles for one painting.
    const float fw = cfg::kFrameWidth;
    const float fd = cfg::kFrameDepth;
    auto bar = [&](const glm::vec3& centre, float halfAlong, float halfUp) {
        const glm::vec3 a = centre - along * halfAlong - up * halfUp;
        const glm::vec3 b = centre + along * halfAlong - up * halfUp;
        const glm::vec3 d = centre + along * halfAlong + up * halfUp;
        const glm::vec3 e = centre - along * halfAlong + up * halfUp;
        const glm::vec3 off = n * fd;
        out.frame.addQuad(a + off, b + off, d + off, e + off);       // face
        out.frame.addQuad(a, b, b + off, a + off);                   // bottom edge
        out.frame.addQuad(d, e, e + off, d + off);                   // top edge
        out.frame.addQuad(b, d, d + off, b + off);                   // right edge
        out.frame.addQuad(e, a, a + off, e + off);                   // left edge
    };
    bar(c + up * (hh + fw * 0.5f), hw + fw, fw * 0.5f);   // top
    bar(c - up * (hh + fw * 0.5f), hw + fw, fw * 0.5f);   // bottom
    bar(c - along * (hw + fw * 0.5f), fw * 0.5f, hh);     // left
    bar(c + along * (hw + fw * 0.5f), fw * 0.5f, hh);     // right

    // The wall label, to the lower right of the work. Real galleries have these,
    // and ours carries the credit line the museum's open-access terms ask for.
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
        m.addQuad(a, b, c, d);

        // Cap, as a fan triangle.
        const glm::vec3 top = base + glm::vec3(0.0f, height, 0.0f);
        gfx::Vertex va{d, glm::vec3(0.0f, 1.0f, 0.0f), {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        gfx::Vertex vb{c, glm::vec3(0.0f, 1.0f, 0.0f), {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        gfx::Vertex vc{top, glm::vec3(0.0f, 1.0f, 0.0f), {0.5f, 1.0f}, {1.0f, 0.0f, 0.0f}};
        m.addTriangle(va, vb, vc);
    }
    return m;
}

} // namespace lumen::build
