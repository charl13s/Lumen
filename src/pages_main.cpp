// pages_main.cpp - the Checkpoint 1 and 2 pages and animations. Owner: Shawn.
//
// Draws every page and every animation with our own algorithms and saves them:
// pages as .bmp images, animations as animated .gif files, plus a strip of six
// frames from each animation as a .bmp for documents that cannot play a GIF.
// Plain C++ only: no window, no OpenGL, no graphics library. The gallery is main.cpp.
//
//   lumen_pages                       saves into output/
//   lumen_pages --out <folder>        saves somewhere else
//   lumen_pages --pages-only          skips the animations, which take longer
//   lumen_pages <assets folder>       if not run from the project folder

#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

#include "core/Animations.h"
#include "core/Demo.h"
#include "core/Font.h"
#include "core/Gif.h"
#include "core/Image.h"
#include "core/Plan.h"
#include "core/Raster.h"

using namespace lumen;

namespace {

constexpr int kDelayCs = 8;   // each frame shows for 0.08 s, 12.5 frames a second

// Copies a frame into the strip at half size, averaging each 2x2 block.
void pasteHalf(raster::Canvas& strip, const raster::Canvas& frame, int x0, int y0) {
    for (int y = 0; y + 1 < frame.height(); y += 2)
        for (int x = 0; x + 1 < frame.width(); x += 2) {
            int r = 0, g = 0, b = 0;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const raster::Colour p = frame.get(x + dx, y + dy);
                    r += p.r; g += p.g; b += p.b;
                }
            strip.set(x0 + x / 2, y0 + y / 2,
                      raster::Colour{static_cast<std::uint8_t>(r / 4), static_cast<std::uint8_t>(g / 4),
                                     static_cast<std::uint8_t>(b / 4), 255});
        }
}

bool saveAnimation(const demo::Animation& a, const Plan* plan, const std::string& outDir) {
    raster::Canvas frame(a.width, a.height);
    gif::Writer gif;
    const std::string gifPath = outDir + "/anim-" + a.name + ".gif";
    if (!gif.open(gifPath, a.width, a.height)) return false;

    const int frames = static_cast<int>(std::lround(a.seconds * 100.0f / kDelayCs));
    for (int i = 0; i < frames; ++i) {
        a.draw(frame, plan, a.seconds * static_cast<float>(i) / frames);
        gif.addFrame(frame, kDelayCs);
    }
    if (!gif.close()) return false;

    // Six moments from the loop, at half size, three across and two down.
    const int fw = a.width / 2, fh = a.height / 2, gap = 12, label = 18;
    raster::Canvas strip(3 * fw + 4 * gap, 2 * (fh + label) + 3 * gap);
    strip.clear(raster::Colour{20, 17, 13, 255});
    for (int k = 0; k < 6; ++k) {
        const float t = a.seconds * static_cast<float>(k) / 6.0f;
        a.draw(frame, plan, t);
        const int x = gap + (k % 3) * (fw + gap), y = gap + (k / 3) * (fh + label + gap);
        pasteHalf(strip, frame, x, y);
        char text[32];
        std::snprintf(text, sizeof(text), "T = %.1f S", t);
        font::drawText(strip, x, y + fh + 5, text, raster::Colour{154, 143, 114, 255}, 1);
    }
    return image::writeBmp(strip, outDir + "/anim-" + a.name + "-frames.bmp");
}

} // namespace

int main(int argc, char** argv) {
    std::string assetDir = "assets/";
    std::string outDir = "output";
    bool animations = true;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) outDir = argv[++i];
        else if (arg == "--pages-only") animations = false;
        else assetDir = arg;
    }

    // The floor plan. The algorithm pages still work without it.
    Plan plan;
    bool havePlan = false;
    try {
        plan = Plan::load(assetDir + "plan.json");
        havePlan = true;
        std::printf("[lumen] plan loaded: %zu rooms, %zu walls\n", plan.rooms.size(), plan.walls.size());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[lumen] %s\n[lumen] The minimap and plan pages will be empty.\n", e.what());
    }
    const Plan* planPtr = havePlan ? &plan : nullptr;

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);

    raster::Canvas canvas(1280, 720);
    int saved = 0;
    for (int i = 0; i < demo::kPageCount; ++i) {
        const auto page = static_cast<demo::Page>(i);
        demo::renderPage(canvas, page, planPtr);

        char name[64];
        std::snprintf(name, sizeof(name), "page-%d.bmp", i + 1);
        const std::string path = outDir + "/" + name;
        if (image::writeBmp(canvas, path)) {
            std::printf("[lumen] %-40s %s\n", demo::pageTitle(page), path.c_str());
            ++saved;
        } else {
            std::fprintf(stderr, "[lumen] could not write %s\n", path.c_str());
        }
    }
    std::printf("[lumen] %d of %d pages saved.\n", saved, demo::kPageCount);
    bool ok = saved == demo::kPageCount;

    if (animations) {
        for (const demo::Animation& a : demo::animations()) {
            std::printf("[lumen] %-40s %s/anim-%s.gif ... ", a.title, outDir.c_str(), a.name);
            std::fflush(stdout);
            const bool done = saveAnimation(a, planPtr, outDir);
            std::printf("%s\n", done ? "saved" : "FAILED");
            ok = ok && done;
        }
        std::printf("[lumen] Open the .gif files in a web browser or any image viewer to watch them.\n");
    }
    return ok ? 0 : 1;
}
