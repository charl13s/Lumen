#include "Model.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

namespace lumen::model {
namespace {

using json  = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;

[[noreturn]] void fail(const std::string& path, const std::string& why) {
    throw std::runtime_error("Could not load model '" + path + "': " + why);
}

Bytes readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return Bytes(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

std::string folderOf(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

// Decodes the base64 in a "data:...;base64," URI. Used by small test models.
Bytes decodeBase64(const std::string& s) {
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    Bytes out;
    int bits = 0, acc = 0;
    for (char c : s) {
        const int v = value(c);
        if (v < 0) continue;   // padding or whitespace
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFF));
        }
    }
    return out;
}

// The parsed file: the JSON and every buffer it points to.
struct Gltf {
    std::string        path;
    std::string        folder;
    json               doc;
    std::vector<Bytes> buffers;
};

Gltf open(const std::string& path) {
    Gltf g;
    g.path   = path;
    g.folder = folderOf(path);

    const Bytes file = readFile(path);
    if (file.empty()) fail(path, "the file is missing or empty");

    Bytes glbBin;
    if (file.size() >= 12 && std::memcmp(file.data(), "glTF", 4) == 0) {
        // Binary glTF: a 12 byte header, then a JSON chunk and a BIN chunk.
        std::size_t at = 12;
        while (at + 8 <= file.size()) {
            std::uint32_t len = 0, type = 0;
            std::memcpy(&len, &file[at], 4);
            std::memcpy(&type, &file[at + 4], 4);
            at += 8;
            if (at + len > file.size()) fail(path, "a chunk runs past the end of the file");
            if (type == 0x4E4F534A)        // "JSON"
                g.doc = json::parse(file.begin() + static_cast<std::ptrdiff_t>(at),
                                    file.begin() + static_cast<std::ptrdiff_t>(at + len));
            else if (type == 0x004E4942)   // "BIN"
                glbBin.assign(file.begin() + static_cast<std::ptrdiff_t>(at),
                              file.begin() + static_cast<std::ptrdiff_t>(at + len));
            at += len;
        }
        if (g.doc.is_null()) fail(path, "no JSON chunk");
    } else {
        try {
            g.doc = json::parse(file.begin(), file.end());
        } catch (const std::exception& e) {
            fail(path, std::string("the JSON is broken: ") + e.what());
        }
    }

    for (const json& b : g.doc.value("buffers", json::array())) {
        if (!b.contains("uri")) {          // the GLB's own BIN chunk
            g.buffers.push_back(glbBin);
            continue;
        }
        const std::string uri = b.at("uri").get<std::string>();
        if (uri.rfind("data:", 0) == 0) {
            const auto comma = uri.find(',');
            if (comma == std::string::npos) fail(path, "a data URI has no comma");
            g.buffers.push_back(decodeBase64(uri.substr(comma + 1)));
        } else {
            Bytes data = readFile(g.folder + uri);
            if (data.empty()) fail(path, "its buffer '" + uri + "' is missing");
            g.buffers.push_back(std::move(data));
        }
        const std::size_t want = b.value("byteLength", std::size_t{0});
        if (g.buffers.back().size() < want)
            fail(path, "a buffer is shorter than the file says");
    }
    return g;
}

// ---- accessors --------------------------------------------------------------

int componentCount(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2")   return 2;
    if (type == "VEC3")   return 3;
    if (type == "VEC4")   return 4;
    if (type == "MAT4")   return 16;
    return 0;
}

int componentBytes(int componentType) {
    switch (componentType) {
        case 5120: case 5121: return 1;   // byte, unsigned byte
        case 5122: case 5123: return 2;   // short, unsigned short
        case 5125: case 5126: return 4;   // unsigned int, float
        default: return 0;
    }
}

// Where an accessor's data is, and how far apart its elements are.
struct View {
    const std::uint8_t* data = nullptr;
    std::size_t count  = 0;
    std::size_t stride = 0;
    int componentType  = 0;
    int components     = 0;
};

View accessor(const Gltf& g, int index) {
    const json& acc = g.doc.at("accessors").at(static_cast<std::size_t>(index));
    if (acc.contains("sparse")) fail(g.path, "sparse accessors are not supported");
    if (!acc.contains("bufferView")) fail(g.path, "an accessor has no data");

    View v;
    v.count         = acc.at("count").get<std::size_t>();
    v.componentType = acc.at("componentType").get<int>();
    v.components    = componentCount(acc.at("type").get<std::string>());
    const int bytes = componentBytes(v.componentType);
    if (v.components == 0 || bytes == 0) fail(g.path, "an accessor has an unknown type");

    const json& bv = g.doc.at("bufferViews").at(acc.at("bufferView").get<std::size_t>());
    const Bytes& buf = g.buffers.at(bv.at("buffer").get<std::size_t>());
    const std::size_t offset = bv.value("byteOffset", std::size_t{0}) +
                               acc.value("byteOffset", std::size_t{0});
    v.stride = bv.value("byteStride", std::size_t{0});
    if (v.stride == 0) v.stride = static_cast<std::size_t>(bytes * v.components);

    const std::size_t last = v.count ? offset + (v.count - 1) * v.stride + bytes * v.components : offset;
    if (last > buf.size()) fail(g.path, "an accessor runs past the end of its buffer");
    v.data = buf.data() + offset;
    return v;
}

// Reads float vectors. Attributes must be floats.
template <int N>
std::vector<glm::vec<N, float>> readFloats(const Gltf& g, int index) {
    const View v = accessor(g, index);
    if (v.componentType != 5126 || v.components < N)
        fail(g.path, "a vertex attribute is not stored as floats");
    std::vector<glm::vec<N, float>> out(v.count);
    for (std::size_t i = 0; i < v.count; ++i)
        std::memcpy(&out[i], v.data + i * v.stride, sizeof(float) * N);
    return out;
}

std::vector<unsigned> readIndices(const Gltf& g, int index) {
    const View v = accessor(g, index);
    std::vector<unsigned> out(v.count);
    for (std::size_t i = 0; i < v.count; ++i) {
        const std::uint8_t* p = v.data + i * v.stride;
        switch (v.componentType) {
            case 5121: out[i] = *p; break;
            case 5123: { std::uint16_t s; std::memcpy(&s, p, 2); out[i] = s; break; }
            case 5125: { std::uint32_t u; std::memcpy(&u, p, 4); out[i] = u; break; }
            default: fail(g.path, "indices must be unsigned integers");
        }
    }
    return out;
}

// ---- materials --------------------------------------------------------------

std::string imagePath(const Gltf& g, const json& textureRef) {
    if (!textureRef.is_object() || !textureRef.contains("index")) return {};
    const json& tex = g.doc.at("textures").at(textureRef.at("index").get<std::size_t>());
    if (!tex.contains("source")) return {};
    const json& img = g.doc.at("images").at(tex.at("source").get<std::size_t>());
    if (!img.contains("uri")) return {};   // images inside a GLB are not supported
    return g.folder + img.at("uri").get<std::string>();
}

void readMaterial(const Gltf& g, int index, Part& part) {
    if (index < 0 || !g.doc.contains("materials")) return;
    const json& mat = g.doc.at("materials").at(static_cast<std::size_t>(index));

    if (mat.contains("pbrMetallicRoughness")) {
        const json& pbr = mat.at("pbrMetallicRoughness");
        part.albedoPath = imagePath(g, pbr.value("baseColorTexture", json()));
        if (pbr.contains("baseColorFactor")) {
            const auto f = pbr.at("baseColorFactor").get<std::vector<float>>();
            if (f.size() == 4) part.baseColour = {f[0], f[1], f[2], f[3]};
        }
    }
    // Older scans use the specular-glossiness extension instead.
    if (part.albedoPath.empty() && mat.contains("extensions")) {
        const json& ext = mat.at("extensions");
        if (ext.contains("KHR_materials_pbrSpecularGlossiness")) {
            const json& sg = ext.at("KHR_materials_pbrSpecularGlossiness");
            part.albedoPath = imagePath(g, sg.value("diffuseTexture", json()));
            if (sg.contains("diffuseFactor")) {
                const auto f = sg.at("diffuseFactor").get<std::vector<float>>();
                if (f.size() == 4) part.baseColour = {f[0], f[1], f[2], f[3]};
            }
        }
    }
    part.normalPath = imagePath(g, mat.value("normalTexture", json()));
}

// ---- nodes --------------------------------------------------------------------

glm::mat4 localMatrix(const json& node) {
    if (node.contains("matrix")) {
        const auto m = node.at("matrix").get<std::vector<float>>();
        if (m.size() == 16) return glm::make_mat4(m.data());   // column major, like glm
    }
    glm::mat4 t(1.0f), r(1.0f), s(1.0f);
    if (node.contains("translation")) {
        const auto v = node.at("translation").get<std::vector<float>>();
        t = glm::translate(glm::mat4(1.0f), glm::vec3(v[0], v[1], v[2]));
    }
    if (node.contains("rotation")) {
        const auto q = node.at("rotation").get<std::vector<float>>();   // x, y, z, w
        r = glm::mat4_cast(glm::quat(q[3], q[0], q[1], q[2]));
    }
    if (node.contains("scale")) {
        const auto v = node.at("scale").get<std::vector<float>>();
        s = glm::scale(glm::mat4(1.0f), glm::vec3(v[0], v[1], v[2]));
    }
    return t * r * s;
}

// Smooth normals from the triangles, for files that do not include any.
void computeNormals(gfx::MeshData& m, std::size_t firstVertex, std::size_t firstIndex) {
    for (std::size_t v = firstVertex; v < m.vertices.size(); ++v)
        m.vertices[v].normal = glm::vec3(0.0f);
    for (std::size_t i = firstIndex; i + 2 < m.indices.size(); i += 3) {
        gfx::Vertex& a = m.vertices[m.indices[i]];
        gfx::Vertex& b = m.vertices[m.indices[i + 1]];
        gfx::Vertex& c = m.vertices[m.indices[i + 2]];
        const glm::vec3 n = glm::cross(b.position - a.position, c.position - a.position);
        a.normal += n; b.normal += n; c.normal += n;   // larger triangles count more
    }
    for (std::size_t v = firstVertex; v < m.vertices.size(); ++v) {
        const float len = glm::length(m.vertices[v].normal);
        m.vertices[v].normal = len > 1e-12f ? m.vertices[v].normal / len : glm::vec3(0, 1, 0);
    }
}

struct Loader {
    const Gltf&          g;
    Model&               out;
    std::map<int, std::size_t> partOfMaterial;

    Part& partFor(int material) {
        auto it = partOfMaterial.find(material);
        if (it != partOfMaterial.end()) return out.parts[it->second];
        partOfMaterial[material] = out.parts.size();
        out.parts.emplace_back();
        readMaterial(g, material, out.parts.back());
        return out.parts.back();
    }

    void addPrimitive(const json& prim, const glm::mat4& world) {
        if (prim.value("mode", 4) != 4) return;   // only triangle lists
        const json& attrs = prim.at("attributes");
        if (!attrs.contains("POSITION")) return;
        if (prim.contains("extensions") &&
            prim.at("extensions").contains("KHR_draco_mesh_compression"))
            fail(g.path, "it uses Draco compression; download the uncompressed glTF instead");

        const auto pos = readFloats<3>(g, attrs.at("POSITION").get<int>());
        std::vector<glm::vec3> nrm;
        std::vector<glm::vec2> uv;
        std::vector<glm::vec4> tan;
        if (attrs.contains("NORMAL"))     nrm = readFloats<3>(g, attrs.at("NORMAL").get<int>());
        if (attrs.contains("TEXCOORD_0")) uv  = readFloats<2>(g, attrs.at("TEXCOORD_0").get<int>());
        if (attrs.contains("TANGENT"))    tan = readFloats<4>(g, attrs.at("TANGENT").get<int>());

        std::vector<unsigned> idx;
        if (prim.contains("indices")) {
            idx = readIndices(g, prim.at("indices").get<int>());
        } else {
            idx.resize(pos.size());
            for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<unsigned>(i);
        }
        for (unsigned i : idx)
            if (i >= pos.size()) fail(g.path, "an index points past the last vertex");

        Part& part = partFor(prim.value("material", -1));
        part.hasUVs      = part.hasUVs || !uv.empty();
        part.hasTangents = part.hasTangents || !tan.empty();

        gfx::MeshData& m = part.mesh;
        const std::size_t base = m.vertices.size();
        const std::size_t firstIndex = m.indices.size();
        const glm::mat3 normalM = glm::inverseTranspose(glm::mat3(world));
        const glm::mat3 linear(world);

        for (std::size_t i = 0; i < pos.size(); ++i) {
            gfx::Vertex v;
            v.position = glm::vec3(world * glm::vec4(pos[i], 1.0f));
            if (!nrm.empty()) v.normal = glm::normalize(normalM * nrm[i]);
            // Our textures are flipped on load to suit OpenGL, so flip v too.
            if (!uv.empty())  v.uv = glm::vec2(uv[i].x, 1.0f - uv[i].y);
            if (!tan.empty()) {
                const glm::vec3 t = linear * glm::vec3(tan[i]);
                if (glm::length(t) > 1e-12f) v.tangent = glm::normalize(t);
            }
            m.vertices.push_back(v);
        }

        // A mirroring transform turns triangles inside out; swap two corners back.
        const bool mirrored = glm::determinant(linear) < 0.0f;
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
            const unsigned a = static_cast<unsigned>(base) + idx[i];
            const unsigned b = static_cast<unsigned>(base) + idx[i + 1];
            const unsigned c = static_cast<unsigned>(base) + idx[i + 2];
            m.indices.push_back(a);
            m.indices.push_back(mirrored ? c : b);
            m.indices.push_back(mirrored ? b : c);
        }
        if (nrm.empty()) computeNormals(m, base, firstIndex);
    }

    void visit(int nodeIndex, const glm::mat4& parent, int depth) {
        if (depth > 64) fail(g.path, "the node tree loops back on itself");
        const json& node = g.doc.at("nodes").at(static_cast<std::size_t>(nodeIndex));
        const glm::mat4 world = parent * localMatrix(node);
        if (node.contains("mesh")) {
            const json& mesh = g.doc.at("meshes").at(node.at("mesh").get<std::size_t>());
            for (const json& prim : mesh.at("primitives")) addPrimitive(prim, world);
        }
        for (const json& child : node.value("children", json::array()))
            visit(child.get<int>(), world, depth + 1);
    }
};

} // namespace

std::size_t Model::triangleCount() const {
    std::size_t n = 0;
    for (const Part& p : parts) n += p.mesh.triangleCount();
    return n;
}

Model load(const std::string& path) {
    const Gltf g = open(path);
    Model out;
    try {
        Loader loader{g, out, {}};
        const json& doc = g.doc;
        if (doc.contains("scenes") && !doc.at("scenes").empty()) {
            const std::size_t s = doc.value("scene", std::size_t{0});
            for (const json& n : doc.at("scenes").at(s).value("nodes", json::array()))
                loader.visit(n.get<int>(), glm::mat4(1.0f), 0);
        } else if (doc.contains("nodes")) {
            for (std::size_t n = 0; n < doc.at("nodes").size(); ++n)
                loader.visit(static_cast<int>(n), glm::mat4(1.0f), 0);
        }
    } catch (const json::exception& e) {
        fail(path, std::string("a required field is missing: ") + e.what());
    }
    if (out.triangleCount() == 0) fail(path, "it has no triangles");

    glm::vec3 lo( std::numeric_limits<float>::max());
    glm::vec3 hi(-std::numeric_limits<float>::max());
    for (const Part& p : out.parts)
        for (const gfx::Vertex& v : p.mesh.vertices) {
            lo = glm::min(lo, v.position);
            hi = glm::max(hi, v.position);
        }
    out.boundsMin = lo;
    out.boundsMax = hi;
    return out;
}

glm::mat4 fitToHeight(const glm::vec3& lo, const glm::vec3& hi, float height) {
    const float h = hi.y - lo.y;
    const float s = h > 1e-6f ? height / h : 1.0f;
    const glm::vec3 foot((lo.x + hi.x) * 0.5f, lo.y, (lo.z + hi.z) * 0.5f);
    return glm::scale(glm::mat4(1.0f), glm::vec3(s)) *
           glm::translate(glm::mat4(1.0f), -foot);
}

glm::mat4 fitToHeight(const Model& m, float height) {
    return fitToHeight(m.boundsMin, m.boundsMax, height);
}

// ---- skinned characters ---------------------------------------------------------

namespace {

std::vector<glm::mat4> readMat4s(const Gltf& g, int index) {
    const View v = accessor(g, index);
    if (v.componentType != 5126 || v.components != 16)
        fail(g.path, "inverse bind matrices must be 4x4 floats");
    std::vector<glm::mat4> out(v.count);
    for (std::size_t i = 0; i < v.count; ++i)
        std::memcpy(&out[i][0][0], v.data + i * v.stride, sizeof(float) * 16);
    return out;
}

// Reads a vec4 of any component type, scaling normalised integers to 0..1.
std::vector<glm::vec4> readVec4Any(const Gltf& g, int index, bool normalise) {
    const View v = accessor(g, index);
    std::vector<glm::vec4> out(v.count, glm::vec4(0.0f));
    const int nc = std::min(v.components, 4);
    for (std::size_t i = 0; i < v.count; ++i) {
        const std::uint8_t* p = v.data + i * v.stride;
        for (int c = 0; c < nc; ++c) {
            float x = 0.0f;
            switch (v.componentType) {
                case 5126: std::memcpy(&x, p + c * 4, 4); break;
                case 5121: x = p[c] / (normalise ? 255.0f : 1.0f); break;
                case 5120: { std::int8_t s; std::memcpy(&s, p + c, 1);
                             x = normalise ? std::max(s / 127.0f, -1.0f) : s; break; }
                case 5122: { std::int16_t s; std::memcpy(&s, p + c * 2, 2);
                             x = normalise ? std::max(s / 32767.0f, -1.0f) : s; break; }
                case 5123: { std::uint16_t s; std::memcpy(&s, p + c * 2, 2);
                             x = s / (normalise ? 65535.0f : 1.0f); break; }
                case 5125: { std::uint32_t u; std::memcpy(&u, p + c * 4, 4);
                             x = static_cast<float>(u); break; }
                default: fail(g.path, "a skin attribute has an unsupported type");
            }
            out[i][c] = x;
        }
    }
    return out;
}

// The bytes of an image packed in a buffer, for .glb files.
std::vector<unsigned char> imageBytes(const Gltf& g, const json& textureRef) {
    if (!textureRef.is_object() || !textureRef.contains("index")) return {};
    const json& tex = g.doc.at("textures").at(textureRef.at("index").get<std::size_t>());
    if (!tex.contains("source")) return {};
    const json& img = g.doc.at("images").at(tex.at("source").get<std::size_t>());
    if (!img.contains("bufferView")) return {};
    const json& bv = g.doc.at("bufferViews").at(img.at("bufferView").get<std::size_t>());
    const Bytes& buf = g.buffers.at(bv.at("buffer").get<std::size_t>());
    const std::size_t off = bv.value("byteOffset", std::size_t{0});
    const std::size_t len = bv.at("byteLength").get<std::size_t>();
    if (off + len > buf.size()) fail(g.path, "a packed image runs past its buffer");
    return std::vector<unsigned char>(buf.begin() + static_cast<std::ptrdiff_t>(off),
                                      buf.begin() + static_cast<std::ptrdiff_t>(off + len));
}

skel::Skeleton readSkeleton(const Gltf& g) {
    skel::Skeleton sk;
    const json nodes = g.doc.value("nodes", json::array());
    sk.nodes.resize(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const json& n = nodes[i];
        skel::Node& out = sk.nodes[i];
        out.name = n.value("name", "node" + std::to_string(i));
        if (n.contains("matrix")) {
            // Split a matrix into move, turn and scale, so it can be animated.
            const glm::mat4 m = localMatrix(n);
            out.t = glm::vec3(m[3]);
            out.s = glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])),
                              glm::length(glm::vec3(m[2])));
            glm::mat3 r(glm::vec3(m[0]) / out.s.x, glm::vec3(m[1]) / out.s.y,
                        glm::vec3(m[2]) / out.s.z);
            out.r = glm::quat_cast(r);
        } else {
            if (n.contains("translation")) {
                const auto v = n.at("translation").get<std::vector<float>>();
                out.t = {v[0], v[1], v[2]};
            }
            if (n.contains("rotation")) {
                const auto q = n.at("rotation").get<std::vector<float>>();
                out.r = glm::quat(q[3], q[0], q[1], q[2]);
            }
            if (n.contains("scale")) {
                const auto v = n.at("scale").get<std::vector<float>>();
                out.s = {v[0], v[1], v[2]};
            }
        }
    }
    for (std::size_t i = 0; i < nodes.size(); ++i)
        for (const json& c : nodes[i].value("children", json::array()))
            sk.nodes.at(c.get<std::size_t>()).parent = static_cast<int>(i);

    // Parents before children: walk down from each root.
    std::vector<int> stack;
    for (std::size_t i = nodes.size(); i-- > 0;)
        if (sk.nodes[i].parent < 0) stack.push_back(static_cast<int>(i));
    std::vector<bool> seen(nodes.size(), false);
    while (!stack.empty()) {
        const int n = stack.back();
        stack.pop_back();
        if (seen[static_cast<std::size_t>(n)]) fail(g.path, "the node tree loops back on itself");
        seen[static_cast<std::size_t>(n)] = true;
        sk.order.push_back(n);
        const json& kids = nodes[static_cast<std::size_t>(n)].value("children", json::array());
        for (std::size_t k = kids.size(); k-- > 0;) stack.push_back(kids[k].get<int>());
    }
    if (sk.order.size() != nodes.size()) fail(g.path, "the node tree loops back on itself");

    if (g.doc.contains("skins") && !g.doc.at("skins").empty()) {
        const json& skin = g.doc.at("skins").at(0);
        for (const json& j : skin.at("joints")) sk.joints.push_back(j.get<int>());
        if (skin.contains("inverseBindMatrices"))
            sk.inverseBind = readMat4s(g, skin.at("inverseBindMatrices").get<int>());
        sk.inverseBind.resize(sk.joints.size(), glm::mat4(1.0f));
    }
    return sk;
}

std::vector<skel::Clip> readClips(const Gltf& g) {
    std::vector<skel::Clip> clips;
    const json nodes = g.doc.value("nodes", json::array());
    std::size_t index = 0;
    for (const json& a : g.doc.value("animations", json::array())) {
        skel::Clip clip;
        clip.name = a.value("name", "clip" + std::to_string(index++));
        const json& samplers = a.at("samplers");
        for (const json& ch : a.at("channels")) {
            const json& target = ch.at("target");
            if (!target.contains("node")) continue;
            const std::string path = target.at("path").get<std::string>();
            skel::Track tr;
            if      (path == "translation") tr.path = skel::Path::Translation;
            else if (path == "rotation")    tr.path = skel::Path::Rotation;
            else if (path == "scale")       tr.path = skel::Path::Scale;
            else continue;   // morph weights are not used
            const std::size_t node = target.at("node").get<std::size_t>();
            tr.node = nodes.at(node).value("name", "node" + std::to_string(node));

            const json& s = samplers.at(ch.at("sampler").get<std::size_t>());
            const std::string interp = s.value("interpolation", std::string("LINEAR"));
            tr.step = interp == "STEP";
            tr.times = [&] {
                const auto t = readFloats<1>(g, s.at("input").get<int>());
                std::vector<float> out;
                for (const auto& v : t) out.push_back(v.x);
                return out;
            }();
            std::vector<glm::vec4> values = readVec4Any(g, s.at("output").get<int>(), true);
            if (interp == "CUBICSPLINE") {
                // Each key is stored as in-tangent, value, out-tangent. Keep the values.
                std::vector<glm::vec4> mid;
                for (std::size_t k = 1; k < values.size(); k += 3) mid.push_back(values[k]);
                values = std::move(mid);
            }
            tr.values = std::move(values);
            if (!tr.times.empty()) clip.duration = std::max(clip.duration, tr.times.back());
            clip.tracks.push_back(std::move(tr));
        }
        clips.push_back(std::move(clip));
    }
    return clips;
}

} // namespace

std::size_t SkinnedModel::triangleCount() const {
    std::size_t n = 0;
    for (const SkinnedPart& p : parts) n += p.mesh.triangleCount();
    return n;
}

SkinnedModel loadSkinned(const std::string& path) {
    const Gltf g = open(path);
    SkinnedModel out;
    try {
        out.skeleton = readSkeleton(g);
        skel::Skeleton& sk = out.skeleton;
        if (sk.joints.empty()) fail(path, "it has no skin, so it cannot be animated");
        out.clips = readClips(g);

        const std::vector<glm::mat4> rest = skel::globals(sk, skel::restPose(sk));
        const std::vector<glm::mat4> restJoints = skel::jointMatrices(sk, rest);
        std::map<int, std::size_t> partOf;

        const json nodes = g.doc.value("nodes", json::array());
        for (std::size_t ni = 0; ni < nodes.size(); ++ni) {
            const json& node = nodes[ni];
            if (!node.contains("mesh")) continue;
            const bool skinned = node.contains("skin");

            // A mesh with no skin rides on the nearest joint above it.
            int rideJoint = 0;
            glm::mat4 rigid(1.0f);
            if (!skinned) {
                for (int p = static_cast<int>(ni); p >= 0; p = sk.nodes[static_cast<std::size_t>(p)].parent)
                    if (sk.jointOf(p) >= 0) { rideJoint = sk.jointOf(p); break; }
                rigid = glm::inverse(restJoints[static_cast<std::size_t>(rideJoint)]) * rest[ni];
            }

            const json& mesh = g.doc.at("meshes").at(node.at("mesh").get<std::size_t>());
            for (const json& prim : mesh.at("primitives")) {
                if (prim.value("mode", 4) != 4) continue;
                const json& at = prim.at("attributes");
                if (!at.contains("POSITION")) continue;

                const int mat = prim.value("material", -1);
                auto found = partOf.find(mat);
                if (found == partOf.end()) {
                    found = partOf.emplace(mat, out.parts.size()).first;
                    out.parts.emplace_back();
                    Part tmp;
                    readMaterial(g, mat, tmp);
                    SkinnedPart& sp = out.parts.back();
                    sp.albedoPath = tmp.albedoPath;
                    sp.baseColour = tmp.baseColour;
                    if (mat >= 0 && sp.albedoPath.empty()) {
                        const json& m = g.doc.at("materials").at(static_cast<std::size_t>(mat));
                        if (m.contains("pbrMetallicRoughness"))
                            sp.albedoBytes = imageBytes(g, m.at("pbrMetallicRoughness").value("baseColorTexture", json()));
                        if (!sp.albedoBytes.empty()) sp.albedoKey = path + "#material" + std::to_string(mat);
                    }
                }
                SkinnedPart& part = out.parts[found->second];

                const auto pos = readFloats<3>(g, at.at("POSITION").get<int>());
                std::vector<glm::vec3> nrm;
                std::vector<glm::vec2> uv;
                std::vector<glm::vec4> joints, weights;
                if (at.contains("NORMAL"))     nrm = readFloats<3>(g, at.at("NORMAL").get<int>());
                if (at.contains("TEXCOORD_0")) uv  = readFloats<2>(g, at.at("TEXCOORD_0").get<int>());
                if (skinned) {
                    if (!at.contains("JOINTS_0") || !at.contains("WEIGHTS_0"))
                        fail(path, "a skinned mesh has no joints or weights");
                    joints  = readVec4Any(g, at.at("JOINTS_0").get<int>(), false);
                    weights = readVec4Any(g, at.at("WEIGHTS_0").get<int>(), true);
                }
                std::vector<unsigned> idx;
                if (prim.contains("indices")) idx = readIndices(g, prim.at("indices").get<int>());
                else for (std::size_t i = 0; i < pos.size(); ++i) idx.push_back(static_cast<unsigned>(i));

                gfx::SkinnedMeshData& m = part.mesh;
                const unsigned base = static_cast<unsigned>(m.vertices.size());
                const glm::mat3 rigidN = glm::inverseTranspose(glm::mat3(rigid));
                for (std::size_t i = 0; i < pos.size(); ++i) {
                    gfx::SkinnedVertex v;
                    if (skinned) {
                        v.position = pos[i];
                        if (!nrm.empty()) v.normal = glm::normalize(nrm[i]);
                        const float sum = weights[i].x + weights[i].y + weights[i].z + weights[i].w;
                        v.weights = sum > 1e-6f ? weights[i] / sum : glm::vec4(1, 0, 0, 0);
                        for (int c = 0; c < 4; ++c) {
                            const float j = joints[i][c];
                            if (j < 0.0f || j >= static_cast<float>(sk.joints.size()))
                                fail(path, "a vertex names a joint the skin does not have");
                            v.joints[c] = static_cast<std::uint16_t>(j);
                        }
                    } else {
                        v.position = glm::vec3(rigid * glm::vec4(pos[i], 1.0f));
                        if (!nrm.empty()) v.normal = glm::normalize(rigidN * nrm[i]);
                        v.joints[0] = static_cast<std::uint16_t>(rideJoint);
                        v.weights = glm::vec4(1, 0, 0, 0);
                    }
                    if (!uv.empty()) v.uv = glm::vec2(uv[i].x, 1.0f - uv[i].y);
                    m.vertices.push_back(v);
                }
                for (unsigned i : idx) {
                    if (i >= pos.size()) fail(path, "an index points past the last vertex");
                    m.indices.push_back(base + i);
                }
            }
        }
        if (out.triangleCount() == 0) fail(path, "it has no triangles");

        // The size in the rest pose, for fitting to a height.
        glm::vec3 lo( std::numeric_limits<float>::max());
        glm::vec3 hi(-std::numeric_limits<float>::max());
        for (const SkinnedPart& p : out.parts)
            for (const gfx::SkinnedVertex& v : p.mesh.vertices) {
                glm::mat4 skin(0.0f);
                for (int c = 0; c < 4; ++c) skin += restJoints[v.joints[c]] * v.weights[c];
                const glm::vec3 w = glm::vec3(skin * glm::vec4(v.position, 1.0f));
                lo = glm::min(lo, w);
                hi = glm::max(hi, w);
            }
        out.boundsMin = lo;
        out.boundsMax = hi;
    } catch (const json::exception& e) {
        fail(path, std::string("a required field is missing: ") + e.what());
    }
    return out;
}

std::vector<skel::Clip> loadClips(const std::string& path) {
    const Gltf g = open(path);
    try {
        return readClips(g);
    } catch (const json::exception& e) {
        fail(path, std::string("a required field is missing: ") + e.what());
    }
}

} // namespace lumen::model
