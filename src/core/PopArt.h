// PopArt.h - turns a photograph into print-style art, pixel by pixel. Owner: Shawn.
//
// Used by Gallery Three, where one portrait is printed four ways, after
// Warhol's screen prints. Every step is our own code on a raster::Canvas:
//
//   luminance      colour to grey, with the Rec. 601 weights
//   stretch        histogram contrast stretch
//   posterise      quantise the grey into a few flat colours
//   halftone       a grid of dots whose area follows the brightness
//   floydSteinberg two colours, with the error spread to the neighbours
//   bayer          ordered dithering with a 4x4 threshold matrix
//
// Tested in tests/test_core.cpp (testPopArt).

#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Raster.h"

namespace lumen::popart {

// A greyscale image. 0 is black, 1 is white. (0, 0) is the top left.
struct Grey {
    int w = 0, h = 0;
    std::vector<float> v;   // row-major

    Grey() = default;
    Grey(int width, int height, float fill = 0.0f);

    float at(int x, int y) const;          // clamped to the edge
    float& ref(int x, int y) { return v[static_cast<std::size_t>(y) * w + x]; }
};

// Rec. 601 luma, Y = 0.299 R + 0.587 G + 0.114 B, scaled to 0..1.
// px holds w * h pixels of `channels` bytes each (1 to 4), top row first.
Grey luminance(const std::uint8_t* px, int w, int h, int channels);

// Histogram stretch. Counts the grey levels in 256 bins, then maps the level
// below which `clip` of the pixels fall to 0, and the level above which `clip`
// fall to 1. A flat photograph uses the whole range afterwards.
void stretch(Grey& g, float clip = 0.02f);

// Each of these draws g as a new canvas, every source pixel becoming a
// scale x scale block, so the print texture stays visible on a large wall.

// Quantise into palette.size() equal bands, darkest first.
raster::Canvas posterise(const Grey& g, const std::vector<raster::Colour>& palette,
                         int scale = 1);

// Halftone. The image is cut into cells of `cell` source pixels. Each cell gets
// one dot, drawn with our midpoint filled circle, with its area proportional to
// the cell's mean brightness b: area = b * cell^2, so r = cell * sqrt(b / pi).
// b first goes through a tone curve, b^1.6, as a printer would.
raster::Canvas halftone(const Grey& g, int cell, raster::Colour dot, raster::Colour paper,
                        int scale = 1);

// Floyd-Steinberg error diffusion to two colours. Each pixel is rounded to
// black or white, and the rounding error is passed on: 7/16 right, 3/16 down
// left, 5/16 down, 1/16 down right. The average brightness is kept.
raster::Canvas floydSteinberg(const Grey& g, raster::Colour dark, raster::Colour light,
                              int scale = 1);

// Ordered dithering. Between two neighbouring palette colours, the 4x4 Bayer
// matrix decides which pixels round up, so a level halfway between lights
// exactly half of every 4x4 tile.
raster::Canvas bayer(const Grey& g, const std::vector<raster::Colour>& palette, int scale = 1);

// Four prints of the same face in a 2x2 grid, one technique each, on a dark
// mount with a title drawn in our bitmap font underneath.
raster::Canvas warholGrid(const Grey& face, const std::string& title, int scale = 2);

// Copies src into dst with its top left at (x, y). Pixels outside dst are skipped.
void blit(raster::Canvas& dst, const raster::Canvas& src, int x, int y);

} // namespace lumen::popart
