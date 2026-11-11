#include "Gif.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace lumen::gif {
namespace {

std::uint32_t key(raster::Colour c) {
    return (static_cast<std::uint32_t>(c.r) << 16) | (static_cast<std::uint32_t>(c.g) << 8) | c.b;
}

raster::Colour unkey(std::uint32_t k) {
    return raster::Colour{static_cast<std::uint8_t>(k >> 16), static_cast<std::uint8_t>(k >> 8),
                          static_cast<std::uint8_t>(k), 255};
}

void put16(std::ofstream& out, int v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
}

// One box of colours for median cut, with how many pixels use each.
struct Box {
    std::vector<std::pair<std::uint32_t, int>> colours;

    int channel(std::uint32_t k, int ch) const { return static_cast<int>((k >> (16 - 8 * ch)) & 0xFF); }

    // The channel with the widest spread, and that spread.
    std::pair<int, int> widest() const {
        int best = 0, spread = -1;
        for (int ch = 0; ch < 3; ++ch) {
            int lo = 255, hi = 0;
            for (const auto& [k, n] : colours) {
                lo = std::min(lo, channel(k, ch));
                hi = std::max(hi, channel(k, ch));
            }
            if (hi - lo > spread) { spread = hi - lo; best = ch; }
        }
        return {best, spread};
    }

    raster::Colour average() const {
        double r = 0, g = 0, b = 0, total = 0;
        for (const auto& [k, n] : colours) {
            r += channel(k, 0) * static_cast<double>(n);
            g += channel(k, 1) * static_cast<double>(n);
            b += channel(k, 2) * static_cast<double>(n);
            total += n;
        }
        auto to8 = [&](double v) { return static_cast<std::uint8_t>(std::clamp(v / total + 0.5, 0.0, 255.0)); };
        return raster::Colour{to8(r), to8(g), to8(b), 255};
    }
};

// Writes codes of a varying bit width, least significant bit first.
class BitWriter {
public:
    void write(unsigned code, int bits) {
        acc_ |= static_cast<std::uint32_t>(code) << used_;
        used_ += bits;
        while (used_ >= 8) {
            bytes.push_back(static_cast<unsigned char>(acc_ & 0xFF));
            acc_ >>= 8;
            used_ -= 8;
        }
    }
    void flush() {
        if (used_ > 0) bytes.push_back(static_cast<unsigned char>(acc_ & 0xFF));
        acc_ = 0;
        used_ = 0;
    }
    std::vector<unsigned char> bytes;

private:
    std::uint32_t acc_ = 0;
    int used_ = 0;
};

} // namespace

Indexed quantise(const raster::Canvas& frame) {
    const int n = frame.width() * frame.height();
    const std::uint8_t* px = frame.pixels();

    std::unordered_map<std::uint32_t, int> counts;
    for (int i = 0; i < n; ++i)
        ++counts[key(raster::Colour{px[i * 4], px[i * 4 + 1], px[i * 4 + 2], 255})];

    Indexed out;
    std::unordered_map<std::uint32_t, unsigned char> index;

    if (counts.size() <= 256) {
        // Few enough colours: use them exactly.
        for (const auto& [k, c] : counts) {
            index[k] = static_cast<unsigned char>(out.palette.size());
            out.palette.push_back(unkey(k));
        }
    } else {
        // Median cut: keep splitting the box with the widest spread of colour at
        // its middle pixel, until there are 256 boxes. Each box gives one colour.
        std::vector<Box> boxes(1);
        boxes[0].colours.assign(counts.begin(), counts.end());
        while (boxes.size() < 256) {
            int pick = -1, bestSpread = 0, bestCh = 0;
            for (std::size_t i = 0; i < boxes.size(); ++i) {
                if (boxes[i].colours.size() < 2) continue;
                const auto [ch, spread] = boxes[i].widest();
                if (spread > bestSpread) { bestSpread = spread; pick = static_cast<int>(i); bestCh = ch; }
            }
            if (pick < 0) break;
            Box& b = boxes[pick];
            std::sort(b.colours.begin(), b.colours.end(), [&](const auto& x, const auto& y) {
                return b.channel(x.first, bestCh) < b.channel(y.first, bestCh);
            });
            long long total = 0, half = 0;
            for (const auto& c : b.colours) total += c.second;
            std::size_t split = 0;
            while (split < b.colours.size() - 1 && half + b.colours[split].second <= total / 2)
                half += b.colours[split++].second;
            if (split == 0) split = 1;
            Box other;
            other.colours.assign(b.colours.begin() + static_cast<long>(split), b.colours.end());
            b.colours.resize(split);
            boxes.push_back(std::move(other));
        }
        for (const Box& b : boxes) out.palette.push_back(b.average());

        // Each colour in the frame maps to its nearest palette entry.
        for (const auto& [k, c] : counts) {
            const raster::Colour col = unkey(k);
            int best = 0;
            long bestD = 1L << 30;
            for (std::size_t i = 0; i < out.palette.size(); ++i) {
                const long dr = col.r - out.palette[i].r, dg = col.g - out.palette[i].g,
                           db = col.b - out.palette[i].b;
                const long d = dr * dr + dg * dg + db * db;
                if (d < bestD) { bestD = d; best = static_cast<int>(i); }
            }
            index[k] = static_cast<unsigned char>(best);
        }
    }

    out.pixels.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        out.pixels[i] = index[key(raster::Colour{px[i * 4], px[i * 4 + 1], px[i * 4 + 2], 255})];
    while (out.palette.size() < 256) out.palette.push_back(raster::Colour{0, 0, 0, 255});
    return out;
}

std::vector<unsigned char> lzwEncode(const std::vector<unsigned char>& indices) {
    const unsigned minCodeSize = 8;
    const unsigned clearCode = 1u << minCodeSize;   // 256
    const unsigned endCode = clearCode + 1;         // 257

    BitWriter bits;
    std::unordered_map<std::uint32_t, unsigned> dict;   // (prefix code, next index) to code
    dict.reserve(8192);
    unsigned codeSize = minCodeSize + 1;
    unsigned maxCode = endCode;

    bits.write(clearCode, static_cast<int>(codeSize));
    int current = -1;
    for (unsigned char px : indices) {
        if (current < 0) { current = px; continue; }
        const std::uint32_t k = (static_cast<std::uint32_t>(current) << 8) | px;
        const auto it = dict.find(k);
        if (it != dict.end()) { current = static_cast<int>(it->second); continue; }

        // Not in the dictionary: send what we have, and add the longer string.
        bits.write(static_cast<unsigned>(current), static_cast<int>(codeSize));
        dict[k] = ++maxCode;
        if (maxCode >= (1u << codeSize)) ++codeSize;
        if (maxCode == 4095) {
            // The dictionary is full: start again.
            bits.write(clearCode, static_cast<int>(codeSize));
            dict.clear();
            codeSize = minCodeSize + 1;
            maxCode = endCode;
        }
        current = px;
    }
    if (current >= 0) bits.write(static_cast<unsigned>(current), static_cast<int>(codeSize));
    bits.write(clearCode, static_cast<int>(codeSize));
    bits.write(endCode, static_cast<int>(minCodeSize + 1));
    bits.flush();
    return bits.bytes;
}

bool Writer::open(const std::string& path, int width, int height) {
    out_.open(path, std::ios::binary);
    if (!out_) return false;
    width_ = width;
    height_ = height;
    frames_ = 0;

    out_.write("GIF89a", 6);
    put16(out_, width);
    put16(out_, height);
    out_.put(static_cast<char>(0x70));   // no global palette; each frame brings its own
    out_.put(0);                         // background colour index
    out_.put(0);                         // square pixels

    // Loop forever (the Netscape extension every browser understands).
    const unsigned char loopBlock[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E',
                                       '2', '.', '0', 0x03, 0x01, 0x00, 0x00, 0x00};
    out_.write(reinterpret_cast<const char*>(loopBlock), sizeof(loopBlock));
    return static_cast<bool>(out_);
}

void Writer::addFrame(const raster::Canvas& frame, int delayCs) {
    if (!out_ || frame.width() != width_ || frame.height() != height_) return;
    const Indexed img = quantise(frame);

    // Graphic control: how long to show this frame.
    const unsigned char control[] = {0x21, 0xF9, 0x04, 0x04,
                                     static_cast<unsigned char>(delayCs & 0xFF),
                                     static_cast<unsigned char>((delayCs >> 8) & 0xFF), 0x00, 0x00};
    out_.write(reinterpret_cast<const char*>(control), sizeof(control));

    // Image descriptor, with a local palette of 256 colours.
    out_.put(0x2C);
    put16(out_, 0);
    put16(out_, 0);
    put16(out_, width_);
    put16(out_, height_);
    out_.put(static_cast<char>(0x87));
    for (const raster::Colour& c : img.palette) {
        out_.put(static_cast<char>(c.r));
        out_.put(static_cast<char>(c.g));
        out_.put(static_cast<char>(c.b));
    }

    // The compressed pixels, in blocks of at most 255 bytes.
    out_.put(8);
    const std::vector<unsigned char> data = lzwEncode(img.pixels);
    for (std::size_t i = 0; i < data.size(); i += 255) {
        const std::size_t len = std::min<std::size_t>(255, data.size() - i);
        out_.put(static_cast<char>(len));
        out_.write(reinterpret_cast<const char*>(data.data() + i), static_cast<std::streamsize>(len));
    }
    out_.put(0);
    ++frames_;
}

bool Writer::close() {
    if (!out_.is_open()) return false;
    out_.put(0x3B);
    const bool ok = static_cast<bool>(out_);
    out_.close();
    return ok;
}

} // namespace lumen::gif
