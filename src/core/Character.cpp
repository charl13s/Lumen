#include "Character.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>

#include "Config.h"
#include "Model.h"

namespace lumen {
namespace {

// For matching names: lower case, without dots, underscores, dashes or spaces,
// so "Hand.R", "hand_r" and "HandR" all match.
std::string lower(const std::string& s) {
    std::string out;
    for (char c : s)
        if (c != '.' && c != '_' && c != '-' && c != ' ')
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

} // namespace

bool Character::load(const std::string& modelPath, const std::string& animPath, float height,
                     gfx::TextureCache& textures) {
    model::SkinnedModel m;
    try {
        m = model::loadSkinned(modelPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[lumen] no character: %s\n", e.what());
        return false;
    }
    if (static_cast<int>(m.skeleton.joints.size()) > cfg::kMaxJoints) {
        std::fprintf(stderr, "[lumen] no character: '%s' has %zu joints; the shader takes %d.\n",
                     modelPath.c_str(), m.skeleton.joints.size(), cfg::kMaxJoints);
        return false;
    }

    sk_ = std::move(m.skeleton);
    clips_ = std::move(m.clips);
    if (!animPath.empty()) {
        try {
            for (skel::Clip& c : model::loadClips(animPath)) {
                if (skel::retarget(c, sk_) == 0) continue;   // made for a different rig
                clips_.push_back(std::move(c));
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[lumen] no animations: %s\n", e.what());
        }
    }
    if (clips_.empty()) {
        std::fprintf(stderr, "[lumen] no character: no clip in '%s' or '%s' moves its bones.\n",
                     modelPath.c_str(), animPath.c_str());
        return false;
    }
    for (skel::Clip& c : clips_) {
        skel::keepInPlace(c, sk_);
        bindings_.push_back(skel::bindClip(c, sk_));
    }

    for (model::SkinnedPart& p : m.parts) {
        DrawPart d;
        d.mesh = gfx::Mesh::upload(p.mesh);
        triangles_ += p.mesh.triangleCount();
        if (!p.albedoPath.empty())       d.material.albedo = textures.get(p.albedoPath);
        else if (!p.albedoBytes.empty()) d.material.albedo = textures.fromMemory(p.albedoKey, p.albedoBytes);
        d.material.tint      = glm::vec3(p.baseColour);
        d.material.specular  = 0.12f;
        d.material.shininess = 24.0f;
        parts_.push_back(std::move(d));
    }
    fit_ = model::fitToHeight(m.boundsMin, m.boundsMax, height);

    const skel::Pose rest = skel::restPose(sk_);
    globals_ = skel::globals(sk_, rest);
    joints_  = skel::jointMatrices(sk_, globals_);
    return true;
}

std::string Character::findClip(const std::string& name) const {
    const std::string want = lower(name);
    for (const skel::Clip& c : clips_)
        if (lower(c.name) == want) return c.name;
    for (const skel::Clip& c : clips_)
        if (lower(c.name).find(want) != std::string::npos) return c.name;
    return {};
}

std::vector<std::string> Character::clipNames() const {
    std::vector<std::string> out;
    for (const skel::Clip& c : clips_) out.push_back(c.name);
    return out;
}

void Character::play(const std::string& clip, bool loop, float fadeSeconds, float holdAt) {
    int index = -1;
    for (std::size_t i = 0; i < clips_.size(); ++i)
        if (clips_[i].name == clip) index = static_cast<int>(i);
    if (index < 0 || index == current_) return;
    previous_     = current_;
    loopPrevious_ = loopCurrent_;
    timePrevious_ = timeCurrent_;
    holdPrevious_ = holdCurrent_;
    holdCurrent_  = loop ? -1.0f : holdAt;
    current_      = index;
    loopCurrent_  = loop;
    timeCurrent_  = 0.0f;
    fadeLength_   = std::max(fadeSeconds, 1e-3f);
    fade_         = previous_ < 0 ? 1.0f : 0.0f;
}

void Character::update(float dt) {
    if (current_ < 0) return;
    timeCurrent_  += dt;
    timePrevious_ += dt;
    if (holdCurrent_ >= 0.0f)  timeCurrent_  = std::min(timeCurrent_, holdCurrent_);
    if (holdPrevious_ >= 0.0f) timePrevious_ = std::min(timePrevious_, holdPrevious_);
    fade_ = std::min(1.0f, fade_ + dt / fadeLength_);

    const std::size_t c = static_cast<std::size_t>(current_);
    skel::Pose pose = skel::restPose(sk_);
    skel::sample(clips_[c], bindings_[c], timeCurrent_, loopCurrent_, pose);
    if (previous_ >= 0 && fade_ < 1.0f) {
        const std::size_t p = static_cast<std::size_t>(previous_);
        skel::Pose before = skel::restPose(sk_);
        skel::sample(clips_[p], bindings_[p], timePrevious_, loopPrevious_, before);
        // Smoothstep, so the change starts and ends gently.
        const float w = fade_ * fade_ * (3.0f - 2.0f * fade_);
        pose = skel::blend(before, pose, w);
    }
    globals_ = skel::globals(sk_, pose);
    joints_  = skel::jointMatrices(sk_, globals_);
}

void Character::setMaterial(const gfx::Material& m) {
    for (DrawPart& p : parts_) p.material = m;
}

void Character::place(Vec2 planPos, float headingDeg, float elevation) {
    // A model's front is +Z. Turning by a sends it to (sin a, 0, cos a); the
    // heading points to (sin h, 0, -cos h) in the world, so a = 180 - h.
    const float a = glm::radians(180.0f - headingDeg);
    place_ = glm::translate(glm::mat4(1.0f), glm::vec3(worldX(planPos.x), elevation, worldZ(planPos.y))) *
             glm::rotate(glm::mat4(1.0f), a, glm::vec3(0.0f, 1.0f, 0.0f));
}

int Character::findNode(const std::string& hint) const {
    const std::string want = lower(hint);
    for (std::size_t i = 0; i < sk_.nodes.size(); ++i)
        if (lower(sk_.nodes[i].name) == want) return static_cast<int>(i);
    for (std::size_t i = 0; i < sk_.nodes.size(); ++i)
        if (lower(sk_.nodes[i].name).find(want) != std::string::npos) return static_cast<int>(i);
    return -1;
}

glm::vec3 Character::nodeWorld(int node) const {
    if (node < 0 || node >= static_cast<int>(globals_.size())) return glm::vec3(model()[3]);
    return glm::vec3(model() * globals_[static_cast<std::size_t>(node)] * glm::vec4(0, 0, 0, 1));
}

} // namespace lumen
