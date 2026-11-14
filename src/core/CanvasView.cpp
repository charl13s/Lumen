#include "CanvasView.h"

#include <glad/glad.h>

namespace lumen::gfx {
namespace {

const char* kVert = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
uniform vec4 uRect;   // x, y, w, h in normalised device coordinates
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

CanvasView::~CanvasView() {
    if (texture_) glDeleteTextures(1, &texture_);
    if (vbo_)     glDeleteBuffers(1, &vbo_);
    if (vao_)     glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
}

void CanvasView::ensureGL(int w, int h) {
    if (!ready_) {
        glGenTextures(1, &texture_);

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
        program_ = glCreateProgram();
        glAttachShader(program_, vs);
        glAttachShader(program_, fs);
        glLinkProgram(program_);
        glDeleteShader(vs);
        glDeleteShader(fs);

        ready_ = true;
    }

    // Only reallocate when the size changes; otherwise just update the pixels.
    if (w != texW_ || h != texH_) {
        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        texW_ = w;
        texH_ = h;
    }
}

void CanvasView::draw(const raster::Canvas& canvas,
                      int screenW, int screenH,
                      int x, int y, int w, int h,
                      float opacity) {
    if (screenW <= 0 || screenH <= 0) return;
    ensureGL(canvas.width(), canvas.height());

    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, canvas.width(), canvas.height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, canvas.pixels());

    // Pixels to normalised device coordinates.
    const float ndcX = 2.0f * x / static_cast<float>(screenW) - 1.0f;
    const float ndcY = 2.0f * y / static_cast<float>(screenH) - 1.0f;
    const float ndcW = 2.0f * w / static_cast<float>(screenW);
    const float ndcH = 2.0f * h / static_cast<float>(screenH);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program_);
    glUniform4f(glGetUniformLocation(program_, "uRect"), ndcX, ndcY, ndcW, ndcH);
    glUniform1i(glGetUniformLocation(program_, "uMap"), 0);
    glUniform1f(glGetUniformLocation(program_, "uOpacity"), opacity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void CanvasView::drawFullscreen(const raster::Canvas& canvas,
                                int screenW, int screenH, float opacity) {
    draw(canvas, screenW, screenH, 0, 0, screenW, screenH, opacity);
}

} // namespace lumen::gfx
