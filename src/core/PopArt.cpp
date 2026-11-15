// PopArt.cpp - print-style art from a photograph. Owner: Shawn. See PopArt.h.

#include "PopArt.h"

#include <algorithm>
#include <cmath>

#include "Font.h"

namespace lumen::popart {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDotGamma = 1.6f;   // halftone tone curve, 1 = straight

using raster::Canvas;
using raster::Colour;

// Fills the scale x scale block for source pixel (x, y).
void block(Canvas& c, int x, int y, int scale, Colour col) {
    for (int j = 0; j < scale; ++j)
        for (int i = 0; i < scale; ++i)
            c.set(x * scale + i, y * scale + j, col);
}

Canvas blank(const Grey& g, int scale, Colour fill) {
    Canvas c(g.w * scale, g.h * scale);
    c.clear(fill);
    return c;
}

// The 4x4 Bayer matrix. Each entry is the order in which that pixel of the
// tile switches on as the level rises, so neighbours switch on far apart.
constexpr int kBayer4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5},
};

} // namespace

Grey::Grey(int width, int height, float fill)
    : w(std::max(width, 0)), h(std::max(height, 0)),
      v(static_cast<std::size_t>(std::max(width, 0)) * std::max(height, 0), fill) {}

float Grey::at(int x, int y) const {
    if (w <= 0 || h <= 0) return 0.0f;
    x = std::clamp(x, 0, w - 1);
    y = std::clamp(y, 0, h - 1);
    return v[static_cast<std::size_t>(y) * w + x];
}

Grey luminance(const std::uint8_t* px, int w, int h, int channels) {
    Grey g(w, h);
    if (!px || channels < 1) return g;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t* p = px + (static_cast<std::size_t>(y) * w + x) * channels;
            float lum;
            if (channels >= 3) lum = 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
            else               lum = p[0];   // already grey
            g.ref(x, y) = lum / 255.0f;
        }
    }
    return g;
}

void stretch(Grey& g, float clip) {
    if (g.v.empty()) return;
    clip = std::clamp(clip, 0.0f, 0.49f);

    // Histogram of 256 grey levels.
    int hist[256] = {};
    for (float f : g.v) ++hist[std::clamp(static_cast<int>(f * 255.0f + 0.5f), 0, 255)];

    // Walk in from each end until `clip` of the pixels have been passed.
    const float limit = clip * static_cast<float>(g.v.size());
    int lo = 0, hi = 255;
    for (float seen = 0.0f; lo < 255; ++lo) {
        seen += static_cast<float>(hist[lo]);
        if (seen > limit) break;
    }
    for (float seen = 0.0f; hi > 0; --hi) {
        seen += static_cast<float>(hist[hi]);
        if (seen > limit) break;
    }
    if (hi <= lo) return;   // a flat image has nothing to stretch

    const float a = static_cast<float>(lo) / 255.0f, b = static_cast<float>(hi) / 255.0f;
    for (float& f : g.v) f = std::clamp((f - a) / (b - a), 0.0f, 1.0f);
}

Canvas posterise(const Grey& g, const std::vector<Colour>& palette, int scale) {
    scale = std::max(scale, 1);
    Canvas c = blank(g, scale, Colour{0, 0, 0, 255});
    if (palette.empty()) return c;
    const int n = static_cast<int>(palette.size());
    for (int y = 0; y < g.h; ++y)
        for (int x = 0; x < g.w; ++x) {
            const int band = std::min(n - 1, static_cast<int>(g.at(x, y) * static_cast<float>(n)));
            block(c, x, y, scale, palette[static_cast<std::size_t>(band)]);
        }
    return c;
}

Canvas halftone(const Grey& g, int cell, Colour dot, Colour paper, int scale) {
    scale = std::max(scale, 1);
    cell  = std::max(cell, 1);
    Canvas c = blank(g, scale, paper);
    const float cellPx = static_cast<float>(cell * scale);

    for (int cy = 0; cy < g.h; cy += cell) {
        for (int cx = 0; cx < g.w; cx += cell) {
            // Mean brightness of the cell.
            float sum = 0.0f;
            int   n   = 0;
            for (int y = cy; y < std::min(g.h, cy + cell); ++y)
                for (int x = cx; x < std::min(g.w, cx + cell); ++x) { sum += g.at(x, y); ++n; }
            // A printer's tone curve: mid-tones get smaller dots than a
            // straight line would give, which keeps the shadows from merging.
            const float b = std::pow(sum / static_cast<float>(n), kDotGamma);

            // Dot area follows brightness: pi r^2 = b * cell^2.
            const float r = cellPx * std::sqrt(b / kPi);
            const int   ri = static_cast<int>(std::lround(r));
            if (r < 0.5f) continue;   // too dark for a dot
            const int px = cx * scale + static_cast<int>(cellPx * 0.5f);
            const int py = cy * scale + static_cast<int>(cellPx * 0.5f);
            raster::circleFilled(c, px, py, ri, dot);
        }
    }
    return c;
}

Canvas floydSteinberg(const Grey& g, Colour dark, Colour light, int scale) {
    scale = std::max(scale, 1);
    Canvas c = blank(g, scale, dark);
    Grey e = g;   // working copy that collects the spread error
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            const float old = e.ref(x, y);
            const float now = old >= 0.5f ? 1.0f : 0.0f;
            const float err = old - now;
            block(c, x, y, scale, now > 0.5f ? light : dark);

            if (x + 1 < g.w) e.ref(x + 1, y) += err * 7.0f / 16.0f;
            if (y + 1 < g.h) {
                if (x > 0) e.ref(x - 1, y + 1) += err * 3.0f / 16.0f;
                e.ref(x, y + 1) += err * 5.0f / 16.0f;
                if (x + 1 < g.w) e.ref(x + 1, y + 1) += err * 1.0f / 16.0f;
            }
        }
    }
    return c;
}

Canvas bayer(const Grey& g, const std::vector<Colour>& palette, int scale) {
    scale = std::max(scale, 1);
    Canvas c = blank(g, scale, Colour{0, 0, 0, 255});
    if (palette.empty()) return c;
    const int top = static_cast<int>(palette.size()) - 1;
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            // Which two colours this level falls between, and how far along.
            const float level = std::clamp(g.at(x, y), 0.0f, 1.0f) * static_cast<float>(top);
            int k = static_cast<int>(level);
            const float frac = level - static_cast<float>(k);
            const float threshold = (static_cast<float>(kBayer4[y % 4][x % 4]) + 0.5f) / 16.0f;
            if (frac > threshold) ++k;
            block(c, x, y, scale, palette[static_cast<std::size_t>(std::min(k, top))]);
        }
    }
    return c;
}

void blit(Canvas& dst, const Canvas& src, int x, int y) {
    for (int j = 0; j < src.height(); ++j)
        for (int i = 0; i < src.width(); ++i)
            dst.set(x + i, y + j, src.get(i, j));
}

Canvas warholGrid(const Grey& face, const std::string& title, int scale) {
    scale = std::max(scale, 1);
    const int pw  = face.w * scale;      // one print
    const int ph  = face.h * scale;
    const int gap = 8 * scale;           // mount between prints
    const int strip = 30 * scale;        // title strip at the bottom

    Canvas out(2 * pw + 3 * gap, 2 * ph + 3 * gap + strip);
    out.clear(Colour{22, 19, 17, 255});

    // Top left: four flat bands, hot pink background.
    blit(out, posterise(face, {{38, 22, 70, 255}, {222, 42, 112, 255},
                               {250, 198, 40, 255}, {255, 240, 205, 255}}, scale),
         gap, gap);
    // Top right: yellow dots on navy, one dot every 5 source pixels.
    blit(out, halftone(face, 5, {250, 214, 40, 255}, {32, 30, 92, 255}, scale),
         2 * gap + pw, gap);
    // Bottom left: two-colour error diffusion, teal.
    blit(out, floydSteinberg(face, {10, 60, 72, 255}, {122, 232, 212, 255}, scale),
         gap, 2 * gap + ph);
    // Bottom right: three-colour ordered dither, orange and pink.
    blit(out, bayer(face, {{92, 20, 22, 255}, {240, 92, 40, 255}, {255, 182, 200, 255}}, scale),
         2 * gap + pw, 2 * gap + ph);

    // The title, as large as fits.
    int fs = 3 * scale;
    while (fs > 1 && font::textWidth(title, fs) > out.width() - 4 * gap) --fs;
    const int ty = 2 * ph + 3 * gap + (strip - font::kGlyphH * fs) / 2 - gap / 2;
    font::drawTextCentred(out, out.width() / 2, ty, title, Colour{232, 196, 112, 255}, fs);
    return out;
}

} // namespace lumen::popart
