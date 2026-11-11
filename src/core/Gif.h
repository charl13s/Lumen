// Gif.h - saves frames as an animated GIF, in plain C++. Owner: Shawn.
//
// Each frame gets its own palette of up to 256 colours (median cut when the
// frame has more), and its pixels are compressed with LZW, as the GIF format
// requires. The animation loops forever.

#pragma once
#include <fstream>
#include <string>
#include <vector>

#include "Raster.h"

namespace lumen::gif {

class Writer {
public:
    // Starts a file. Returns false if it cannot be created.
    bool open(const std::string& path, int width, int height);
    // Adds a frame. The canvas must match the size given to open().
    // delayCs is how long the frame is shown, in hundredths of a second.
    void addFrame(const raster::Canvas& frame, int delayCs);
    // Finishes the file. Returns false if anything failed to write.
    bool close();

    int frames() const { return frames_; }

private:
    std::ofstream out_;
    int width_ = 0, height_ = 0, frames_ = 0;
};

// Picks up to 256 colours for an image (median cut), then gives each pixel the
// index of the nearest one. Exposed so it can be tested.
struct Indexed {
    std::vector<raster::Colour> palette;
    std::vector<unsigned char> pixels;
};
Indexed quantise(const raster::Canvas& frame);

// GIF's LZW compression of palette indices, 8 bit minimum code size.
std::vector<unsigned char> lzwEncode(const std::vector<unsigned char>& indices);

} // namespace lumen::gif
