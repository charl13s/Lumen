// main.cpp - window, render loop, and the only place that calls into every room.
//
// Owned by Shawn (integration). The flow is deliberately boring:
//
//   load the plan  ->  build the building  ->  ask each room for its pieces
//   ->  every frame: input, collision, lighting, draw, minimap
//
// Nothing here knows what is inside any room. Rooms describe; this draws.

#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/Builders.h"
#include "core/Camera.h"
#include "core/Collision.h"
#include "core/Config.h"
#include "core/Gfx.h"
#include "core/Lighting.h"
#include "core/Minimap.h"
#include "core/Plan.h"
#include "core/Room.h"

using namespace lumen;

namespace {

// One drawable thing, after upload. Rooms produce Pieces; this is what we keep.
struct Drawable {
    std::unique_ptr<gfx::Mesh> mesh;
    gfx::Material              material;
    glm::mat4                  transform{1.0f};
    std::string                roomId;   // empty = part of the building shell
};

struct App {
    GLFWwindow* window = nullptr;
    int  width = 1600, height = 900;
    bool mouseCaptured = true;
    double lastX = 0.0, lastY = 0.0;
    float  accumDX = 0.0f, accumDY = 0.0f;
    bool   firstMouse = true;
    bool   showNotes = false;
};

App g_app;

void onResize(GLFWwindow*, int w, int h) {
    g_app.width  = w;
    g_app.height = h;
    glViewport(0, 0, w, h);
}

void onMouse(GLFWwindow*, double x, double y) {
    if (g_app.firstMouse) {
        g_app.lastX = x;
        g_app.lastY = y;
        g_app.firstMouse = false;
        return;
    }
    g_app.accumDX += static_cast<float>(x - g_app.lastX);
    g_app.accumDY += static_cast<float>(y - g_app.lastY);
    g_app.lastX = x;
    g_app.lastY = y;
}

void onKey(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) {
        g_app.mouseCaptured = !g_app.mouseCaptured;
        glfwSetInputMode(w, GLFW_CURSOR,
                         g_app.mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        g_app.firstMouse = true;
    }
    if (key == GLFW_KEY_F1) g_app.showNotes = !g_app.showNotes;
}

void bindMaterial(const gfx::Shader& sh, const gfx::Material& m,
                  gfx::TextureCache& textures) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m.albedo ? m.albedo->id() : textures.white()->id());
    sh.set("uAlbedo", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m.normal ? m.normal->id() : textures.flatNormal()->id());
    sh.set("uNormalMap", 1);

    sh.set("uTint", m.tint);
    sh.set("uSpecular", m.specular);
    sh.set("uShininess", m.shininess);
    sh.set("uEmissive", m.emissive);
    sh.set("uShading", static_cast<int>(m.shading));
}

} // namespace

int main(int argc, char** argv) {
    const std::string assetDir = (argc > 1) ? std::string(argv[1]) : std::string("assets/");

    try {
        // ---- plan ----------------------------------------------------------
        Plan plan = Plan::load(assetDir + "plan.json");
        std::printf("[lumen] plan loaded: %zu rooms, %zu wall segments\n",
                    plan.rooms.size(), plan.walls.size());

        // ---- window --------------------------------------------------------
        if (!glfwInit()) {
            std::fprintf(stderr, "Failed to initialise GLFW.\n");
            return 1;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

        g_app.window = glfwCreateWindow(g_app.width, g_app.height,
                                        "LUMEN - a virtual gallery", nullptr, nullptr);
        if (!g_app.window) {
            std::fprintf(stderr, "Failed to create a window. Is your driver OpenGL 3.3+?\n");
            glfwTerminate();
            return 1;
        }
        glfwMakeContextCurrent(g_app.window);
        glfwSwapInterval(1);
        glfwSetFramebufferSizeCallback(g_app.window, onResize);
        glfwSetCursorPosCallback(g_app.window, onMouse);
        glfwSetKeyCallback(g_app.window, onKey);
        glfwSetInputMode(g_app.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            std::fprintf(stderr, "Failed to load OpenGL functions.\n");
            return 1;
        }
        std::printf("[lumen] OpenGL %s on %s\n",
                    glGetString(GL_VERSION), glGetString(GL_RENDERER));

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        // ---- resources -----------------------------------------------------
        gfx::TextureCache textures;
        auto shader = gfx::Shader::fromFiles("shaders/lumen.vert", "shaders/lumen.frag");

        std::vector<Drawable> scene;

        // The building shell. Extruded from the plan: this is the week 4-5
        // polygon work the proposal promised.
        {
            Drawable walls;
            walls.mesh = gfx::Mesh::upload(build::extrudeWalls(plan));
            walls.material.albedo   = textures.get(assetDir + "materials/plaster.jpg");
            walls.material.tint     = glm::vec3(0.88f, 0.86f, 0.82f);
            walls.material.shading  = cfg::Shading::BlinnPhong;
            walls.material.specular = 0.04f;
            std::printf("[lumen] walls: %zu triangles\n", walls.mesh->triangleCount());
            scene.push_back(std::move(walls));

            Drawable floors;
            floors.mesh = gfx::Mesh::upload(build::buildFloors(plan));
            floors.material.albedo   = textures.get(assetDir + "materials/stone.jpg");
            floors.material.shading  = cfg::Shading::BlinnPhong;
            floors.material.specular = 0.10f;
            scene.push_back(std::move(floors));

            for (const RoomPlan& r : plan.rooms) {
                Drawable vault;
                vault.mesh = gfx::Mesh::upload(
                    build::buildRoomVaults(r, plan.wallHeight, plan.vaultApex));
                vault.material.albedo   = textures.get(assetDir + "materials/concrete.jpg");
                vault.material.shading  = cfg::Shading::Lambert;
                vault.material.specular = 0.02f;
                vault.roomId = r.id;
                scene.push_back(std::move(vault));
            }
        }

        // The rooms. Each member's file, asked once for its contents.
        std::vector<RoomPtr> rooms;
        for (const RoomPlan& rp : plan.rooms) {
            RoomPtr room = makeRoom(rp.id);
            if (!room) continue;

            RoomContext ctx;
            ctx.plan     = &plan;
            ctx.me       = &rp;
            ctx.textures = &textures;
            ctx.assetDir = &assetDir;

            std::vector<Piece> pieces = room->build(ctx);
            std::printf("[lumen] %-14s (%s): %zu pieces\n",
                        rp.name.c_str(), rp.owner.c_str(), pieces.size());

            for (Piece& p : pieces) {
                Drawable d;
                d.mesh      = gfx::Mesh::upload(p.mesh);
                d.material  = p.material;
                d.transform = p.transform;
                d.roomId    = rp.id;
                scene.push_back(std::move(d));
            }
            rooms.push_back(std::move(room));
        }

        // ---- state ---------------------------------------------------------
        Collider      collider(plan.walls, plan.wallThickness);
        Camera        camera(plan.spawn(), plan.spawnHeadingDeg());
        LightingState lighting(plan);
        Minimap       minimap(plan);

        std::printf("[lumen] %zu drawables, %zu textures. WASD to walk, "
                    "mouse to look, Esc to free the cursor.\n",
                    scene.size(), textures.count());

        // ---- loop ----------------------------------------------------------
        double last = glfwGetTime();
        while (!glfwWindowShouldClose(g_app.window)) {
            const double now = glfwGetTime();
            float dt = static_cast<float>(now - last);
            last = now;
            if (dt > 0.1f) dt = 0.1f;   // a long stall must not teleport the visitor

            glfwPollEvents();

            MoveInput in;
            if (g_app.mouseCaptured) {
                in.forward = glfwGetKey(g_app.window, GLFW_KEY_W) == GLFW_PRESS;
                in.back    = glfwGetKey(g_app.window, GLFW_KEY_S) == GLFW_PRESS;
                in.left    = glfwGetKey(g_app.window, GLFW_KEY_A) == GLFW_PRESS;
                in.right   = glfwGetKey(g_app.window, GLFW_KEY_D) == GLFW_PRESS;
                in.run     = glfwGetKey(g_app.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
                in.mouseDX = g_app.accumDX;
                in.mouseDY = g_app.accumDY;
            }
            g_app.accumDX = g_app.accumDY = 0.0f;

            camera.update(in, collider, dt);

            // The reveal: which room are we in, and how far has its vault faded?
            lighting.update(camera.planPos(), dt);
            const glm::vec3 eye = camera.eye();
            lighting.lamp.follow(eye.x, eye.y, eye.z,
                                 camera.forward().x, camera.forward().y, camera.forward().z,
                                 camera.right().x, camera.right().y, camera.right().z,
                                 dt);

            for (RoomPtr& r : rooms) r->update(dt);

            // ---- draw ------------------------------------------------------
            glClearColor(0.012f, 0.010f, 0.008f, 1.0f);   // the gallery is unlit
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const float aspect = static_cast<float>(g_app.width) /
                                 static_cast<float>(g_app.height ? g_app.height : 1);

            shader->use();
            shader->set("uView", camera.view());
            shader->set("uProj", camera.projection(aspect));
            shader->set("uViewPos", eye);

            const Lamp& lamp = lighting.lamp;
            shader->set("uLampPos", glm::vec3(lamp.posX, lamp.posY, lamp.posZ));
            shader->set("uLampDir", glm::vec3(lamp.dirX, lamp.dirY, lamp.dirZ));
            shader->set("uLampColour", glm::vec3(cfg::kLampColour[0],
                                                 cfg::kLampColour[1],
                                                 cfg::kLampColour[2]));
            shader->set("uLampIntensity", lamp.intensity);
            shader->set("uLampRange", lamp.range);
            shader->set("uLampInnerCos", lamp.innerCos);
            shader->set("uLampOuterCos", lamp.outerCos);
            shader->set("uVaultColour", glm::vec3(cfg::kVaultColour[0],
                                                  cfg::kVaultColour[1],
                                                  cfg::kVaultColour[2]));
            shader->set("uAmbient", 0.012f);

            for (const Drawable& d : scene) {
                // A piece belonging to a room gets that room's vault level; the
                // corridor and the shell get nothing, because nothing out there
                // is lit except by the lamp.
                const float vault = d.roomId.empty() ? 0.0f : lighting.levelFor(d.roomId);
                shader->set("uVaultLevel", vault);

                shader->set("uModel", d.transform);
                shader->set("uNormalMatrix",
                            glm::mat3(glm::inverseTranspose(glm::mat3(d.transform))));
                bindMaterial(*shader, d.material, textures);
                if (d.mesh) d.mesh->draw();
            }

            // The minimap, rasterised by hand on the CPU. See core/Raster.h.
            minimap.redraw(camera.planPos(), camera.headingDeg(), lighting.currentRoomId());
            minimap.present(g_app.width, g_app.height);

            glfwSwapBuffers(g_app.window);
        }

        if (g_app.showNotes)
            for (RoomPtr& r : rooms) std::printf("%s\n", r->techniqueNotes().c_str());

        scene.clear();
        rooms.clear();
        glfwDestroyWindow(g_app.window);
        glfwTerminate();
        return 0;

    } catch (const std::exception& e) {
        std::fprintf(stderr, "\n[lumen] %s\n", e.what());
        if (g_app.window) glfwDestroyWindow(g_app.window);
        glfwTerminate();
        return 1;
    }
}
