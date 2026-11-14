// Model.h - loads statues saved as glTF 2.0 (.gltf with its .bin, or .glb).
//
// Reads the node tree, applies each node's transform, and returns plain
// MeshData grouped by material, so it can be tested without a window.
// Supports what museum scans use: triangles, float positions, normals,
// texture coordinates and tangents, 8, 16 or 32 bit indices, and the colour
// and normal textures of each material.

#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx.h"
#include "Skeleton.h"

namespace lumen::model {

// All the triangles that share one material.
struct Part {
    gfx::MeshData mesh;
    std::string   albedoPath;   // colour texture, empty if none
    std::string   normalPath;   // normal map, empty if none
    glm::vec4     baseColour{1.0f};
    bool          hasUVs = false;
    bool          hasTangents = false;
};

struct Model {
    std::vector<Part> parts;
    glm::vec3 boundsMin{0.0f};   // after the node transforms
    glm::vec3 boundsMax{0.0f};

    std::size_t triangleCount() const;
    glm::vec3   size() const { return boundsMax - boundsMin; }
};

// Throws std::runtime_error with a clear message if the file is missing,
// broken, or uses something this loader does not support.
Model load(const std::string& path);

// Scales the model evenly so it stands `height` metres tall, with its feet on
// y = 0 and its middle on x = z = 0. Apply this first, then place it.
glm::mat4 fitToHeight(const Model& m, float height);
glm::mat4 fitToHeight(const glm::vec3& boundsMin, const glm::vec3& boundsMax, float height);

// ---- animated characters --------------------------------------------------------

// The triangles of a character that share one material. The vertices keep
// the skin's own space; the bones place them.
struct SkinnedPart {
    gfx::SkinnedMeshData       mesh;
    std::string                albedoPath;    // colour texture file, empty if none
    std::vector<unsigned char> albedoBytes;   // or the image packed in a .glb
    std::string                albedoKey;     // names the packed image in the cache
    glm::vec4                  baseColour{1.0f};
};

struct SkinnedModel {
    std::vector<SkinnedPart> parts;
    skel::Skeleton           skeleton;
    std::vector<skel::Clip>  clips;      // any animations in the same file
    glm::vec3 boundsMin{0.0f};           // in the rest pose
    glm::vec3 boundsMax{0.0f};

    std::size_t triangleCount() const;
};

// A rigged character: its skin, skeleton and any clips. Meshes without a skin
// ride on the nearest joint above them. Throws with a clear message.
SkinnedModel loadSkinned(const std::string& path);

// Only the animations from a file, such as an animation library.
std::vector<skel::Clip> loadClips(const std::string& path);

} // namespace lumen::model
