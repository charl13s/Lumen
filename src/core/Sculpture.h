// Sculpture.h - the statues and carved reliefs in the corridor and atrium.
//
// What goes where is in assets/sculpture.json. A statue stands on a plinth,
// either in the atrium under the oculus or in one of the corridor niches,
// facing the visitor. A relief hangs on the atrium's outer wall, facing the
// corridor. The placing is plain maths, tested without a window; build()
// loads the models and textures.

#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx.h"
#include "Model.h"
#include "Plan.h"

namespace lumen::sculpture {

// How a statue's surface is drawn.
enum class Finish {
    Scan,     // the scan's own colour and normal textures
    Marble,   // marble.jpg wrapped on with triplanar mapping
    Bronze,   // the same, tinted dark bronze and shinier
};

struct StatueSpec {
    std::string title;
    std::string model;        // path under assets/
    std::string place;        // "atrium", or "niche 0" to "niche 3"
    std::string credit;
    float       height  = 1.8f;    // m, the statue itself
    float       turnDeg = 0.0f;    // extra turn if the scan faces the wrong way
    float       forward = 0.0f;    // m, moves it out towards the visitor
    glm::vec3   plinth{0.9f, 0.6f, 0.9f};   // width, height, depth in m
    Finish      finish  = Finish::Scan;
};

struct ReliefSpec {
    std::string title;
    std::string image;        // path under assets/
    std::string credit;
    char        wall  = 'N';  // which outer face of the atrium: N, S, E or W
    float       at    = 0.0f; // plan x along a N or S face, plan y along E or W
    float       width = 1.6f; // m; the height follows the image
    float       centreHeight = 2.0f;
};

struct Catalogue {
    std::vector<StatueSpec> statues;
    std::vector<ReliefSpec> reliefs;
};

// Throws with a clear message if the file is missing or broken.
Catalogue load(const std::string& path);

// "scan", "marble" or "bronze". False for anything else.
bool parseFinish(const std::string& name, Finish& out);

// The material for one part of a model in a finish. The marble and bronze
// surfaces use materials/marble.jpg and materials/plaster.jpg from assetDir.
gfx::Material statueMaterial(const model::Part& part, Finish finish,
                             const std::string& assetDir, gfx::TextureCache& textures);

// Where a statue stands, in plan space, and which way it faces.
struct Spot {
    Vec2        at;
    Vec2        facing;   // unit vector, towards the visitor
    std::string zone;     // "ATRIUM" or "NICHE", for the lighting
};

// Throws if the place is not "atrium" or a niche in the plan.
Spot spotFor(const Plan& plan, const std::string& place);

// Model to world for a statue already fitted to its height: turns it to face
// spot.facing (a scan's front is +Z), then stands it on top of the plinth.
glm::mat4 statueTransform(const Spot& spot, float plinthHeight, float turnDeg);

// A point on an outer face of the atrium wall, in world space.
struct WallSpot {
    glm::vec3 centre;   // on the wall face
    glm::vec3 normal;   // out of the wall, into the corridor
    glm::vec3 right;    // the visitor's right, looking at the wall
};

WallSpot wallSpotFor(const Plan& plan, char wall, float at, float centreHeight);

// A relief panel: the carved image on a stone slab standing out from the wall.
struct Panel {
    gfx::MeshData face;   // the image, textured 0 to 1
    gfx::MeshData slab;   // the stone around and behind it
};

Panel buildPanel(const WallSpot& spot, float width, float height);

// The outline of each plinth as wall segments, for the collider. Collision
// walls are thick, so each outline is pulled in by half the wall thickness
// to leave the visitor stopping at the plinth's real edge.
std::vector<Segment> plinthOutlines(const Plan& plan, const Catalogue& cat);

// Something to draw, with the zone that decides its light.
struct Item {
    gfx::MeshData mesh;
    gfx::Material material;
    glm::mat4     transform{1.0f};
    std::string   zone;   // "ATRIUM" or "NICHE"
    std::string   note;
};

// Loads every statue and relief in the catalogue. A statue that fails to load
// is skipped with a message, so one bad file does not stop the gallery.
std::vector<Item> build(const Plan& plan, const Catalogue& cat,
                        const std::string& assetDir, gfx::TextureCache& textures);

} // namespace lumen::sculpture
