// Gfx.h - shaders, textures, meshes and materials. Owner: Timothy.
// All OpenGL objects are created here. Room files never call OpenGL.

#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Config.h"

namespace lumen::raster { class Canvas; }

namespace lumen::gfx {

// ---- shaders ---------------------------------------------------------------

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;

    // Compiles and links. Throws with the GLSL compiler's error message.
    static std::unique_ptr<Shader> fromFiles(const std::string& vertPath,
                                             const std::string& fragPath);

    void use() const;
    unsigned id() const { return prog_; }

    void set(const std::string& name, int v) const;
    void set(const std::string& name, float v) const;
    void set(const std::string& name, const glm::vec2& v) const;
    void set(const std::string& name, const glm::vec3& v) const;
    // Without this, a mat3 silently converts to mat4, the upload fails, and
    // every normal in the scene becomes zero.
    void set(const std::string& name, const glm::mat3& v) const;
    void set(const std::string& name, const glm::mat4& v) const;
    // An array of matrices, such as the joints of a skinned character.
    void set(const std::string& name, const std::vector<glm::mat4>& v) const;

private:
    unsigned prog_ = 0;
    mutable std::unordered_map<std::string, int> locs_;
    int location(const std::string& name) const;
};

// ---- textures --------------------------------------------------------------

// Each file is loaded once and cached by path.
// Colour textures are uploaded as sRGB so lighting is done in linear space.
class Texture {
public:
    enum class Kind {
        Colour,   // sRGB: paintings, wall materials, anything you look at
        Data,     // linear: normal maps, roughness, masks
    };

    unsigned id()     const { return id_; }
    int      width()  const { return w_; }
    int      height() const { return h_; }

private:
    friend class TextureCache;
    unsigned id_ = 0;
    int w_ = 0, h_ = 0;
};

class TextureCache {
public:
    ~TextureCache();

    // Returns nullptr and prints a message if the file is missing.
    const Texture* get(const std::string& path, Texture::Kind kind = Texture::Kind::Colour);

    // Makes a normal map from an image's brightness, so brushstrokes catch the lamp.
    const Texture* reliefFrom(const std::string& colourPath,
                              float strength = cfg::kReliefStrength);

    // An image packed inside another file, such as a .glb model. key names it in the cache.
    const Texture* fromMemory(const std::string& key, const std::vector<unsigned char>& bytes,
                              Texture::Kind kind = Texture::Kind::Colour);

    // A picture drawn by our own 2D code. Uploaded once per key, as a colour texture.
    const Texture* fromCanvas(const std::string& key, const raster::Canvas& image);

    const Texture* white();   // 1x1, for untextured surfaces
    const Texture* flatNormal();  // 1x1 pointing straight out

    std::size_t count() const { return cache_.size(); }

private:
    std::unordered_map<std::string, std::unique_ptr<Texture>> cache_;
};

// ---- meshes ----------------------------------------------------------------

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
    glm::vec3 tangent{1.0f, 0.0f, 0.0f};   // for normal mapping
};

// Geometry built on the CPU, then uploaded as a Mesh.
struct MeshData {
    std::vector<Vertex>        vertices;
    std::vector<unsigned>      indices;

    void addTriangle(const Vertex& a, const Vertex& b, const Vertex& c);
    // Quad a, b, c, d in winding order. Normal and tangent are worked out for you.
    void addQuad(const glm::vec3& a, const glm::vec3& b,
                 const glm::vec3& c, const glm::vec3& d,
                 float uScale = 1.0f, float vScale = 1.0f);
    void append(const MeshData& other, const glm::mat4& transform);
    std::size_t triangleCount() const { return indices.size() / 3; }
};

// A vertex that follows up to four bones, for animated characters.
struct SkinnedVertex {
    glm::vec3     position{0.0f};
    glm::vec3     normal{0.0f, 1.0f, 0.0f};
    glm::vec2     uv{0.0f};
    glm::vec3     tangent{1.0f, 0.0f, 0.0f};
    std::uint16_t joints[4] = {0, 0, 0, 0};
    glm::vec4     weights{1.0f, 0.0f, 0.0f, 0.0f};   // add up to 1
};

struct SkinnedMeshData {
    std::vector<SkinnedVertex> vertices;
    std::vector<unsigned>      indices;
    std::size_t triangleCount() const { return indices.size() / 3; }
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    static std::unique_ptr<Mesh> upload(const MeshData& data);
    static std::unique_ptr<Mesh> upload(const SkinnedMeshData& data);
    void draw() const;
    std::size_t triangleCount() const { return tris_; }

private:
    unsigned vao_ = 0, vbo_ = 0, ebo_ = 0;
    std::size_t indexCount_ = 0;
    std::size_t tris_ = 0;
};

// ---- shadow map ------------------------------------------------------------

// A depth texture rendered from the lamp. begin() switches drawing into it,
// end() switches back to the window.
class ShadowMap {
public:
    ShadowMap() = default;
    ~ShadowMap();
    ShadowMap(const ShadowMap&)            = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    bool create(int size);
    void begin() const;
    void end(int screenW, int screenH) const;

    unsigned texture() const { return depth_; }
    unsigned placeholder() const { return dummy_; }   // bound while drawing the map itself

private:
    unsigned fbo_ = 0, depth_ = 0, dummy_ = 0;
    int size_ = 0;
};

// ---- materials -------------------------------------------------------------

// What a surface looks like. Rooms fill this in.
struct Material {
    const Texture* albedo = nullptr;
    const Texture* normal = nullptr;
    glm::vec3      tint{1.0f};
    float          specular  = 0.15f;  // 0 = chalk, 1 = polished
    float          shininess = 32.0f;
    float          emissive  = 0.0f;   // >0 makes it glow on its own
    float          triplanarScale = 1.0f;  // texture repeats per metre, Triplanar only
    cfg::Shading   shading   = cfg::Shading::BlinnPhong;
};

} // namespace lumen::gfx
