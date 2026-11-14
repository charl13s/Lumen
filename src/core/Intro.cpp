#include "Intro.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

#include "Config.h"

namespace lumen {
namespace {

constexpr float kPi = 3.14159265358979f;

float distance(Vec2 a, Vec2 b) { return std::hypot(b.x - a.x, b.y - a.y); }

// Compass heading from a to b: 0 north (+y), 90 east (+x).
float headingTo(Vec2 a, Vec2 b) {
    float h = std::atan2(b.x - a.x, b.y - a.y) * 180.0f / kPi;
    return h < 0.0f ? h + 360.0f : h;
}

} // namespace

bool loadBearerSettings(const std::string& path, BearerSettings& out, std::string& why) {
    std::ifstream f(path);
    if (!f) { why = "'" + path + "' is missing"; return false; }
    try {
        const nlohmann::json j = nlohmann::json::parse(f);
        out.character  = j.at("character").get<std::string>();
        out.animations = j.value("animations", std::string());
        out.credit     = j.value("credit", std::string());
        out.hand       = j.value("hand", out.hand);
        out.grip       = j.value("grip", out.grip);
        out.finish     = j.value("finish", out.finish);
        out.height     = j.value("height", out.height);
        out.walkSpeed  = j.value("walk_speed", out.walkSpeed);
        out.giveHold   = j.value("give_hold", out.giveHold);
        if (j.contains("clips")) {
            const auto& c = j.at("clips");
            out.idle = c.value("idle", out.idle);
            out.walk = c.value("walk", out.walk);
            out.give = c.value("give", out.give);
            out.idleTorch = c.value("idle_torch", out.idle);
        }
    } catch (const std::exception& e) {
        why = "'" + path + "' could not be read: " + e.what();
        return false;
    }
    return true;
}

Intro::Intro(const Plan& plan, Vec2 visitor) : visitor_(visitor) {
    // The post: beside the statue, on the side towards the way in.
    const Vec2 c = plan.oculusCentre;
    const Vec2 post{c.x - cfg::kBearerPost[0], c.y - cfg::kBearerPost[1]};

    // The atrium's own doorway: the doorway that lies on the atrium's edge.
    const Rect& a = plan.atriumRect;
    Vec2 door = visitor;
    for (const Segment& d : plan.doorways) {
        const Vec2 mid{(d.a.x + d.b.x) * 0.5f, (d.a.y + d.b.y) * 0.5f};
        const bool onEdge = std::fabs(mid.x - a.x0) < 0.01f || std::fabs(mid.x - a.x1) < 0.01f ||
                            std::fabs(mid.y - a.y0) < 0.01f || std::fabs(mid.y - a.y1) < 0.01f;
        if (onEdge && a.contains(mid, -0.01f)) { door = mid; break; }
    }
    // A step inside the doorway, so the bearer walks straight through it.
    const Vec2 in = a.centre();
    const float dl = distance(door, in);
    const Vec2 inside = dl > 1e-4f ? Vec2{door.x + (in.x - door.x) / dl, door.y + (in.y - door.y) / dl} : door;

    // Stop at arm's length from the visitor, on the way to the door.
    const float vl = distance(visitor, door);
    const Vec2 meet = vl > 1e-4f
        ? Vec2{visitor.x + (door.x - visitor.x) / vl * cfg::kBearerStandOff,
               visitor.y + (door.y - visitor.y) / vl * cfg::kBearerStandOff}
        : visitor;

    path_ = {post, inside, meet};
    pos_ = post;
    postHeading_ = headingTo(post, inside);
    heading_ = postHeading_;
}

float Intro::length() const {
    float l = 0.0f;
    for (std::size_t i = 1; i < path_.size(); ++i) l += distance(path_[i - 1], path_[i]);
    return l;
}

Vec2 Intro::pointAt(float d) const {
    d = std::max(0.0f, d);
    for (std::size_t i = 1; i < path_.size(); ++i) {
        const float seg = distance(path_[i - 1], path_[i]);
        if (d <= seg && seg > 1e-6f) {
            const float t = d / seg;
            return {path_[i - 1].x + (path_[i].x - path_[i - 1].x) * t,
                    path_[i - 1].y + (path_[i].y - path_[i - 1].y) * t};
        }
        d -= seg;
    }
    return path_.back();
}

void Intro::turnTowards(float target, float dt) {
    float diff = std::fmod(target - heading_ + 540.0f, 360.0f) - 180.0f;   // -180 to 180
    const float step = cfg::kBearerTurnRate * dt;
    diff = std::clamp(diff, -step, step);
    heading_ = std::fmod(heading_ + diff + 360.0f, 360.0f);
}

void Intro::enter(Stage s) {
    stage_ = s;
    clock_ = 0.0f;
}

void Intro::update(float dt, bool takePressed) {
    clock_ += dt;
    if (taken_) handoverClock_ += dt;

    switch (stage_) {
        case Stage::Waiting:
            if (clock_ >= cfg::kIntroStartDelay) enter(Stage::Approaching);
            break;

        case Stage::Approaching: {
            along_ = std::min(length(), along_ + speed_ * dt);
            const Vec2 next = pointAt(along_ + 0.3f);
            pos_ = pointAt(along_);
            if (distance(pos_, next) > 1e-3f) turnTowards(headingTo(pos_, next), dt);
            if (along_ >= length() - 1e-4f) enter(Stage::Offering);
            break;
        }

        case Stage::Offering:
            turnTowards(headingTo(pos_, visitor_), dt);
            if (!taken_ && ((takePressed && clock_ >= cfg::kIntroPromptAfter) ||
                            clock_ >= cfg::kIntroAutoTake)) {
                taken_ = true;
                handoverClock_ = 0.0f;
                clock_ = 0.0f;
            }
            if (taken_ && clock_ >= cfg::kIntroHandoverTime + cfg::kIntroPause) enter(Stage::Returning);
            break;

        case Stage::Returning: {
            along_ = std::max(0.0f, along_ - speed_ * dt);
            const Vec2 next = pointAt(along_ - 0.3f);
            pos_ = pointAt(along_);
            if (distance(pos_, next) > 1e-3f) turnTowards(headingTo(pos_, next), dt);
            if (along_ <= 1e-4f) enter(Stage::Done);
            break;
        }

        case Stage::Done:
            turnTowards(postHeading_, dt);
            break;
    }
}

void Intro::skip() {
    taken_ = true;
    handoverClock_ = cfg::kIntroHandoverTime;
    along_ = 0.0f;
    pos_ = path_.front();
    heading_ = postHeading_;
    enter(Stage::Done);
    clock_ = cfg::kBearerRefreeze;   // already stone
}

float Intro::elevation() const {
    // Smoothstep over the first stretch of the route, so the step down eases.
    const float t = std::clamp(along_ / cfg::kBearerStepLength, 0.0f, 1.0f);
    return cfg::kBearerPlinthHeight * (1.0f - t * t * (3.0f - 2.0f * t));
}

bool Intro::frozen() const {
    return stage_ == Stage::Waiting || (stage_ == Stage::Done && clock_ >= cfg::kBearerRefreeze);
}

const char* Intro::clip() const {
    switch (stage_) {
        case Stage::Approaching:
        case Stage::Returning: return "walk";
        case Stage::Offering:  return taken_ ? "idle" : "give";
        default:               return taken_ ? "idle" : "idle_torch";
    }
}

float Intro::handover() const {
    if (!taken_) return 0.0f;
    const float t = std::clamp(handoverClock_ / cfg::kIntroHandoverTime, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

std::string Intro::prompt() const {
    if (stage_ == Stage::Offering && !taken_ && clock_ >= cfg::kIntroPromptAfter)
        return "PRESS E TO TAKE THE LAMP";
    return {};
}

} // namespace lumen
