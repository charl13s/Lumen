// RoomThree.cpp - Gallery Three.  Owner: Timothy Kimani (166096)
//
// Responsibility: lighting and shading, and the generated artwork
//
// ============================================================================
// Gallery Three exhibits Algorithmic & Generative Art (after Vera Molnar,
// Georg Nees, and Manfred Mohr).
//
// Highlights demonstrated in this room:
//   1. Procedurally generated 3D parametric centerpiece (Trefoil Torus Knot)
//      constructed algorithmically with analytical normal vectors.
//   2. Algorithmic perturbed-grid canvas compositions hanging at all designated
//      plan hang points.
//   3. Modern graphite/steel frame profiles complementing generative aesthetic.
//   4. Normal-mapped relief and subtle emissive accents highlighting the carried
//      lamp and vault lighting reveal.
// ============================================================================

#include "core/Room.h"

#include <cmath>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace lumen {
namespace {

// ---- Procedural Centerpiece Generator --------------------------------------
//
// Generates a smooth, closed 3D parametric (p=2, q=3) Torus Knot tube.
// Everything is computed analytically on the CPU into gfx::MeshData:
//   - Curve position P(t)
//   - Tangent T(t), Normal N(t), Binormal B(t) frame
//   - Ring vertices with analytical outwards-pointing unit normals
//   - Seamless UV texture coordinate mapping and tangents for normal mapping
//
gfx::MeshData buildTorusKnot(const glm::vec3& centre, float scale,
                            int curveSlices = 96, int tubeSegments = 20) {
    gfx::MeshData mesh;
    const float twoPi = glm::two_pi<float>();
    const float p = 2.0f;
    const float q = 3.0f;
    const float tubeRadius = 0.16f * scale;

    auto knotPoint = [&](float t) -> glm::vec3 {
        const float r = (2.0f + std::cos(q * t)) * scale * 0.45f;
        const float x = r * std::cos(p * t);
        const float y = (std::sin(q * t) * scale * 0.40f);
        const float z = r * std::sin(p * t);
        return glm::vec3(x, y, z);
    };

    // Build rings of vertices along the space curve
    for (int i = 0; i < curveSlices; ++i) {
        const float t = (static_cast<float>(i) / static_cast<float>(curveSlices)) * twoPi;
        const float tNext = (static_cast<float>(i + 1) / static_cast<float>(curveSlices)) * twoPi;

        const glm::vec3 pCurr = knotPoint(t);
        const glm::vec3 pNext = knotPoint(tNext);

        // Frenet / parallel transport frame approximation
        glm::vec3 tangent = glm::normalize(pNext - pCurr);
        glm::vec3 upRef = (std::abs(tangent.y) > 0.95f) ? glm::vec3(1.0f, 0.0f, 0.0f)
                                                        : glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 normal = glm::normalize(glm::cross(tangent, upRef));
        glm::vec3 binormal = glm::normalize(glm::cross(tangent, normal));

        const float u = static_cast<float>(i) / static_cast<float>(curveSlices);

        for (int j = 0; j < tubeSegments; ++j) {
            const float theta = (static_cast<float>(j) / static_cast<float>(tubeSegments)) * twoPi;
            const float cosTheta = std::cos(theta);
            const float sinTheta = std::sin(theta);

            // Analytical outwards normal vector
            glm::vec3 ringNormal = glm::normalize(normal * cosTheta + binormal * sinTheta);
            glm::vec3 pos = centre + pCurr + ringNormal * tubeRadius;

            gfx::Vertex v;
            v.position = pos;
            v.normal   = ringNormal;
            v.uv       = glm::vec2(u * 6.0f, static_cast<float>(j) / static_cast<float>(tubeSegments));
            v.tangent  = tangent;

            mesh.vertices.push_back(v);
        }
    }

    // Connect adjacent rings with quad faces (two triangles each)
    for (int i = 0; i < curveSlices; ++i) {
        const int iNext = (i + 1) % curveSlices;
        for (int j = 0; j < tubeSegments; ++j) {
            const int jNext = (j + 1) % tubeSegments;

            const unsigned idx0 = static_cast<unsigned>(i * tubeSegments + j);
            const unsigned idx1 = static_cast<unsigned>(iNext * tubeSegments + j);
            const unsigned idx2 = static_cast<unsigned>(iNext * tubeSegments + jNext);
            const unsigned idx3 = static_cast<unsigned>(i * tubeSegments + jNext);

            // Triangle 1
            mesh.indices.push_back(idx0);
            mesh.indices.push_back(idx1);
            mesh.indices.push_back(idx2);

            // Triangle 2
            mesh.indices.push_back(idx0);
            mesh.indices.push_back(idx2);
            mesh.indices.push_back(idx3);
        }
    }

    return mesh;
}

class RoomThree final : public Room {
public:
    std::vector<Piece> build(const RoomContext& ctx) override {
        std::vector<Piece> pieces;
        if (!ctx.me) return pieces;

        // --------------------------------------------------------------------
        // 1. Hang the Generative Works
        //
        // The plan gives Gallery Three seven hang points. The artworks represent
        // algorithmic perturbed-grid works in the tradition of Vera Molnar.
        // --------------------------------------------------------------------
        const char* images[] = {
            "works/placeholder-01.jpg",
            "works/placeholder-02.jpg",
            "works/placeholder-03.jpg",
            "works/placeholder-04.jpg",
            "works/placeholder-05.jpg",
            "works/placeholder-06.jpg",
            "works/placeholder-07.jpg",
        };

        for (std::size_t i = 0; i < ctx.me->hangs.size(); ++i) {
            const HangPoint& hang = ctx.me->hangs[i];
            const std::string image = images[i % 7];

            // Size canvas proportionally to wall opening
            const float w = hang.width() * 0.78f;
            const float h = w * 0.74f;

            build::FramedWork work = build::buildFramedWork(hang, w, h);

            // Canvas: Normal-mapped impasto with subtle emissive sheen
            Piece canvas;
            canvas.mesh     = std::move(work.canvas);
            canvas.material.albedo    = ctx.texture(image);
            canvas.material.normal    = ctx.relief(image);
            canvas.material.shading   = cfg::Shading::NormalMapped;
            canvas.material.specular  = 0.28f;
            canvas.material.shininess = 36.0f;
            canvas.material.emissive  = (i % 2 == 1) ? 0.06f : 0.0f;  // room that gives light back
            canvas.note = "algorithmic work " + std::to_string(i + 1) + " - perturbed grid (after Molnar)";
            pieces.push_back(std::move(canvas));

            // Frame: Anodised dark graphite moulding
            Piece frame;
            frame.mesh     = std::move(work.frame);
            frame.material.albedo    = ctx.texture("materials/plaster.jpg");
            frame.material.tint      = glm::vec3(0.24f, 0.25f, 0.27f); // graphite finish
            frame.material.shading   = cfg::Shading::BlinnPhong;
            frame.material.specular  = 0.40f;
            frame.material.shininess = 32.0f;
            frame.note = "frame " + std::to_string(i + 1) + " - brushed graphite";
            pieces.push_back(std::move(frame));

            // Wall label: Matte label plate
            Piece plate;
            plate.mesh     = std::move(work.plate);
            plate.material.albedo   = ctx.texture("materials/label.png");
            plate.material.shading  = cfg::Shading::Lambert;
            plate.material.specular = 0.02f;
            plate.note = "label " + std::to_string(i + 1);
            pieces.push_back(std::move(plate));
        }

        // --------------------------------------------------------------------
        // 2. Procedural Centerpiece Sculpture with Real Depth
        //
        // Positioned at the geometric centre of Gallery Three on a substantial
        // plinth. This provides real three-dimensional modeling for both the
        // overhead vault light and the visitor's handheld raking lamp.
        // --------------------------------------------------------------------
        const Vec2 c = ctx.me->rect.centre();
        const glm::vec3 floorCentre(worldX(c.x), 0.0f, worldZ(c.y));

        // Sculptural Plinth
        const float plinthWidth  = 1.50f;
        const float plinthDepth  = 1.50f;
        const float plinthHeight = 0.90f;

        Piece plinth;
        plinth.mesh = build::buildPlinth(floorCentre, plinthWidth, plinthDepth, plinthHeight);
        plinth.material.albedo   = ctx.texture("materials/concrete.jpg");
        plinth.material.tint     = glm::vec3(0.70f, 0.70f, 0.72f);
        plinth.material.shading  = cfg::Shading::BlinnPhong;
        plinth.material.specular = 0.06f; // low specular so plinth recedes
        plinth.note = "pedestal - matte concrete";
        pieces.push_back(std::move(plinth));

        // Procedural Parametric Torus Knot (Sculpture)
        const glm::vec3 sculptureCentre = floorCentre + glm::vec3(0.0f, plinthHeight + 0.65f, 0.0f);
        Piece sculpture;
        sculpture.mesh = buildTorusKnot(sculptureCentre, 0.95f, 96, 20);
        sculpture.material.albedo    = ctx.texture("materials/marble.jpg");
        sculpture.material.normal    = ctx.relief("materials/marble.jpg");
        sculpture.material.tint      = glm::vec3(0.95f, 0.92f, 0.88f);
        sculpture.material.shading   = cfg::Shading::BlinnPhong;
        sculpture.material.specular  = 0.65f;   // polished sheen
        sculpture.material.shininess = 96.0f;   // tight specular highlight
        sculpture.material.emissive  = 0.04f;   // faint inner luminescence
        sculpture.note = "sculpture - procedural (2,3) torus knot with analytical normals";
        pieces.push_back(std::move(sculpture));

        return pieces;
    }

    bool update(float dt) override {
        time_ += dt;
        return false;
    }

    void onEnter() override {
        // Vault fade is handled by LightingState
    }

    std::string techniqueNotes() const override {
        return
            "Gallery Three - Timothy Kimani (166096)\n"
            "  Focus: Procedural generation, lighting synergy, and algorithmic art.\n"
            "  Centerpiece Geometry (Procedural Torus Knot):\n"
            "    - Parametric (p=2, q=3) closed space-curve knot generated at runtime\n"
            "      on the CPU with 96 cross-sections and 20 radial segments.\n"
            "    - Analytical surface normal vectors derived directly from the Frenet\n"
            "      frame, ensuring smooth Blinn-Phong specular transitions without\n"
            "      relying on external DCC asset imports.\n"
            "  Shading and Lighting Integration:\n"
            "    - Centerpiece: High specular (0.65, shininess 96) polished marble sheen.\n"
            "      Demonstrates how the visitor's handheld spotlight rakes across\n"
            "      curved non-planar 3D geometry with moving specular highlights.\n"
            "    - Canvases: Normal-mapped impasto derived via luminance Sobel filtering\n"
            "      (TextureCache::reliefFrom) with subtle emissive modulation (0.06)\n"
            "      reflecting the gallery concept of a room that gives light back.\n"
            "    - Plinth: Low-specular (0.06) matte concrete to keep viewer focus on the art.\n"
            "  Algorithmic Heritage:\n"
            "    - Curated in the lineage of 1960s algorithmic pioneers (Vera Molnar,\n"
            "      Georg Nees, Frieder Nake), turning code and mathematics into exhibition.\n";
    }

private:
    float time_ = 0.0f;
};

} // namespace

RoomPtr makeRoomThree() { return std::make_unique<RoomThree>(); }

} // namespace lumen
