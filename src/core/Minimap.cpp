#include "Minimap.h"

#include <cmath>
#include <vector>

#include <glad/glad.h>

#include "Gfx.h"

namespace lumen {
namespace {

using raster::Colour;
using raster::Point;

// The deck's palette: near-black ground, cream walls, gold accents.
const Colour kGround {20, 17, 13, 210};
const Colour kWall   {230, 214, 164, 255};
const Colour kRoom   {44, 37, 24, 255};
const Colour kLitRoom{95, 78, 40, 255};
const Colour kVisitor{242, 212, 121, 255};
const Colour kDoor   {150, 132, 86, 255};
const Colour kEdge   {120, 108, 78, 255};

constexpr float kDeg2Rad = 3.14159265358979f / 180.0f;

const char* kVert = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
uniform vec4 uRect;   // x, y, w, h in normalised device coords
void main() {
    vUV = aUV;
    gl_Position = vec4(uRect.x + aPos.x * uRect.z,
                       uRect.y + aPos.y * uRect.w, 0.0, 1.0);
})";

const char* kFrag = R"(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uMap;
uniform float uOpacity;
void main() {
    vec4 c = texture(uMap, vUV);
    FragColor = vec4(c.rgb, c.a * uOpacity);
})";

unsigned compileInline(GLenum type, const char* src) {
    const unsigned s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

} // namespace

Minimap::Minimap(const Plan& plan, int pixels)
    : plan_(&plan), canvas_(pixels, pixels) {}

Minimap::~Minimap() {
    if (texture_) glDeleteTextures(1, &texture_);
    if (vbo_)     glDeleteBuffers(1, &vbo_);
    if (vao_)     glDeleteVertexArrays(1, &vao_);
}

Point Minimap::toPx(Vec2 p) const {
    // Plan +y is north; canvas +y is down, so north must flip.
    const float half = canvas_.width() * 0.5f;
    return Point{
        static_cast<int>(std::lround(half + (p.x - origin_.x) * scale_)),
        static_cast<int>(std::lround(half - (p.y - origin_.y) * scale_)),
    };
}

void Minimap::redraw(Vec2 player, float headingDeg, const std::string& currentRoomId) {
    if (!plan_) return;
    origin_ = player;                 // the map scrolls under a fixed visitor
    canvas_.clear(kGround);

    const int n = canvas_.width();
    const float vxMin = 1.0f, vyMin = 1.0f;
    const float vxMax = static_cast<float>(n - 2);
    const float vyMax = static_cast<float>(n - 2);

    // --- room footprints, scanline filled --------------------------------
    for (const RoomPlan& r : plan_->rooms) {
        const Point a = toPx(Vec2{r.rect.x0, r.rect.y0});
        const Point b = toPx(Vec2{r.rect.x1, r.rect.y1});
        const std::vector<Point> quad{
            {a.x, a.y}, {b.x, a.y}, {b.x, b.y}, {a.x, b.y}};
        raster::fillPolygonScanline(canvas_, quad,
                                    (r.id == currentRoomId) ? kLitRoom : kRoom);
    }

    // The corridor ring and atrium, so the loop reads as a loop.
    {
        const Point o0 = toPx(Vec2{plan_->corridorOuter.x0, plan_->corridorOuter.y0});
        const Point o1 = toPx(Vec2{plan_->corridorOuter.x1, plan_->corridorOuter.y1});
        const std::vector<Point> ring{
            {o0.x, o0.y}, {o1.x, o0.y}, {o1.x, o1.y}, {o0.x, o1.y}};
        raster::fillPolygonScanline(canvas_, ring, kRoom);

        const Point i0 = toPx(Vec2{plan_->corridorInner.x0, plan_->corridorInner.y0});
        const Point i1 = toPx(Vec2{plan_->corridorInner.x1, plan_->corridorInner.y1});
        const std::vector<Point> atrium{
            {i0.x, i0.y}, {i1.x, i0.y}, {i1.x, i1.y}, {i0.x, i1.y}};
        raster::fillPolygonScanline(canvas_, atrium, kGround);
    }

    // --- walls, clipped then drawn with Bresenham -------------------------
    for (const Segment& s : plan_->walls) {
        const Point a = toPx(s.a);
        const Point b = toPx(s.b);

        float x0 = static_cast<float>(a.x), y0 = static_cast<float>(a.y);
        float x1 = static_cast<float>(b.x), y1 = static_cast<float>(b.y);

        // Cohen-Sutherland against the minimap viewport: most of the building is
        // off-map at any moment, and clipping first means we never rasterise it.
        if (!raster::clipCohenSutherland(x0, y0, x1, y1, vxMin, vyMin, vxMax, vyMax))
            continue;

        raster::lineThick(canvas_,
                          static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0)),
                          static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)),
                          2, kWall);
    }

    // --- doorways, as small circles --------------------------------------
    for (const RoomPlan& r : plan_->rooms) {
        const Point d = toPx(r.door.centre(r.rect));
        raster::circleFilled(canvas_, d.x, d.y, 2, kDoor);
    }

    // --- the visitor -------------------------------------------------------
    const int c = n / 2;
    raster::circleFilled(canvas_, c, c, 4, kVisitor);
    raster::circleMidpoint(canvas_, c, c, 7, kVisitor);

    // A short stalk showing which way they are facing. Heading 0 is north, which
    // is up on the map.
    const float h = headingDeg * kDeg2Rad;
    const int tipX = c + static_cast<int>(std::lround(std::sin(h) * 14.0f));
    const int tipY = c - static_cast<int>(std::lround(std::cos(h) * 14.0f));
    raster::lineThick(canvas_, c, c, tipX, tipY, 2, kVisitor);

    // --- border ------------------------------------------------------------
    raster::lineBresenham(canvas_, 0, 0, n - 1, 0, kEdge);
    raster::lineBresenham(canvas_, n - 1, 0, n - 1, n - 1, kEdge);
    raster::lineBresenham(canvas_, n - 1, n - 1, 0, n - 1, kEdge);
    raster::lineBresenham(canvas_, 0, n - 1, 0, 0, kEdge);
}

void Minimap::ensureGL() {
    if (glReady_) return;

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, canvas_.width(), canvas_.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    const float quad[] = {
        0.0f, 0.0f,  0.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 1.0f,
        1.0f, 1.0f,  1.0f, 0.0f,
        0.0f, 0.0f,  0.0f, 1.0f,
        1.0f, 1.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  0.0f, 0.0f,
    };
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);

    const unsigned vs = compileInline(GL_VERTEX_SHADER, kVert);
    const unsigned fs = compileInline(GL_FRAGMENT_SHADER, kFrag);
    const unsigned prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    progId_ = prog;

    glReady_ = true;
}

void Minimap::present(int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0) return;
    ensureGL();

    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, canvas_.width(), canvas_.height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, canvas_.pixels());

    // Bottom-right corner, in normalised device coordinates.
    const float w = 2.0f * canvas_.width()  / static_cast<float>(screenW);
    const float h = 2.0f * canvas_.height() / static_cast<float>(screenH);
    const float mx = 2.0f * cfg::kMinimapMargin / static_cast<float>(screenW);
    const float my = 2.0f * cfg::kMinimapMargin / static_cast<float>(screenH);
    const float x = 1.0f - w - mx;
    const float y = -1.0f + my;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(progId_);
    glUniform4f(glGetUniformLocation(progId_, "uRect"), x, y, w, h);
    glUniform1i(glGetUniformLocation(progId_, "uMap"), 0);
    glUniform1f(glGetUniformLocation(progId_, "uOpacity"), cfg::kMinimapOpacity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

} // namespace lumen
