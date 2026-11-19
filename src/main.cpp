// main.cpp - the gallery: window, render loop and rooms. Owner: Shawn.
//
// Load the plan, build the building, ask each room for its pieces, then every
// frame: input, collision, lighting, shadow map, draw, minimap.
//
//   lumen.exe [assets/]
//   lumen.exe --shot out.bmp X Y HEADING [PITCH] [--light N] [--no-shadows]
//             [--intro-time T]   (a shot skips the intro, unless T seconds of it are asked for)
//   lumen.exe --no-intro          starts with the lamp already in hand
//       renders one settled frame from plan position (X, Y) and saves it.

#include <algorithm>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

// Stops GLFW including its own OpenGL header, which clashes with glad.
// CMakeLists.txt already sets this; the guard avoids defining it twice.
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/Builders.h"
#include "core/Character.h"
#include "core/Camera.h"
#include "core/CanvasView.h"
#include "core/Collision.h"
#include "core/Config.h"
#include "core/Demo.h"
#include "core/Font.h"
#include "core/Gfx.h"
#include "core/Image.h"
#include "core/Intro.h"
#include "core/Lighting.h"
#include "core/Minimap.h"
#include "core/Plan.h"
#include "core/Room.h"
#include "core/Sculpture.h"
#include "core/Tour.h"

using namespace lumen;

namespace {

// One uploaded thing to draw.
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

    // F2 shows the algorithm demo.
    bool   showDemo  = false;
    int    demoPage  = 0;

    // T starts the guided tour.
    bool   tourOn    = false;

    // L cycles the lighting modes in Config.h.
    int    lightMode = cfg::kDefaultLightingMode;

    // K turns the lamp's shadows on and off, to compare.
    bool   shadows   = true;

    // F3 shows who made the statues and reliefs.
    bool   showCredits = false;

    // E takes the lamp from the torch bearer. Set by the key, used once.
    bool   takePressed = false;
    bool   intro       = true;
};

// A screenshot request from the command line.
struct Shot {
    std::string path;
    Vec2  at{0.0f, 0.0f};
    float heading = 0.0f;
    float pitch   = 0.0f;
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

    if (key == GLFW_KEY_F2) {
        g_app.showDemo = !g_app.showDemo;
        // Free the mouse while the demo is open.
        g_app.mouseCaptured = !g_app.showDemo;
        glfwSetInputMode(w, GLFW_CURSOR,
                         g_app.mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        g_app.firstMouse = true;
    }
    if (g_app.showDemo) {
        if (key == GLFW_KEY_RIGHT)
            g_app.demoPage = (g_app.demoPage + 1) % lumen::demo::kPageCount;
        if (key == GLFW_KEY_LEFT)
            g_app.demoPage = (g_app.demoPage + lumen::demo::kPageCount - 1)
                             % lumen::demo::kPageCount;
    }

    if (key == GLFW_KEY_F3) g_app.showCredits = !g_app.showCredits;
    if (key == GLFW_KEY_E)  g_app.takePressed = true;
    if (key == GLFW_KEY_T) g_app.tourOn = !g_app.tourOn;
    if (key == GLFW_KEY_K) g_app.shadows = !g_app.shadows;

    if (key == GLFW_KEY_L) {
        g_app.lightMode = (g_app.lightMode + 1) % cfg::kLightingPresetCount;
        std::printf("[lumen] lighting: %s\n",
                    cfg::kLightingPresets[g_app.lightMode].name);
        std::fflush(stdout);
    }
}

// Status strip, top left: lighting mode, room, position, FPS.
// Drawn with our own bitmap font, like the minimap.
void drawHud(raster::Canvas& c, const cfg::LightingPreset& preset,
             const std::string& room, Vec2 pos, bool touring, float fps, bool shadows) {
    using raster::Colour;
    const Colour bg  {20, 17, 13, 190};
    const Colour gold{242, 212, 121, 255};
    const Colour dim {154, 143, 114, 255};

    c.clear(bg);

    char line[128];
    std::snprintf(line, sizeof(line), "LIGHTING: %s", preset.name);
    font::drawText(c, 10, 8, line, gold, 2);

    std::snprintf(line, sizeof(line), "REVEAL %.2f   AMBIENT %.3f   LAMP %.2fX",
                  preset.reveal, preset.ambient, preset.lampScale);
    font::drawText(c, 10, 30, line, dim, 1);

    std::snprintf(line, sizeof(line), "ROOM: %s   SHADOWS %s",
                  room.empty() ? "CORRIDOR" : room.c_str(), shadows ? "ON" : "OFF");
    font::drawText(c, 10, 46, line, dim, 1);

    std::snprintf(line, sizeof(line), "AT %.1f, %.1f M   %.0f FPS%s",
                  pos.x, pos.y, fps, touring ? "   TOUR" : "");
    font::drawText(c, 10, 62, line, dim, 1);

    font::drawText(c, 10, 82, "L LIGHT  K SHADOWS  T TOUR  F2 ALGORITHMS  F3 CREDITS",
                   dim, 1);
}

// The credits screen: every statue and relief, with the credit its licence asks for.
void drawCredits(raster::Canvas& c, const sculpture::Catalogue& cat,
                 const std::string& bearerCredit) {
    using raster::Colour;
    const Colour bg  {14, 12, 9, 235};
    const Colour gold{242, 212, 121, 255};
    const Colour text{214, 204, 178, 255};
    const Colour dim {140, 130, 104, 255};
    c.clear(bg);

    int y = 40;
    font::drawTextCentred(c, c.width() / 2, y, "CREDITS", gold, 4);
    y += 56;
    auto section = [&](const char* name) {
        font::drawText(c, 80, y, name, gold, 2);
        y += 26;
    };
    auto entry = [&](const std::string& title, const std::string& credit) {
        font::drawText(c, 100, y, title, text, 2);
        y += 18;
        font::drawText(c, 120, y, credit, dim, 2);
        y += 24;
    };
    section("STATUES");
    for (const auto& s : cat.statues) entry(s.title, s.credit);
    y += 8;
    section("RELIEFS");
    for (const auto& r : cat.reliefs) entry(r.title, r.credit);
    if (!bearerCredit.empty()) {
        y += 8;
        section("THE TORCH BEARER");
        font::drawText(c, 120, y, bearerCredit, dim, 2);
    }
    font::drawTextCentred(c, c.width() / 2, c.height() - 30, "F3 TO CLOSE", dim, 2);
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
    sh.set("uTriplanarScale", m.triplanarScale);
    sh.set("uShading", static_cast<int>(m.shading));
}

// Draws the animated character: the shader bends each vertex by its bones.
void drawBearer(const gfx::Shader& sh, const Character& c, gfx::TextureCache& textures) {
    sh.set("uSkinned", 1);
    sh.set("uJoints", c.joints());
    sh.set("uModel", c.model());
    sh.set("uNormalMatrix", glm::mat3(glm::inverseTranspose(glm::mat3(c.model()))));
    for (const Character::DrawPart& p : c.parts()) {
        bindMaterial(sh, p.material, textures);
        p.mesh->draw();
    }
    sh.set("uSkinned", 0);
}

// Reads the finished frame back and saves it.
bool saveFrame(const std::string& path, int w, int h) {
    std::vector<unsigned char> px(static_cast<std::size_t>(w) * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    raster::Canvas c(w, h);
    for (int y = 0; y < h; ++y)          // OpenGL rows start at the bottom
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = &px[(static_cast<std::size_t>(h - 1 - y) * w + x) * 4];
            c.set(x, y, raster::Colour{p[0], p[1], p[2], 255});
        }
    return image::writeBmp(c, path);
}

} // namespace

int main(int argc, char** argv) {
    std::string assetDir = "assets/";
    Shot shot;
    float introTime = -1.0f;   // for shots: how far into the intro to show
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--shot" && i + 4 < argc) {
            shot.path    = argv[++i];
            shot.at.x    = std::stof(argv[++i]);
            shot.at.y    = std::stof(argv[++i]);
            shot.heading = std::stof(argv[++i]);
            if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0)
                shot.pitch = std::stof(argv[++i]);
        } else if (arg == "--light" && i + 1 < argc) {
            g_app.lightMode = std::stoi(argv[++i]) % cfg::kLightingPresetCount;
        } else if (arg == "--no-intro") {
            g_app.intro = false;
        } else if (arg == "--intro-time" && i + 1 < argc) {
            introTime = std::stof(argv[++i]);
        } else if (arg == "--no-shadows") {
            g_app.shadows = false;
        } else {
            assetDir = arg;
        }
    }
    const bool shooting = !shot.path.empty();

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
        glfwSwapInterval(shooting ? 0 : 1);   // a shot runs flat out, so it can time the frames
        glfwSetFramebufferSizeCallback(g_app.window, onResize);
        glfwSetCursorPosCallback(g_app.window, onMouse);
        glfwSetKeyCallback(g_app.window, onKey);
        if (!shooting) glfwSetInputMode(g_app.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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

        gfx::ShadowMap shadowMap;
        const bool haveShadows = shadowMap.create(cfg::kShadowMapSize);

        std::vector<Drawable> scene;

        // The building, extruded from the plan.
        {
            // Inside the galleries the walls stay plain plaster, for each room
            // to dress as it likes. The shared spaces are clad in limestone.
            gfx::MeshData roomWalls, sharedWalls;
            build::splitWallsByRoom(build::extrudeWalls(plan), plan, roomWalls, sharedWalls);
            // The texture is two blocks by two.
            build::projectUVs(sharedWalls, glm::vec2(cfg::kStoneBlockW, cfg::kStoneBlockH) * 2.0f);

            Drawable walls;
            walls.mesh = gfx::Mesh::upload(roomWalls);
            walls.material.albedo   = textures.get(assetDir + "materials/plaster.jpg");
            walls.material.tint     = glm::vec3(0.88f, 0.86f, 0.82f);
            walls.material.shading  = cfg::Shading::BlinnPhong;
            walls.material.specular = 0.04f;
            std::printf("[lumen] walls: %zu triangles in rooms, %zu shared\n",
                        roomWalls.triangleCount(), sharedWalls.triangleCount());
            scene.push_back(std::move(walls));

            Drawable stone;
            stone.mesh = gfx::Mesh::upload(sharedWalls);
            stone.material.albedo   = textures.get(assetDir + "materials/stone.jpg");
            stone.material.tint     = glm::vec3(cfg::kStoneTint[0], cfg::kStoneTint[1], cfg::kStoneTint[2]);
            stone.material.shading  = cfg::Shading::Lambert;   // honed, no shine
            stone.material.specular = 0.03f;
            scene.push_back(std::move(stone));

            Drawable floors;
            floors.mesh = gfx::Mesh::upload(build::buildFloors(plan));
            floors.material.albedo   = textures.get(assetDir + "materials/stone.jpg");
            floors.material.shading  = cfg::Shading::BlinnPhong;
            floors.material.specular = 0.10f;
            scene.push_back(std::move(floors));

            // Ceilings over the corridor, and the atrium's, open to the sky.
            Drawable ceilings;
            // The same stone as the walls, paler, so the ceiling does not
            // stand out against them.
            const glm::vec3 ceilingTint(cfg::kCeilingTint[0], cfg::kCeilingTint[1], cfg::kCeilingTint[2]);
            gfx::MeshData ceilingMesh = build::buildCeilings(plan);
            build::projectUVs(ceilingMesh, glm::vec2(cfg::kStoneBlockW * 2.0f));
            ceilings.mesh = gfx::Mesh::upload(ceilingMesh);
            ceilings.material.albedo   = textures.get(assetDir + "materials/stone.jpg");
            ceilings.material.tint     = ceilingTint;
            ceilings.material.shading  = cfg::Shading::Lambert;
            scene.push_back(std::move(ceilings));

            Drawable oculus;
            gfx::MeshData oculusMesh = build::buildOculusCeiling(plan, cfg::kOculusRim);
            build::projectUVs(oculusMesh, glm::vec2(cfg::kStoneBlockW * 2.0f));
            oculus.mesh = gfx::Mesh::upload(oculusMesh);
            oculus.material.albedo  = textures.get(assetDir + "materials/stone.jpg");
            oculus.material.tint    = ceilingTint;
            oculus.material.shading = cfg::Shading::Lambert;
            oculus.roomId = "ATRIUM";
            scene.push_back(std::move(oculus));

            Drawable sky;
            sky.mesh = gfx::Mesh::upload(build::buildDisc(
                glm::vec3(worldX(plan.oculusCentre.x), plan.wallHeight + cfg::kOculusRim + 0.01f,
                          worldZ(plan.oculusCentre.y)),
                plan.oculusRadius + 0.05f, true, 96));
            sky.material.tint     = glm::vec3(0.82f, 0.90f, 1.00f);
            sky.material.shading  = cfg::Shading::Emissive;
            sky.material.emissive = cfg::kSkyGlow;
            sky.material.specular = 0.0f;
            scene.push_back(std::move(sky));

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

        // The statues and reliefs in the corridor and atrium.
        sculpture::Catalogue sculptures;
        try {
            sculptures = sculpture::load(assetDir + "sculpture.json");
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[lumen] no sculpture: %s\n", e.what());
        }
        for (sculpture::Item& it : sculpture::build(plan, sculptures, assetDir, textures)) {
            Drawable d;
            d.mesh      = gfx::Mesh::upload(it.mesh);
            d.material  = it.material;
            d.transform = it.transform;
            d.roomId    = it.zone;
            scene.push_back(std::move(d));
        }

        // Ask each room for its pieces.
        std::vector<RoomPtr> rooms;
        std::vector<std::string> roomIds;                  // the plan id of each room
        std::vector<std::vector<std::size_t>> roomPieces;  // where each room's pieces are in scene
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

            std::vector<std::size_t> where;
            for (Piece& p : pieces) {
                Drawable d;
                d.mesh      = gfx::Mesh::upload(p.mesh);
                d.material  = p.material;
                d.transform = p.transform;
                d.roomId    = rp.id;
                where.push_back(scene.size());
                scene.push_back(std::move(d));
            }
            rooms.push_back(std::move(room));
            roomIds.push_back(rp.id);
            roomPieces.push_back(std::move(where));
        }

        // ---- state ---------------------------------------------------------
        // The walls, plus the plinths so nobody walks through a statue, plus
        // whatever each room says is solid. Collision walls are thick, so each
        // box's outline is pulled in by half a wall to keep its true size.
        std::vector<Segment> solid = plan.walls;
        for (const Segment& s : sculpture::plinthOutlines(plan, sculptures)) solid.push_back(s);
        for (const RoomPtr& room : rooms)
            for (const Footprint& f : room->footprints()) {
                const float inset = plan.wallThickness * 0.5f;
                const float hx = std::max(0.05f, f.half.x - inset), hy = std::max(0.05f, f.half.y - inset);
                const Vec2 a{f.centre.x - hx, f.centre.y - hy}, b{f.centre.x + hx, f.centre.y - hy},
                           c{f.centre.x + hx, f.centre.y + hy}, d{f.centre.x - hx, f.centre.y + hy};
                solid.push_back({a, b});
                solid.push_back({b, c});
                solid.push_back({c, d});
                solid.push_back({d, a});
            }
        Collider        collider(solid, plan.wallThickness);

        // ---- the torch bearer ------------------------------------------------
        BearerSettings bearerSet;
        Character      bearer;
        std::string    why;
        bool haveBearer = loadBearerSettings(assetDir + "intro.json", bearerSet, why);
        if (!haveBearer)
            std::fprintf(stderr, "[lumen] no torch bearer: %s\n", why.c_str());
        else
            haveBearer = bearer.load(assetDir + bearerSet.character,
                                     bearerSet.animations.empty() ? "" : assetDir + bearerSet.animations,
                                     bearerSet.height, textures);

        // The clips for each part of the story, by the names in intro.json.
        std::string clipIdle, clipWalk, clipGive, clipIdleTorch;
        int handNode = -1, gripNode = -1;
        if (haveBearer) {
            clipIdle = bearer.findClip(bearerSet.idle);
            clipWalk = bearer.findClip(bearerSet.walk);
            clipGive = bearer.findClip(bearerSet.give);
            clipIdleTorch = bearer.findClip(bearerSet.idleTorch);
            const std::vector<std::string> names = bearer.clipNames();
            if (clipIdle.empty() || clipWalk.empty() || clipGive.empty()) {
                std::fprintf(stderr, "[lumen] intro.json names a clip that is not there. The clips are:\n");
                for (const std::string& n : names) std::fprintf(stderr, "         %s\n", n.c_str());
            }
            if (clipIdle.empty()) clipIdle = names.front();
            if (clipWalk.empty()) clipWalk = clipIdle;
            if (clipGive.empty()) clipGive = clipIdle;
            if (clipIdleTorch.empty()) clipIdleTorch = clipIdle;
            handNode = bearer.findNode(bearerSet.hand);
            if (!bearerSet.grip.empty()) gripNode = bearer.findNode(bearerSet.grip);
            if (handNode < 0)
                std::fprintf(stderr, "[lumen] no bone called '%s'; the torch is held in front.\n",
                             bearerSet.hand.c_str());
            std::printf("[lumen] torch bearer: %zu triangles, clips %s / %s / %s\n",
                        bearer.triangleCount(), clipIdle.c_str(), clipWalk.c_str(), clipGive.c_str());
        }

        Intro intro(plan, plan.spawn());
        intro.setWalkSpeed(bearerSet.walkSpeed);

        // The statue's plinth, a low step beside the Apollo.
        if (haveBearer) {
            const Vec2 post = intro.path().front();
            Drawable plinth;
            plinth.mesh = gfx::Mesh::upload(build::buildPlinth(
                glm::vec3(worldX(post.x), 0.0f, worldZ(post.y)),
                cfg::kBearerPlinthSize, cfg::kBearerPlinthSize, cfg::kBearerPlinthHeight));
            plinth.material.albedo    = textures.get(assetDir + "materials/marble.jpg");
            plinth.material.tint      = glm::vec3(0.50f, 0.48f, 0.45f);
            plinth.material.specular  = 0.20f;
            plinth.material.shininess = 48.0f;
            plinth.roomId = "ATRIUM";
            scene.push_back(std::move(plinth));

            // Nobody walks through it. Collision walls are thick, so pull the outline in.
            const float half = cfg::kBearerPlinthSize * 0.5f - plan.wallThickness * 0.5f;
            const Vec2 a{post.x - half, post.y - half}, b{post.x + half, post.y - half},
                       cc{post.x + half, post.y + half}, d{post.x - half, post.y + half};
            solid.push_back({a, b});
            solid.push_back({b, cc});
            solid.push_back({cc, d});
            solid.push_back({d, a});
            collider = Collider(solid, plan.wallThickness);

            // A living statue: marble that follows the body as it moves.
            if (bearerSet.finish == "marble") {
                gfx::Material stone;
                stone.albedo    = textures.get(assetDir + "materials/marble.jpg");
                stone.tint      = glm::vec3(0.95f, 0.93f, 0.89f);
                stone.specular  = 0.35f;
                stone.shininess = 60.0f;
                bearer.setMaterial(stone);
            }
        }
        if (!haveBearer || !g_app.intro || (shooting && introTime < 0.0f)) intro.skip();

        // The torch: a wooden handle, a brass cup and the flame, built round its own base.
        Drawable torchHandle, torchFlame;
        {
            gfx::MeshData handle = build::buildCylinder(glm::vec3(0.0f, -0.22f, 0.0f), 0.022f, 0.48f, 12);
            handle.append(build::buildCylinder(glm::vec3(0.0f, 0.26f, 0.0f), 0.05f, 0.06f, 16),
                          glm::mat4(1.0f));
            torchHandle.mesh = gfx::Mesh::upload(handle);
            torchHandle.material.tint      = glm::vec3(0.36f, 0.24f, 0.13f);
            torchHandle.material.specular  = 0.3f;
            torchFlame.mesh = gfx::Mesh::upload(
                build::buildCone(glm::vec3(0.0f, 0.31f, 0.0f), 0.045f, 0.16f, 16));
            torchFlame.material.tint     = glm::vec3(1.0f, 0.38f, 0.06f);
            torchFlame.material.shading  = cfg::Shading::Emissive;
            torchFlame.material.emissive = cfg::kTorchFlameGlow;
        }

        // Moves the story on and poses the bearer to match.
        auto advanceIntro = [&](float step, bool take) {
            if (!haveBearer) return;
            intro.update(step, take);
            bearer.place(intro.position(), intro.headingDeg(), intro.elevation());
            const std::string role = intro.clip();
            if (role == "walk")      bearer.play(clipWalk, true);
            else if (role == "give") bearer.play(clipGive, false, 0.3f, bearerSet.giveHold);
            else if (role == "idle_torch") bearer.play(clipIdleTorch, true);
            else                     bearer.play(clipIdle, true);
            // Stone does not move: hold the pose until it wakes.
            bearer.update(intro.frozen() ? 0.0f : step);
        };
        // Where the torch is: in the hand, or held out in front if there is no hand bone.
        auto torchBase = [&]() {
            // In the palm: part way from the wrist to the knuckles, if there is a knuckle bone.
            if (handNode >= 0 && gripNode >= 0)
                return glm::mix(bearer.nodeWorld(handNode), bearer.nodeWorld(gripNode), 0.55f);
            if (handNode >= 0) return bearer.nodeWorld(handNode);
            const float h = glm::radians(intro.headingDeg());
            const Vec2 p = intro.position();
            return glm::vec3(worldX(p.x) + std::sin(h) * 0.35f, 1.05f, worldZ(p.y) - std::cos(h) * 0.35f);
        };
        if (haveBearer) {
            advanceIntro(0.0f, false);
            if (shooting && introTime > 0.0f)
                for (float t = 0.0f; t < introTime; t += 1.0f / 30.0f) advanceIntro(1.0f / 30.0f, false);
        }
        Camera          camera(plan.spawn(), plan.spawnHeadingDeg());
        LightingState   lighting(plan);
        Minimap         minimap(plan);
        TourCamera      tour(plan);
        gfx::CanvasView demoView;
        raster::Canvas  demoCanvas(cfg::kDemoW, cfg::kDemoH);
        gfx::CanvasView hudView;
        raster::Canvas  hudCanvas(360, 100);
        gfx::CanvasView promptView;
        raster::Canvas  promptCanvas(380, 38);
        float           fps = 0.0f;

        if (shooting) {
            camera.teleport(shot.at, shot.heading);
            camera.setPitch(shot.pitch);
        }
        int frame = 0;
        std::string visitorRoom;   // the room the visitor was in last frame, "" in the corridor

        std::printf("[lumen] tour route: %zu spline points, %.1f m around\n",
                    tour.path().size(), tour.totalLength());
        std::printf("[lumen] lighting: %s\n",
                    cfg::kLightingPresets[g_app.lightMode].name);

        std::printf("[lumen] %zu drawables, %zu textures.\n"
                    "         WASD walk, mouse look, Shift run\n"
                    "         T    guided tour along the spline route\n"
                    "         F2   algorithm demo, arrows to change page\n"
                    "         L    lighting: exhibition, gallery, lamp only\n"
                    "         E    take the lamp from the torch bearer\n"
                    "         F3   credits for the statues and reliefs\n"
                    "         F1   technique notes, Esc frees the cursor\n",
                    scene.size(), textures.count());

        // ---- loop ----------------------------------------------------------
        double last = glfwGetTime();
        while (!glfwWindowShouldClose(g_app.window)) {
            const double now = glfwGetTime();
            float dt = static_cast<float>(now - last);
            last = now;
            if (dt > 0.1f) dt = 0.1f;   // so a pause does not make the visitor jump
            if (shooting) dt = 1.0f / 30.0f;   // let the room light and lamp settle
            if (dt > 1e-5f) fps = fps * 0.9f + (1.0f / dt) * 0.1f;

            glfwPollEvents();

            // The tour hands over the lamp at once if the intro is still going.
            if (g_app.tourOn && intro.stage() != Intro::Stage::Done) intro.skip();
            const bool take = g_app.takePressed;
            g_app.takePressed = false;
            // A shot shows the intro at exactly --intro-time, so it is not moved on here.
            advanceIntro(shooting ? 0.0f : dt, take);

            // During the tour, walking input is ignored.
            if (g_app.tourOn != tour.running()) {
                if (g_app.tourOn) tour.start(); else tour.stop();
            }
            if (tour.running()) {
                tour.update(dt);
                camera.teleport(tour.position(), tour.headingDeg());
            }

            MoveInput in;
            if (g_app.mouseCaptured && !tour.running() && !shooting) {
                // Until the lamp is in your hand you can look around but not walk.
                const bool canWalk = !intro.visitorLocked();
                in.forward = canWalk && glfwGetKey(g_app.window, GLFW_KEY_W) == GLFW_PRESS;
                in.back    = canWalk && glfwGetKey(g_app.window, GLFW_KEY_S) == GLFW_PRESS;
                in.left    = canWalk && glfwGetKey(g_app.window, GLFW_KEY_A) == GLFW_PRESS;
                in.right   = canWalk && glfwGetKey(g_app.window, GLFW_KEY_D) == GLFW_PRESS;
                in.run     = glfwGetKey(g_app.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
                in.mouseDX = g_app.accumDX;
                in.mouseDY = g_app.accumDY;
            }
            g_app.accumDX = g_app.accumDY = 0.0f;

            if (!tour.running()) camera.update(in, collider, dt);

            // Lighting mode. Changes fade rather than snap.
            const cfg::LightingPreset& preset = cfg::kLightingPresets[g_app.lightMode];
            lighting.setRevealLevel(preset.reveal);

            // Which room are we in, and how bright is its light?
            lighting.update(camera.planPos(), dt);
            const glm::vec3 eye = camera.eye();
            lighting.lamp.follow(eye.x, eye.y, eye.z,
                                 camera.forward().x, camera.forward().y, camera.forward().z,
                                 camera.right().x, camera.right().y, camera.right().z,
                                 dt);

            // Until it is handed over, the light is the bearer's torch, pointing
            // ahead of them. Then it glides from the torch into your hand.
            // A flame flickers: a few sine waves at odd rates, so it never quite repeats.
            const float t = static_cast<float>(now);
            const float flicker = 0.5f * std::sin(t * 13.0f) + 0.3f * std::sin(t * 23.7f + 1.0f) +
                                  0.2f * std::sin(t * 7.3f + 2.0f);
            const glm::vec3 flame = torchBase() + glm::vec3(0.0f, 0.4f, 0.0f);
            if (haveBearer && intro.handover() < 1.0f) {
                const float h = glm::radians(intro.headingDeg());
                const glm::vec3 torchDir = glm::normalize(glm::vec3(std::sin(h), -0.45f, -std::cos(h)));
                const float k = intro.handover();
                Lamp& l = lighting.lamp;
                const glm::vec3 held(l.posX, l.posY, l.posZ);
                const glm::vec3 pos = glm::mix(flame, held, k);
                const glm::vec3 dir = glm::normalize(glm::mix(torchDir, glm::vec3(l.dirX, l.dirY, l.dirZ), k));
                l.posX = pos.x; l.posY = pos.y; l.posZ = pos.z;
                l.dirX = dir.x; l.dirY = dir.y; l.dirZ = dir.z;
            }

            // Rooms move their pieces: when update() says something moved, the
            // new transforms replace the old ones, in the order build() gave them.
            for (std::size_t i = 0; i < rooms.size(); ++i) {
                if (!rooms[i]->update(dt)) continue;
                std::vector<glm::mat4> moved;
                rooms[i]->transforms(moved);
                for (std::size_t k = 0; k < moved.size() && k < roomPieces[i].size(); ++k)
                    scene[roomPieces[i][k]].transform = moved[k];
            }
            // And hear when the visitor walks in or out.
            if (lighting.currentRoomId() != visitorRoom) {
                for (std::size_t i = 0; i < rooms.size(); ++i) {
                    if (roomIds[i] == visitorRoom) rooms[i]->onExit();
                    if (roomIds[i] == lighting.currentRoomId()) rooms[i]->onEnter();
                }
                visitorRoom = lighting.currentRoomId();
            }

            // ---- draw ------------------------------------------------------
            const float aspect = static_cast<float>(g_app.width) /
                                 static_cast<float>(g_app.height ? g_app.height : 1);

            shader->use();
            shader->set("uViewPos", eye);

            const Lamp& lamp = lighting.lamp;
            shader->set("uLampPos", glm::vec3(lamp.posX, lamp.posY, lamp.posZ));
            shader->set("uLampDir", glm::vec3(lamp.dirX, lamp.dirY, lamp.dirZ));
            shader->set("uLampColour", glm::vec3(cfg::kLampColour[0],
                                                 cfg::kLampColour[1],
                                                 cfg::kLampColour[2]));
            // The torch's light flickers with its flame, and steadies once it is your lamp.
            const float flame01 = haveBearer ? 1.0f - intro.handover() : 0.0f;
            shader->set("uLampIntensity",
                        lamp.intensity * preset.lampScale * (1.0f + 0.07f * flicker * flame01));
            shader->set("uLampRange", lamp.range);
            shader->set("uLampInnerCos", lamp.innerCos);
            shader->set("uLampOuterCos", lamp.outerCos);
            shader->set("uVaultColour", glm::vec3(cfg::kVaultColour[0],
                                                  cfg::kVaultColour[1],
                                                  cfg::kVaultColour[2]));
            shader->set("uAmbient", preset.ambient);

            // The bearer's flame lights them and their surroundings, and fades
            // as the torch becomes your lamp.
            shader->set("uFlamePos", flame);
            shader->set("uFlameColour", glm::vec3(cfg::kFlameColour[0], cfg::kFlameColour[1],
                                                  cfg::kFlameColour[2]));
            shader->set("uFlameIntensity", haveBearer ? cfg::kFlameIntensity * (1.0f - intro.handover()) *
                                                            (1.0f + 0.12f * flicker)
                                                      : 0.0f);
            shader->set("uFlameRange", cfg::kFlameRange);

            // Shadow pass: draw the scene's depth from the lamp, into the shadow map.
            const bool castShadows = haveShadows && g_app.shadows;
            const glm::mat4 lampVP = lampProjection(lamp) * lampView(lamp);
            shader->set("uShadowMap", 2);
            if (castShadows) {
                shadowMap.begin();
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, shadowMap.placeholder());
                shader->set("uShadowPass", 1);
                shader->set("uView", lampView(lamp));
                shader->set("uProj", lampProjection(lamp));
                for (const Drawable& d : scene) {
                    shader->set("uModel", d.transform);
                    shader->set("uNormalMatrix",
                                glm::mat3(glm::inverseTranspose(glm::mat3(d.transform))));
                    bindMaterial(*shader, d.material, textures);
                    if (d.mesh) d.mesh->draw();
                }
                if (haveBearer && !intro.bearerHasTorch()) drawBearer(*shader, bearer, textures);
                shadowMap.end(g_app.width, g_app.height);
            }

            // Main pass, from the visitor's eye.
            glClearColor(0.012f, 0.010f, 0.008f, 1.0f);   // nearly black
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader->use();
            shader->set("uShadowPass", 0);
            shader->set("uView", camera.view());
            shader->set("uProj", camera.projection(aspect));
            shader->set("uLampViewProj", lampVP);
            shader->set("uShadowsOn", castShadows ? 1 : 0);
            shader->set("uShadowBias", cfg::kShadowBias);
            shader->set("uShadowNormalPush", cfg::kShadowNormalPush);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D,
                          castShadows ? shadowMap.texture() : shadowMap.placeholder());

            for (const Drawable& d : scene) {
                // Room pieces get their room's light. The corridor only gets the
                // lamp, apart from the oculus over the atrium and the niche lights.
                float vault = 0.0f;
                if (d.roomId == "ATRIUM")     vault = preset.reveal * cfg::kOculusLight;
                else if (d.roomId == "NICHE") vault = preset.reveal * cfg::kNicheLight;
                else if (!d.roomId.empty())   vault = lighting.levelFor(d.roomId);
                shader->set("uVaultLevel", vault);

                shader->set("uModel", d.transform);
                shader->set("uNormalMatrix",
                            glm::mat3(glm::inverseTranspose(glm::mat3(d.transform))));
                bindMaterial(*shader, d.material, textures);
                if (d.mesh) d.mesh->draw();
            }

            if (haveBearer) {
                // The flame lights its bearer like everything else. From above,
                // only the oculus does, in the atrium.
                shader->set("uVaultLevel", plan.atriumRect.contains(intro.position())
                                               ? preset.reveal * cfg::kOculusLight : 0.0f);
                drawBearer(*shader, bearer, textures);

                if (intro.bearerHasTorch()) {
                    const glm::mat4 at = glm::translate(glm::mat4(1.0f), torchBase());
                    shader->set("uModel", at);
                    shader->set("uNormalMatrix", glm::mat3(1.0f));
                    bindMaterial(*shader, torchHandle.material, textures);
                    torchHandle.mesh->draw();
                    // The flame stretches and swells with the flicker, from its base.
                    const glm::vec3 base(0.0f, 0.31f, 0.0f);
                    const glm::mat4 flameAt = at * glm::translate(glm::mat4(1.0f), base) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(1.0f + 0.08f * flicker,
                                                              1.0f + 0.20f * flicker,
                                                              1.0f + 0.08f * flicker)) *
                        glm::translate(glm::mat4(1.0f), -base);
                    shader->set("uModel", flameAt);
                    bindMaterial(*shader, torchFlame.material, textures);
                    torchFlame.mesh->draw();
                }
            }

            // The minimap, drawn with our own rasteriser.
            minimap.redraw(camera.planPos(), camera.headingDeg(), lighting.currentRoomId());
            minimap.present(g_app.width, g_app.height);

            // The status strip.
            drawHud(hudCanvas, preset, lighting.currentRoomId(), camera.planPos(),
                    tour.running(), shooting ? 30.0f : fps, castShadows);
            hudView.draw(hudCanvas, g_app.width, g_app.height,
                         16, g_app.height - hudCanvas.height() - 16,
                         hudCanvas.width(), hudCanvas.height(), 0.92f);

            // "Press E to take the lamp", centred near the bottom.
            const std::string prompt = intro.prompt();
            if (!prompt.empty()) {
                promptCanvas.clear(raster::Colour{20, 17, 13, 200});
                font::drawTextCentred(promptCanvas, promptCanvas.width() / 2, 12, prompt,
                                      raster::Colour{242, 212, 121, 255}, 2);
                promptView.draw(promptCanvas, g_app.width, g_app.height,
                                (g_app.width - promptCanvas.width()) / 2, 90,
                                promptCanvas.width(), promptCanvas.height(), 0.92f);
            }

            if (g_app.showCredits) {
                drawCredits(demoCanvas, sculptures, haveBearer ? bearerSet.credit : std::string());
                demoView.drawFullscreen(demoCanvas, g_app.width, g_app.height, 0.97f);
            }

            // The algorithm demo, on top of everything.
            if (g_app.showDemo) {
                demo::renderPage(demoCanvas, static_cast<demo::Page>(g_app.demoPage), &plan);
                demoView.drawFullscreen(demoCanvas, g_app.width, g_app.height, 0.97f);
            }

            // Report any OpenGL error from the first frame, so a broken draw call
            // shows up in the console instead of as a dark screen.
            if (frame == 0)
                for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError())
                    std::fprintf(stderr, "[lumen] OpenGL error 0x%04X in the first frame\n", e);
            ++frame;
            // A shot also times the last 60 frames, to check the frame rate.
            static double timerStart = 0.0;
            if (shooting && frame == 60) { glFinish(); timerStart = glfwGetTime(); }
            if (shooting && frame == 120) {
                glFinish();
                const double ms = (glfwGetTime() - timerStart) * 1000.0 / 60.0;
                std::printf("[lumen] %.1f ms a frame, about %.0f FPS\n", ms, 1000.0 / ms);
                const bool ok = saveFrame(shot.path, g_app.width, g_app.height);
                std::printf("[lumen] %s %s\n", ok ? "saved" : "could not save", shot.path.c_str());
                glfwSetWindowShouldClose(g_app.window, 1);
            }

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
