// Character.h - an animated person: a skinned mesh, its skeleton and its clips.
//
// Plays one clip at a time and fades into the next, so walking into standing
// still is smooth. Clips can come from a separate animation library made for
// the same rig; they are fitted to this character's bones by name.

#pragma once
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx.h"
#include "Plan.h"
#include "Skeleton.h"

namespace lumen {

class Character {
public:
    // Loads the character, and clips from animPath as well if it is not empty.
    // height is in metres. Returns false and prints why if it cannot.
    bool load(const std::string& modelPath, const std::string& animPath, float height,
              gfx::TextureCache& textures);
    bool loaded() const { return !parts_.empty(); }

    // The clip whose name matches, ignoring case: an exact match first, then
    // the first name containing it. Empty if none.
    std::string findClip(const std::string& name) const;
    std::vector<std::string> clipNames() const;

    // Starts a clip, fading from the current one. Does nothing if it is already
    // playing. A clip that does not loop can stop at holdAt seconds instead of its end.
    void play(const std::string& clip, bool loop, float fadeSeconds = 0.3f,
              float holdAt = -1.0f);
    void update(float dt);

    // Stands the character at a plan position, facing a compass heading
    // (0 north, 90 east), like the camera.
    void place(Vec2 planPos, float headingDeg, float elevation = 0.0f);

    // Gives every part the same material, keeping the character's texture
    // coordinates, so a marble texture follows the body as it moves.
    void setMaterial(const gfx::Material& m);

    glm::mat4 model() const { return place_ * fit_; }
    const std::vector<glm::mat4>& joints() const { return joints_; }

    // A bone whose name contains the hint, ignoring case, or -1.
    int findNode(const std::string& hint) const;
    // Where a bone is in the world now.
    glm::vec3 nodeWorld(int node) const;

    struct DrawPart {
        std::unique_ptr<gfx::Mesh> mesh;
        gfx::Material              material;
    };
    const std::vector<DrawPart>& parts() const { return parts_; }
    std::size_t triangleCount() const { return triangles_; }

private:
    skel::Skeleton                sk_;
    std::vector<skel::Clip>       clips_;
    std::vector<std::vector<int>> bindings_;
    std::vector<DrawPart>         parts_;
    std::size_t                   triangles_ = 0;

    glm::mat4 fit_{1.0f}, place_{1.0f};
    int   current_ = -1, previous_ = -1;
    bool  loopCurrent_ = true, loopPrevious_ = true;
    float timeCurrent_ = 0.0f, timePrevious_ = 0.0f;
    float fade_ = 1.0f, fadeLength_ = 0.3f;   // fade_ runs 0 to 1 into the current clip
    float holdCurrent_ = -1.0f, holdPrevious_ = -1.0f;

    std::vector<glm::mat4> globals_, joints_;
};

} // namespace lumen
