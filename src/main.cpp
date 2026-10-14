// main.cpp - LUMEN, Checkpoint 1. Owner: Shawn.
//
// Draws every page with our own algorithms and saves it as a .bmp image.
// Plain C++ only: no window, no OpenGL, no graphics library.
//
//   lumen_cp1                       saves into output/
//   lumen_cp1 --out <folder>        saves somewhere else
//   lumen_cp1 <assets folder>       if not run from the project folder

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

#include "core/Demo.h"
#include "core/Image.h"
#include "core/Plan.h"
#include "core/Raster.h"

using namespace lumen;

int main(int argc, char** argv) {
    std::string assetDir = "assets/";
    std::string outDir = "output";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) outDir = argv[++i];
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

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);

    raster::Canvas canvas(1280, 720);
    int saved = 0;
    for (int i = 0; i < demo::kPageCount; ++i) {
        const auto page = static_cast<demo::Page>(i);
        demo::renderPage(canvas, page, havePlan ? &plan : nullptr);

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
    std::printf("[lumen] %d of %d pages saved. Open them in any image viewer.\n",
                saved, demo::kPageCount);
    return saved == demo::kPageCount ? 0 : 1;
}
