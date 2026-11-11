// ArtPage.cpp - generated artwork for Gallery Three. Owner: Timothy.
//
// Four small works, each made from a seed with our own drawing algorithms. The
// same seed always gives the same picture. Two follow early computer artists,
// two are our own. Each work also moves: give it a time in seconds and it
// draws that moment. The movement loops every 8 seconds.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Curves.h"
#include "PageKit.h"
#include "Polygon.h"
#include "Transform.h"

namespace lumen::demo {
namespace {

// Repeatable random numbers from a seed (a linear congruential generator).
struct Random {
    unsigned state;
    explicit Random(unsigned seed) : state(seed) {}
    float next() {   // 0 to 1
        state = state * 1664525u + 1013904223u;
        return static_cast<float>((state >> 8) & 0xFFFFFFu) / 16777215.0f;
    }
    float range(float a, float b) { return a + (b - a) * next(); }
};

constexpr int kSize = 250;        // each canvas is square, in pixels
constexpr float kLoop = 8.0f;     // seconds before the movement repeats

const Colour kPaper{236, 230, 214, 255};
const Colour kInkDark{34, 30, 26, 255};
const Colour kPalette[] = {{196, 84, 60, 255}, {222, 170, 72, 255}, {70, 110, 140, 255},
                           {110, 150, 110, 255}, {60, 52, 80, 255}};

int ix(float v) { return static_cast<int>(std::lround(v)); }

// The phase of the loop, 0 to 2 pi.
float phase(float t) { return 2.0f * kPi * std::fmod(t, kLoop) / kLoop; }

// Interruptions: short lines at random angles on a grid, with some left out.
// Over the loop each line turns half a circle, one way or the other.
void interruptions(Canvas& c, int x0, int y0, unsigned seed, float t) {
    Random r(seed), move(seed ^ 0x5A5A5A5Au);
    const int cells = 22;
    const float cell = static_cast<float>(kSize - 20) / cells;
    for (int row = 0; row < cells; ++row) {
        for (int col = 0; col < cells; ++col) {
            const float turn = move.next() < 0.5f ? -0.5f : 0.5f;
            const float a = r.range(0.0f, kPi) + turn * phase(t);
            if (r.next() < 0.12f) continue;   // the interruptions
            const float cx = x0 + 10 + (col + 0.5f) * cell;
            const float cy = y0 + 10 + (row + 0.5f) * cell;
            const float dx = std::cos(a) * cell * 0.5f, dy = std::sin(a) * cell * 0.5f;
            raster::lineBresenham(c, ix(cx - dx), ix(cy - dy), ix(cx + dx), ix(cy + dy), kInkDark);
        }
    }
}

// Schotter: rows of squares, each rotated about its centre and moved, more so
// the lower it is. Over the loop the disorder settles and returns.
void schotter(Canvas& c, int x0, int y0, unsigned seed, float t) {
    Random r(seed);
    const float amount = 0.5f + 0.5f * std::cos(phase(t));
    const int cols = 9, rows = 10, cell = 22, side = 18;
    const int ox = x0 + (kSize - cols * cell) / 2, oy = y0 + (kSize - rows * cell) / 2;
    for (int row = 0; row < rows; ++row) {
        const float disorder = amount * static_cast<float>(row) / (rows - 1);
        for (int col = 0; col < cols; ++col) {
            const float dx = r.range(-1, 1) * disorder * 6.0f, dy = r.range(-1, 1) * disorder * 6.0f;
            const float angle = r.range(-1, 1) * disorder * kPi / 4.0f;
            const xf::PointF centre{ox + col * cell + side * 0.5f, oy + row * cell + side * 0.5f};
            const poly::Polygon square = poly::rectangle(-side * 0.5f, -side * 0.5f, side * 0.5f, side * 0.5f);
            const xf::Mat3 m = xf::translation(centre.x + dx, centre.y + dy) * xf::rotation(angle);
            raster::polyline(c, poly::toPixels(xf::transform(square, m)), 1, kInkDark, true);
        }
    }
}

// Mosaic: random polygons, clipped to the canvas with Sutherland-Hodgman and
// filled by scanline. Each one turns about its centre and drifts in a circle.
void mosaic(Canvas& c, int x0, int y0, unsigned seed, float t) {
    Random r(seed), move(seed ^ 0x5A5A5A5Au);
    const float left = static_cast<float>(x0), top = static_cast<float>(y0);
    for (int i = 0; i < 45; ++i) {
        const poly::PointF centre{r.range(left - 20, left + kSize + 20), r.range(top - 20, top + kSize + 20)};
        const int n = 3 + static_cast<int>(r.next() * 5.0f);
        std::vector<float> angles;
        for (int k = 0; k < n; ++k) angles.push_back(r.range(0.0f, 2.0f * kPi));
        std::sort(angles.begin(), angles.end());   // sorted angles keep the outline simple
        poly::Polygon shape;
        for (float a : angles) {
            const float rad = r.range(18.0f, 60.0f);
            shape.push_back({centre.x + std::cos(a) * rad, centre.y + std::sin(a) * rad});
        }
        const Colour col = kPalette[static_cast<int>(r.next() * 5.0f) % 5];

        const float spin = move.next() < 0.5f ? -1.0f : 1.0f, offset = move.range(0.0f, 2.0f * kPi);
        const float drift = 8.0f;
        const xf::Mat3 m = xf::translation(drift * std::sin(phase(t) + offset) - drift * std::sin(offset),
                                           drift * std::cos(phase(t) + offset) - drift * std::cos(offset)) *
                           xf::rotationAbout(spin * phase(t), centre);
        const poly::Polygon inside = poly::clipSutherlandHodgman(
            xf::transform(shape, m), left, top, left + kSize - 1, top + kSize - 1);
        if (inside.empty()) continue;
        raster::fillPolygonScanline(c, poly::toPixels(inside), col);
        raster::polyline(c, poly::toPixels(inside), 1, kInkDark, true);
    }
}

// Currents: cubic Bezier curves flowing across, clipped to the canvas, with a
// few midpoint circles floating over them. The curves sway and the circles
// drift to the right, coming back in on the left.
void currents(Canvas& c, int x0, int y0, unsigned seed, float t) {
    Random r(seed), move(seed ^ 0x5A5A5A5Au);
    const float l = static_cast<float>(x0), top = static_cast<float>(y0);
    const float rgt = l + kSize - 1, bot = top + kSize - 1;
    for (int i = 0; i < 18; ++i) {
        const float y = top + r.range(10.0f, kSize - 10.0f);
        // Each sway is zero at t = 0, so the still picture is unchanged.
        const float p1 = move.range(0.0f, 2.0f * kPi), p2 = move.range(0.0f, 2.0f * kPi);
        const float sway1 = 25.0f * (std::sin(phase(t) + p1) - std::sin(p1));
        const float sway2 = 25.0f * (std::sin(phase(t) + p2) - std::sin(p2));
        const std::vector<curves::P2> control{{l - 10, y},
                                              {l + 80, y + r.range(-90, 90) + sway1},
                                              {l + 170, y + r.range(-90, 90) + sway2},
                                              {rgt + 10, y + r.range(-40, 40)}};
        const std::vector<curves::P2> pts = curves::sampleBezier(control, 60);
        const Colour col = kPalette[i % 5];
        for (std::size_t k = 0; k + 1 < pts.size(); ++k) {
            float ax = pts[k].x, ay = pts[k].y, bx = pts[k + 1].x, by = pts[k + 1].y;
            if (!raster::clipCohenSutherland(ax, ay, bx, by, l, top, rgt, bot)) continue;
            raster::lineThick(c, ix(ax), ix(ay), ix(bx), ix(by), 2, col);
        }
    }
    const float travel = kSize - 40.0f;
    for (int i = 0; i < 6; ++i) {
        const int rad = static_cast<int>(r.range(6.0f, 18.0f));
        const float startX = r.range(l + 20, rgt - 20);
        const int cy = ix(r.range(top + 20, bot - 20));
        const float x = l + 20 + std::fmod(startX - (l + 20) + travel * std::fmod(t, kLoop) / kLoop, travel);
        raster::circleFilled(c, ix(x), cy, rad, kPaper);
        raster::circleMidpoint(c, ix(x), cy, rad, kInkDark);
    }
}

struct Work {
    const char* title;
    const char* by;
    const char* how;
    unsigned seed;
    void (*draw)(Canvas&, int, int, unsigned, float);
};

const Work kWorks[] = {
    {"INTERRUPTIONS", "AFTER VERA MOLNAR, 1968", "BRESENHAM LINES ON A GRID", 1968u, interruptions},
    {"SCHOTTER", "AFTER GEORG NEES, 1968", "ROTATED SQUARES, MORE DISORDER LOWER DOWN", 1965u, schotter},
    {"MOSAIC", "LUMEN, 2026", "SUTHERLAND-HODGMAN, SCANLINE FILL", 3204u, mosaic},
    {"CURRENTS", "LUMEN, 2026", "CUBIC BEZIERS AND MIDPOINT CIRCLES", 2026u, currents},
};

// A plain frame, then the paper, then the work at time t.
void framedWork(Canvas& c, const Work& wk, int x, int y, float t) {
    raster::fillPolygonScanline(c, {{x - 8, y - 8}, {x + kSize + 8, y - 8},
                                    {x + kSize + 8, y + kSize + 8}, {x - 8, y + kSize + 8}},
                                kGoldDim);
    raster::fillPolygonScanline(c, {{x, y}, {x + kSize, y}, {x + kSize, y + kSize}, {x, y + kSize}},
                                kPaper);
    wk.draw(c, x, y, wk.seed, t);
}

} // namespace

void pageArtwork(Canvas& c, float /*time*/) {
    const int w = c.width(), h = c.height();
    panel(c, 30, 60, w - 60, h - 160, "GALLERY THREE: GENERATED ARTWORK");

    for (int i = 0; i < 4; ++i) {
        const Work& wk = kWorks[i];
        const int x = 60 + i * 300, y = 110;
        framedWork(c, wk, x, y, 0.0f);

        // The wall label.
        const int ly = y + kSize + 26;
        font::drawText(c, x, ly, wk.title, kInk, 2);
        font::drawText(c, x, ly + 24, wk.by, kDim, 1);
        font::drawText(c, x, ly + 40, wk.how, kDim, 1);
        font::drawText(c, x, ly + 56, "SEED " + std::to_string(wk.seed), kGold, 1);
    }

    caption(c, 60, h - 150, "EACH WORK IS MADE FROM A SEED. THE SAME SEED ALWAYS GIVES THE SAME PICTURE,");
    caption(c, 60, h - 134, "SO A NEW SEED IS A NEW EXHIBITION.");
}

// All four works moving together.
void animArtwork(Canvas& c, const Plan* /*plan*/, float t) {
    c.clear(kBg);
    font::drawText(c, 30, 14, "GALLERY THREE, MOVING", kGold, 2);
    for (int i = 0; i < 4; ++i) {
        const int x = 38 + i * 280, y = 58;
        framedWork(c, kWorks[i], x, y, t);
        font::drawText(c, x, y + kSize + 18, kWorks[i].title, kInk, 1);
    }
}

} // namespace lumen::demo
