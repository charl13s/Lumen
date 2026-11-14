// CanvasView.h - shows a Canvas on screen.
//
// The minimap and demo draw into a raster::Canvas with our own code. This is the
// only place that hands it to OpenGL: upload it as a texture, draw one quad.

#pragma once
#include "Raster.h"

namespace lumen::gfx {

class CanvasView {
public:
    CanvasView() = default;
    ~CanvasView();
    CanvasView(const CanvasView&)            = delete;
    CanvasView& operator=(const CanvasView&) = delete;

    // Draws into a rectangle, in pixels from the bottom left of the window.
    // GL objects are created on first use.
    void draw(const raster::Canvas& canvas,
              int screenW, int screenH,
              int x, int y, int w, int h,
              float opacity = 1.0f);

    // Fills the whole window.
    void drawFullscreen(const raster::Canvas& canvas, int screenW, int screenH,
                        float opacity = 1.0f);

private:
    unsigned texture_ = 0;
    unsigned vao_ = 0, vbo_ = 0;
    unsigned program_ = 0;
    int      texW_ = 0, texH_ = 0;
    bool     ready_ = false;

    void ensureGL(int w, int h);
};

} // namespace lumen::gfx
