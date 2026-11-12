// Animations.h - every animation the program saves. Owner: Shawn.
//
// Each animation draws one frame for a time in seconds. The main program steps
// the time forward and saves the frames as an animated GIF.

#pragma once
#include <vector>

#include "Plan.h"
#include "Raster.h"

namespace lumen::demo {

struct Animation {
    const char* name;     // file name: output/anim-<name>.gif
    const char* title;
    float seconds;        // one full loop
    int width, height;
    void (*draw)(raster::Canvas& c, const Plan* plan, float t);
};

const std::vector<Animation>& animations();

// Drawn in their owners' files.
void animTransforms(raster::Canvas& c, const Plan* plan, float t);   // TransformPage.cpp, Priscillah
void animTour(raster::Canvas& c, const Plan* plan, float t);         // MapPages.cpp, Charles
void animArtwork(raster::Canvas& c, const Plan* plan, float t);      // ArtPage.cpp, Timothy
void animLamp(raster::Canvas& c, const Plan* plan, float t);         // LampAnim.cpp, Timothy
void animRoom3D(raster::Canvas& c, const Plan* plan, float t);       // Room3D.cpp, Erykah

} // namespace lumen::demo
