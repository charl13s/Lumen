#include "Plan.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Json.h"

namespace lumen {
namespace {

using json::Value;

float num(const Value& v) { return static_cast<float>(v.number()); }

Vec2 toVec2(const Value& j) {
    if (!j.isArray() || j.size() < 2) throw std::runtime_error("expected a [x, y] pair");
    return Vec2{num(j[0]), num(j[1])};
}

Segment toSegment(const Value& j) {
    if (!j.isArray() || j.size() < 2) throw std::runtime_error("expected a [[x,y],[x,y]] segment");
    return Segment{toVec2(j[0]), toVec2(j[1])};
}

Rect toRect(const Value& j) {
    if (!j.isArray() || j.size() < 4) throw std::runtime_error("expected a [x0,y0,x1,y1] rect");
    Rect r{num(j[0]), num(j[1]), num(j[2]), num(j[3])};
    if (r.x1 < r.x0) std::swap(r.x0, r.x1);
    if (r.y1 < r.y0) std::swap(r.y0, r.y1);
    return r;
}

Side toSide(const std::string& s) {
    if (s == "N") return Side::North;
    if (s == "S") return Side::South;
    if (s == "E") return Side::East;
    if (s == "W") return Side::West;
    throw std::runtime_error("unknown door side '" + s + "'");
}

// The inward normal of a hang point: from the wall it sits on, toward the room.
Vec2 inwardNormalFor(const Segment& span, const Rect& room) {
    Vec2 mid{(span.a.x + span.b.x) * 0.5f, (span.a.y + span.b.y) * 0.5f};
    Vec2 c = room.centre();
    if (span.horizontal()) return Vec2{0.0f, (c.y > mid.y) ? 1.0f : -1.0f};
    return Vec2{(c.x > mid.x) ? 1.0f : -1.0f, 0.0f};
}

Plan parsePlan(const Value& j) {
    Plan p;
    p.wallThickness = static_cast<float>(j.numberOr("wall_thickness", 0.45));
    p.wallHeight    = static_cast<float>(j.numberOr("wall_height", 4.2));
    p.vaultApex     = static_cast<float>(j.numberOr("vault_apex_height", 6.0));

    const Value& b = j.at("bounds");
    p.bounds = Rect{num(b.at("x0")), num(b.at("y0")), num(b.at("x1")), num(b.at("y1"))};

    p.atriumRect    = toRect(j.at("atrium").at("rect"));
    p.vestibuleRect = toRect(j.at("vestibule").at("rect"));
    p.corridorOuter = toRect(j.at("corridor").at("outer"));
    p.corridorInner = toRect(j.at("corridor").at("inner"));

    const Value& oc = j.at("atrium").at("oculus");
    p.oculusCentre = toVec2(oc.at("centre"));
    p.oculusRadius = num(oc.at("radius"));

    for (const Value& n : j.at("niches").array())
        p.niches.push_back(toRect(n.at("rect")));

    for (const Value& r : j.at("rooms").array()) {
        RoomPlan rp;
        rp.id    = r.at("id").string();
        rp.name  = r.at("name").string();
        rp.owner = r.contains("owner") ? r.at("owner").string() : std::string{};
        rp.rect  = toRect(r.at("rect"));

        const Value& d = r.at("door");
        rp.door.side = toSide(d.at("side").string());
        rp.door.a    = num(d.at("a"));
        rp.door.b    = num(d.at("b"));

        for (const Value& v : r.at("vaults").array())
            rp.vaults.push_back(toSegment(v));

        for (const Value& h : r.at("hangs").array()) {
            HangPoint hp;
            hp.span         = toSegment(h);
            hp.inwardNormal = inwardNormalFor(hp.span, rp.rect);
            rp.hangs.push_back(hp);
        }
        p.rooms.push_back(std::move(rp));
    }

    for (const Value& w : j.at("walls").array())
        p.walls.push_back(toSegment(w));

    for (const Value& r : j.at("route").array())
        p.route.push_back(toVec2(r));

    // Every doorway: the rooms, the atrium and the vestibule.
    for (const RoomPlan& r : p.rooms) p.doorways.push_back(r.door.segment(r.rect));
    auto extraDoor = [&](const Value& d, const Rect& rect) {
        Door door;
        door.side = toSide(d[0].string());
        door.a    = num(d[1]);
        door.b    = num(d[2]);
        p.doorways.push_back(door.segment(rect));
    };
    if (j.at("atrium").contains("door"))    extraDoor(j.at("atrium").at("door"), p.atriumRect);
    if (j.at("vestibule").contains("door")) extraDoor(j.at("vestibule").at("door"), p.vestibuleRect);

    if (p.rooms.empty()) throw std::runtime_error("plan.json contains no rooms");
    if (p.walls.empty()) throw std::runtime_error("plan.json contains no walls");
    return p;
}

} // namespace

// ---- small geometry --------------------------------------------------------

float Segment::length() const {
    const float dx = b.x - a.x, dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool Segment::horizontal() const {
    return std::fabs(b.y - a.y) < 1e-4f;
}

bool Rect::contains(Vec2 p) const {
    return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1;
}

bool Rect::contains(Vec2 p, float margin) const {
    return p.x >= x0 + margin && p.x <= x1 - margin &&
           p.y >= y0 + margin && p.y <= y1 - margin;
}

Vec2 Rect::centre() const {
    return Vec2{(x0 + x1) * 0.5f, (y0 + y1) * 0.5f};
}

Segment Door::segment(const Rect& r) const {
    switch (side) {
        case Side::North: return Segment{{a, r.y1}, {b, r.y1}};
        case Side::South: return Segment{{a, r.y0}, {b, r.y0}};
        case Side::West:  return Segment{{r.x0, a}, {r.x0, b}};
        case Side::East:  return Segment{{r.x1, a}, {r.x1, b}};
    }
    return Segment{};
}

Vec2 Door::centre(const Rect& r) const {
    const Segment s = segment(r);
    return Vec2{(s.a.x + s.b.x) * 0.5f, (s.a.y + s.b.y) * 0.5f};
}

// ---- plan queries ----------------------------------------------------------

Vec2 Plan::spawn() const {
    // Just inside the vestibule door, in the west corridor leg.
    return Vec2{corridorOuter.x0 + 1.4f, (vestibuleRect.y0 + vestibuleRect.y1) * 0.5f};
}

float Plan::spawnHeadingDeg() const {
    return 90.0f;  // facing east, into the building
}

const RoomPlan* Plan::roomAt(Vec2 p, float margin) const {
    for (const RoomPlan& r : rooms)
        if (r.rect.contains(p, margin)) return &r;
    return nullptr;
}

// ---- loading ---------------------------------------------------------------

Plan Plan::load(const std::string& path) {
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error(
            "Could not open '" + path + "'.\n"
            "Run the program from the project folder, or pass the assets folder as an argument.");

    std::stringstream ss;
    ss << in.rdbuf();

    json::Value j;
    try {
        j = json::Value::parse(ss.str());
    } catch (const std::exception& e) {
        throw std::runtime_error("plan.json is " + std::string(e.what()));
    }

    try {
        return parsePlan(j);
    } catch (const std::exception& e) {
        throw std::runtime_error("'" + path + "' is missing a field or has a wrong value: " +
                                 std::string(e.what()));
    }
}

} // namespace lumen
