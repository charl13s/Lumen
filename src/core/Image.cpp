#include "Image.h"

#include <cstdint>
#include <fstream>
#include <vector>

namespace lumen::image {
namespace {

void put16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}

void put32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

} // namespace

bool writeBmp(const raster::Canvas& c, const std::string& path) {
    const int w = c.width(), h = c.height();
    const std::uint32_t rowBytes = (static_cast<std::uint32_t>(w) * 3 + 3) & ~3u;   // rows pad to 4 bytes
    const std::uint32_t pixelBytes = rowBytes * static_cast<std::uint32_t>(h);

    std::vector<std::uint8_t> b;
    b.reserve(54 + pixelBytes);

    // File header.
    b.push_back('B');
    b.push_back('M');
    put32(b, 54 + pixelBytes);
    put32(b, 0);
    put32(b, 54);

    // Info header.
    put32(b, 40);
    put32(b, static_cast<std::uint32_t>(w));
    put32(b, static_cast<std::uint32_t>(h));   // positive height: rows stored bottom up
    put16(b, 1);
    put16(b, 24);
    put32(b, 0);
    put32(b, pixelBytes);
    put32(b, 2835);   // 72 dpi
    put32(b, 2835);
    put32(b, 0);
    put32(b, 0);

    // Pixels, bottom row first, in blue green red order.
    for (int y = h - 1; y >= 0; --y) {
        std::uint32_t written = 0;
        for (int x = 0; x < w; ++x) {
            const raster::Colour p = c.get(x, y);
            b.push_back(p.b);
            b.push_back(p.g);
            b.push_back(p.r);
            written += 3;
        }
        for (; written < rowBytes; ++written) b.push_back(0);
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    return static_cast<bool>(out);
}

} // namespace lumen::image
