#include "Gfx.h"
#include "Raster.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// Anisotropic filtering constants. vcpkg's glad does not include them, so they
// are defined here. The values come from the extension spec.
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

namespace lumen::gfx {
namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open shader file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

unsigned compile(GLenum type, const std::string& src, const std::string& path) {
    const unsigned s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);

    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error("Failed to compile " + path + ":\n" + log);
    }
    return s;
}

} // namespace

// ---- Shader ----------------------------------------------------------------

Shader::~Shader() {
    if (prog_) glDeleteProgram(prog_);
}

std::unique_ptr<Shader> Shader::fromFiles(const std::string& vertPath,
                                          const std::string& fragPath) {
    const unsigned vs = compile(GL_VERTEX_SHADER, readFile(vertPath), vertPath);
    const unsigned fs = compile(GL_FRAGMENT_SHADER, readFile(fragPath), fragPath);

    auto sh = std::unique_ptr<Shader>(new Shader());
    sh->prog_ = glCreateProgram();
    glAttachShader(sh->prog_, vs);
    glAttachShader(sh->prog_, fs);
    glLinkProgram(sh->prog_);

    int ok = 0;
    glGetProgramiv(sh->prog_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(sh->prog_, sizeof(log), nullptr, log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw std::runtime_error("Failed to link shader program:\n" + std::string(log));
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return sh;
}

void Shader::use() const { glUseProgram(prog_); }

int Shader::location(const std::string& name) const {
    auto it = locs_.find(name);
    if (it != locs_.end()) return it->second;
    const int loc = glGetUniformLocation(prog_, name.c_str());
    locs_.emplace(name, loc);
    return loc;
}

void Shader::set(const std::string& n, int v) const   { glUniform1i(location(n), v); }
void Shader::set(const std::string& n, float v) const { glUniform1f(location(n), v); }
void Shader::set(const std::string& n, const glm::vec2& v) const {
    glUniform2f(location(n), v.x, v.y);
}
void Shader::set(const std::string& n, const glm::vec3& v) const {
    glUniform3f(location(n), v.x, v.y, v.z);
}
void Shader::set(const std::string& n, const glm::mat3& v) const {
    glUniformMatrix3fv(location(n), 1, GL_FALSE, &v[0][0]);
}
void Shader::set(const std::string& n, const glm::mat4& v) const {
    glUniformMatrix4fv(location(n), 1, GL_FALSE, &v[0][0]);
}
void Shader::set(const std::string& n, const std::vector<glm::mat4>& v) const {
    if (!v.empty())
        glUniformMatrix4fv(location(n), static_cast<GLsizei>(v.size()), GL_FALSE, &v[0][0][0]);
}

// ---- TextureCache ----------------------------------------------------------

TextureCache::~TextureCache() {
    for (auto& [path, tex] : cache_)
        if (tex && tex->id_) glDeleteTextures(1, &tex->id_);
}

namespace {

// The driver's maximum anisotropy, or 1.0 if unsupported (the query then
// raises an error). Asked once.
float maxAnisotropy() {
    static float cached = -1.0f;
    if (cached < 0.0f) {
        while (glGetError() != GL_NO_ERROR) {}      // clear any earlier error
        float value = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &value);
        cached = (glGetError() == GL_NO_ERROR) ? value : 1.0f;
    }
    return cached;
}

unsigned uploadPixels(const unsigned char* px, int w, int h, int channels,
                      Texture::Kind kind) {
    unsigned id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // sRGB for colours, linear for data like normals.
    GLenum internal, format;
    if (channels == 4) {
        internal = (kind == Texture::Kind::Colour) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        format   = GL_RGBA;
    } else if (channels == 3) {
        internal = (kind == Texture::Kind::Colour) ? GL_SRGB8 : GL_RGB8;
        format   = GL_RGB;
    } else {
        internal = GL_R8;
        format   = GL_RED;
    }

    // Our rows are packed tight. OpenGL expects each row to start on 4 bytes,
    // so an RGB image whose width is not a multiple of 4 would be read past its end.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal), w, h, 0,
                 format, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Repeat, so wall and floor materials tile. Clamping smeared the edge pixels
    // over everything past the first tile.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Anisotropic filtering keeps paintings sharp when seen at an angle.
    const float maxAniso = maxAnisotropy();
    if (maxAniso > 1.0f) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        (maxAniso < 8.0f) ? maxAniso : 8.0f);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

} // namespace

const Texture* TextureCache::get(const std::string& path, Texture::Kind kind) {
    const std::string key = path + (kind == Texture::Kind::Colour ? "|c" : "|d");
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second.get();

    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &channels, 0);
    if (!px) {
        std::fprintf(stderr,
                     "[gfx] Could not load texture '%s' - using a placeholder.\n"
                     "      Check the path in assets/works.json.\n", path.c_str());
        return white();
    }

    auto tex = std::make_unique<Texture>();
    tex->id_ = uploadPixels(px, w, h, channels, kind);
    tex->w_  = w;
    tex->h_  = h;
    stbi_image_free(px);

    const Texture* out = tex.get();
    cache_.emplace(key, std::move(tex));
    return out;
}

const Texture* TextureCache::fromMemory(const std::string& key,
                                        const std::vector<unsigned char>& bytes,
                                        Texture::Kind kind) {
    const std::string k = key + (kind == Texture::Kind::Colour ? "|mc" : "|md");
    auto it = cache_.find(k);
    if (it != cache_.end()) return it->second.get();

    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* px = bytes.empty() ? nullptr
                      : stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                              &w, &h, &channels, 0);
    if (!px) {
        std::fprintf(stderr, "[gfx] Could not read the image packed in '%s'.\n", key.c_str());
        return white();
    }
    auto tex = std::make_unique<Texture>();
    tex->id_ = uploadPixels(px, w, h, channels, kind);
    tex->w_  = w;
    tex->h_  = h;
    stbi_image_free(px);
    const Texture* out = tex.get();
    cache_.emplace(k, std::move(tex));
    return out;
}

const Texture* TextureCache::fromCanvas(const std::string& key, const raster::Canvas& image) {
    const std::string k = key + "|canvas";
    auto it = cache_.find(k);
    if (it != cache_.end()) return it->second.get();
    if (image.width() <= 0 || image.height() <= 0) return white();

    // A Canvas starts at the top row; OpenGL textures start at the bottom.
    const std::size_t row = static_cast<std::size_t>(image.width()) * 4;
    std::vector<unsigned char> flipped(image.byteSize());
    for (int y = 0; y < image.height(); ++y)
        std::memcpy(&flipped[static_cast<std::size_t>(image.height() - 1 - y) * row],
                    image.pixels() + static_cast<std::size_t>(y) * row, row);

    auto tex = std::make_unique<Texture>();
    tex->id_ = uploadPixels(flipped.data(), image.width(), image.height(), 4, Texture::Kind::Colour);
    tex->w_  = image.width();
    tex->h_  = image.height();
    const Texture* out = tex.get();
    cache_.emplace(k, std::move(tex));
    return out;
}

const Texture* TextureCache::reliefFrom(const std::string& colourPath, float strength) {
    const std::string key = colourPath + "|relief";
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second.get();

    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* px = stbi_load(colourPath.c_str(), &w, &h, &channels, 0);
    if (!px || w < 2 || h < 2) {
        if (px) stbi_image_free(px);
        return flatNormal();
    }

    // A Sobel filter on the brightness gives a slope, used as the surface normal.
    auto lum = [&](int x, int y) -> float {
        x = (x < 0) ? 0 : (x >= w ? w - 1 : x);
        y = (y < 0) ? 0 : (y >= h ? h - 1 : y);
        const unsigned char* p = px + (static_cast<std::size_t>(y) * w + x) * channels;
        if (channels >= 3)
            return (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / 255.0f;
        return p[0] / 255.0f;
    };

    std::vector<unsigned char> normal(static_cast<std::size_t>(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float gx =
                (lum(x - 1, y - 1) + 2.0f * lum(x - 1, y) + lum(x - 1, y + 1)) -
                (lum(x + 1, y - 1) + 2.0f * lum(x + 1, y) + lum(x + 1, y + 1));
            const float gy =
                (lum(x - 1, y - 1) + 2.0f * lum(x, y - 1) + lum(x + 1, y - 1)) -
                (lum(x - 1, y + 1) + 2.0f * lum(x, y + 1) + lum(x + 1, y + 1));

            glm::vec3 n(gx * strength, gy * strength, 1.0f);
            n = glm::normalize(n);

            const std::size_t i = (static_cast<std::size_t>(y) * w + x) * 3;
            normal[i + 0] = static_cast<unsigned char>((n.x * 0.5f + 0.5f) * 255.0f);
            normal[i + 1] = static_cast<unsigned char>((n.y * 0.5f + 0.5f) * 255.0f);
            normal[i + 2] = static_cast<unsigned char>((n.z * 0.5f + 0.5f) * 255.0f);
        }
    }
    stbi_image_free(px);

    auto tex = std::make_unique<Texture>();
    tex->id_ = uploadPixels(normal.data(), w, h, 3, Texture::Kind::Data);
    tex->w_  = w;
    tex->h_  = h;

    const Texture* out = tex.get();
    cache_.emplace(key, std::move(tex));
    return out;
}

const Texture* TextureCache::white() {
    auto it = cache_.find("|white");
    if (it != cache_.end()) return it->second.get();

    const unsigned char px[4] = {255, 255, 255, 255};
    auto tex = std::make_unique<Texture>();
    tex->id_ = uploadPixels(px, 1, 1, 4, Texture::Kind::Data);
    tex->w_ = tex->h_ = 1;

    const Texture* out = tex.get();
    cache_.emplace("|white", std::move(tex));
    return out;
}

const Texture* TextureCache::flatNormal() {
    auto it = cache_.find("|flatn");
    if (it != cache_.end()) return it->second.get();

    const unsigned char px[3] = {128, 128, 255};   // (0,0,1) encoded
    auto tex = std::make_unique<Texture>();
    tex->id_ = uploadPixels(px, 1, 1, 3, Texture::Kind::Data);
    tex->w_ = tex->h_ = 1;

    const Texture* out = tex.get();
    cache_.emplace("|flatn", std::move(tex));
    return out;
}

// ---- MeshData --------------------------------------------------------------

void MeshData::addTriangle(const Vertex& a, const Vertex& b, const Vertex& c) {
    const unsigned base = static_cast<unsigned>(vertices.size());
    vertices.push_back(a);
    vertices.push_back(b);
    vertices.push_back(c);
    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
}

void MeshData::addQuad(const glm::vec3& a, const glm::vec3& b,
                       const glm::vec3& c, const glm::vec3& d,
                       float uScale, float vScale) {
    const glm::vec3 n = glm::normalize(glm::cross(b - a, d - a));
    glm::vec3 t = b - a;
    const float tl = glm::length(t);
    t = (tl > 1e-6f) ? t / tl : glm::vec3(1.0f, 0.0f, 0.0f);

    const unsigned base = static_cast<unsigned>(vertices.size());
    vertices.push_back(Vertex{a, n, {0.0f, 0.0f},        t});
    vertices.push_back(Vertex{b, n, {uScale, 0.0f},      t});
    vertices.push_back(Vertex{c, n, {uScale, vScale},    t});
    vertices.push_back(Vertex{d, n, {0.0f, vScale},      t});

    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
}

void MeshData::append(const MeshData& other, const glm::mat4& transform) {
    const unsigned base = static_cast<unsigned>(vertices.size());
    const glm::mat3 nrm = glm::mat3(transform);

    for (const Vertex& v : other.vertices) {
        Vertex out = v;
        out.position = glm::vec3(transform * glm::vec4(v.position, 1.0f));
        out.normal   = glm::normalize(nrm * v.normal);
        out.tangent  = glm::normalize(nrm * v.tangent);
        vertices.push_back(out);
    }
    for (unsigned i : other.indices) indices.push_back(base + i);
}

// ---- Mesh ------------------------------------------------------------------

Mesh::~Mesh() {
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

std::unique_ptr<Mesh> Mesh::upload(const MeshData& data) {
    auto m = std::unique_ptr<Mesh>(new Mesh());
    m->indexCount_ = data.indices.size();
    m->tris_       = data.indices.size() / 3;
    if (data.vertices.empty() || data.indices.empty()) return m;

    glGenVertexArrays(1, &m->vao_);
    glGenBuffers(1, &m->vbo_);
    glGenBuffers(1, &m->ebo_);

    glBindVertexArray(m->vao_);

    glBindBuffer(GL_ARRAY_BUFFER, m->vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex)),
                 data.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.indices.size() * sizeof(unsigned)),
                 data.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, tangent)));

    glBindVertexArray(0);
    return m;
}

std::unique_ptr<Mesh> Mesh::upload(const SkinnedMeshData& data) {
    auto m = std::unique_ptr<Mesh>(new Mesh());
    m->indexCount_ = data.indices.size();
    m->tris_       = data.indices.size() / 3;
    if (data.vertices.empty() || data.indices.empty()) return m;

    glGenVertexArrays(1, &m->vao_);
    glGenBuffers(1, &m->vbo_);
    glGenBuffers(1, &m->ebo_);
    glBindVertexArray(m->vao_);

    glBindBuffer(GL_ARRAY_BUFFER, m->vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.vertices.size() * sizeof(SkinnedVertex)),
                 data.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.indices.size() * sizeof(unsigned)),
                 data.indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = sizeof(SkinnedVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(SkinnedVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(SkinnedVertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(SkinnedVertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(SkinnedVertex, tangent)));
    // Joint numbers stay whole numbers in the shader, so they use the I version.
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 4, GL_UNSIGNED_SHORT, stride,
                           reinterpret_cast<void*>(offsetof(SkinnedVertex, joints)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(SkinnedVertex, weights)));

    glBindVertexArray(0);
    return m;
}

void Mesh::draw() const {
    if (!vao_ || indexCount_ == 0) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---- ShadowMap -------------------------------------------------------------

namespace {

unsigned makeDepthTexture(int size) {
    unsigned t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Outside the map counts as lit.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    // Let the GPU do the depth comparison, so a lookup returns 0 (shadow) to 1 (lit).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    return t;
}

} // namespace

ShadowMap::~ShadowMap() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (depth_) glDeleteTextures(1, &depth_);
    if (dummy_) glDeleteTextures(1, &dummy_);
}

bool ShadowMap::create(int size) {
    size_ = size;
    depth_ = makeDepthTexture(size);
    dummy_ = makeDepthTexture(1);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
    glDrawBuffer(GL_NONE);   // depth only
    glReadBuffer(GL_NONE);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!ok) std::fprintf(stderr, "[gfx] shadow map framebuffer is incomplete\n");
    return ok;
}

void ShadowMap::begin() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, size_, size_);
    glClear(GL_DEPTH_BUFFER_BIT);
    // Draw both sides, and push depths back a little to stop surfaces shadowing themselves.
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
}

void ShadowMap::end(int screenW, int screenH) const {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenW, screenH);
}

} // namespace lumen::gfx
