// Image.h - saves a Canvas as a .bmp file, in plain C++. Owner: Priscillah.
//
// Used to save every page as an image.

#pragma once
#include <string>

#include "Raster.h"

namespace lumen::image {

// 24 bit uncompressed BMP. Returns false if the file could not be written.
bool writeBmp(const raster::Canvas& canvas, const std::string& path);

} // namespace lumen::image
