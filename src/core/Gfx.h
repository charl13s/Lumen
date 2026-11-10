// Gfx.h - every OpenGL object in LUMEN is created here and nowhere else.
//
// THE RULE THAT KEEPS FIVE PEOPLE OUT OF EACH OTHER'S WAY:
//
//   A room file may not call OpenGL. Not glGenTextures, not glDrawArrays, not
//   glUniform. A room describes what it wants; the core draws it.
//
// If five people each write a texture loader, we get five loaders, five copies
// of the sRGB mistake, and a merge conflict every week. With one loader there is
// one place to fix it, and when Timothy improves the lighting every room gets
// the improvement for free.
//
// If you need something here that does not exist yet, open an issue. Do not add
// a GL call to a room file.

#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Config.h"

namespace lumen::gfx {

// ---- shaders ---------------------------------------------------------------

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;

    // Compiles and links. Throws std::runtime_error carrying the GLSL compiler's
    // own message, which is almost always enough to find the problem.
    static std::unique_ptr<Shader> fromFiles(const std::string& vertPath,
                                             const std::string& fragPath);

    void use() const;
    unsigned id() const { return prog_; }

    void set(const std::string& name, int v) const;
    void set(const std::string& name, float v) const;
    void set(const std::string& name, const glm::vec2& v) const;
    void set(const std::string& name, const glm::vec3& v) const;
    void set(const std::string& name, const glm::mat4& v) const;

private:
    unsigned prog_ = 0;
    mutable std::unordered_map<std::string, int> locs_;
    int location(const std::string& name) const;
};

// ---- textures --------------------------------------------------------------

// Loaded once, cached by path. Ask for the same file from five rooms and you get
// the same GPU handle, loaded once.
//
// GAMMA: colour textures are uploaded as sRGB so the GPU linearises them on
// read. All lighting maths then happens in linear space, and the final frame is
// converted back on output. Get this wrong and every light intensity in the
// project has to be re-tuned, so it is handled here, once.
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

    // Returns nullptr and logs if the file is missing, so a member with a typo in
    // a filename gets a grey placeholder and a clear message, not a crash that
    // blocks everyone else.
    const Texture* get(const std::string& path, Texture::Kind kind = Texture::Kind::Colour);

    // Derives a normal map from a colour image's own luminance gradient. This is
    // how a flat painting stops reading as a sticker: the brushwork catches the
    // lamp as you walk past, for no extra asset and about fifteen lines of code.
    const Texture* reliefFrom(const std::string& colourPath,
                              float strength = cfg::kReliefStrength);

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

// A chunk of geometry on the GPU. Build it on the CPU with MeshData, then upload.
struct MeshData {
    std::vector<Vertex>        vertices;
    std::vector<unsigned>      indices;

    void addTriangle(const Vertex& a, const Vertex& b, const Vertex& c);
    // Quad in winding order a->b->c->d. Computes the normal and tangent for you.
    void addQuad(const glm::vec3& a, const glm::vec3& b,
                 const glm::vec3& c, const glm::vec3& d,
                 float uScale = 1.0f, float vScale = 1.0f);
    void append(const MeshData& other, const glm::mat4& transform);
    std::size_t triangleCount() const { return indices.size() / 3; }
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    static std::unique_ptr<Mesh> upload(const MeshData& data);
    void draw() const;
    std::size_t triangleCount() const { return tris_; }

private:
    unsigned vao_ = 0, vbo_ = 0, ebo_ = 0;
    std::size_t indexCount_ = 0;
    std::size_t tris_ = 0;
};

// ---- materials -------------------------------------------------------------

// What a surface is made of. A room fills this in; the core binds it.
struct Material {
    const Texture* albedo = nullptr;
    const Texture* normal = nullptr;
    glm::vec3      tint{1.0f};
    float          specular  = 0.15f;  // 0 = chalk, 1 = polished
    float          shininess = 32.0f;
    float          emissive  = 0.0f;   // >0 makes it glow on its own
    cfg::Shading   shading   = cfg::Shading::BlinnPhong;
};

} // namespace lumen::gfx
