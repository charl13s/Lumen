// Intro.h - the opening: a statue brings you the lamp.
//
// You start in the dark corridor, facing the atrium. Beside the Apollo stands
// a marble statue of a torch bearer, holding the only light. It comes alive,
// steps down off its plinth, walks out to you and holds out the torch. Press
// E (or wait) to take it: the light moves into your hand, and the statue
// walks back, steps up onto its plinth and turns to stone again. You can look
// around the whole time, and walk once you have the lamp.
//
// Only the story is here: where the bearer is, which clip plays, who holds
// the light. No OpenGL, so it can be tested.

#pragma once
#include <string>
#include <vector>

#include "Plan.h"

namespace lumen {

// Who brings the lamp, from assets/intro.json.
struct BearerSettings {
    std::string character;    // the rigged model, under assets/
    std::string animations;   // an animation library for the same rig, or empty
    std::string credit;
    std::string hand = "hand_r";   // the bone the torch is held in
    std::string grip;              // a knuckle bone, to put the torch in the palm; optional
    std::string finish = "marble"; // "marble" for a living statue, "skin" for its own textures
    std::string idle = "idle", walk = "walk", give = "give";   // clip names
    std::string idleTorch = "idle";   // standing still while holding the torch
    float giveHold  = -1.0f;  // s into the give clip to hold, with the arm out; -1 for its end
    float height    = 1.75f;  // m
    float walkSpeed = 1.2f;   // m/s, to match the walk clip's stride
};

// False if the file is missing or broken, with the reason in why.
bool loadBearerSettings(const std::string& path, BearerSettings& out, std::string& why);

class Intro {
public:
    enum class Stage {
        Waiting,      // the bearer stands by the statue
        Approaching,  // walking out to the visitor
        Offering,     // holding out the torch
        Returning,    // walking back without it
        Done,         // standing by the statue for good
    };

    // visitor is where the visitor stands while waiting.
    Intro(const Plan& plan, Vec2 visitor);

    // takePressed is true on the frame E is pressed.
    void update(float dt, bool takePressed);

    // Jumps to the end: the visitor has the lamp, the bearer is at their post.
    void skip();

    Stage stage() const { return stage_; }
    Vec2  position() const { return pos_; }
    // How high the bearer's feet are: on the plinth, stepping down, or on the floor.
    float elevation() const;
    // True while the bearer is stone: before it wakes, and once it is home again.
    bool  frozen() const;
    float headingDeg() const { return heading_; }

    // "idle_torch" (standing with the torch), "idle", "walk" or "give".
    const char* clip() const;

    bool  bearerHasTorch() const { return !taken_; }
    // How far the light has moved from the torch into the visitor's hand, 0 to 1.
    float handover() const;
    bool  visitorLocked() const { return !taken_; }
    // Text for the screen, or empty.
    std::string prompt() const;

    // The route from the statue out to the visitor.
    const std::vector<Vec2>& path() const { return path_; }
    float walkSpeed() const { return speed_; }
    void  setWalkSpeed(float metresPerSecond) { speed_ = metresPerSecond; }

private:
    std::vector<Vec2> path_;   // post first, handover point last
    Vec2  visitor_;
    Vec2  pos_;
    float heading_ = 270.0f;
    float postHeading_ = 270.0f;

    Stage stage_ = Stage::Waiting;
    float clock_ = 0.0f;       // seconds in this stage
    float along_ = 0.0f;       // metres walked along the path
    float handoverClock_ = 0.0f;
    bool  taken_ = false;
    float speed_ = 1.2f;

    void  enter(Stage s);
    Vec2  pointAt(float distance) const;
    float length() const;
    void  turnTowards(float targetDeg, float dt);
};

} // namespace lumen
