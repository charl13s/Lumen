// Demo.h - draws the algorithms, and the gallery's components, large and labelled.
// Each page is drawn into a Canvas by our own code and saved as an image.

#pragma once
#include <string>

#include "Plan.h"
#include "Raster.h"

namespace lumen::demo {

enum class Page : int {
    Lines = 0,       // DDA against Bresenham, and the octants
    Circles,         // midpoint circle and midpoint ellipse
    Fills,           // scanline polygon fill and boundary fill
    Clipping,        // Cohen-Sutherland against a viewport
    Polygons,        // polygon properties and Sutherland-Hodgman clipping
    Curves,          // Bezier, de Casteljau, Catmull-Rom
    Components,      // a framed painting, the vault in section, plinth and lamp
    Minimap,         // the minimap around a visitor
    Gallery,         // the plan itself, drawn with all of the above
    Count
};

inline constexpr int kPageCount = static_cast<int>(Page::Count);

const char* pageTitle(Page page);

// Draws one page. `plan` is needed for the Minimap and Gallery pages.
void renderPage(raster::Canvas& canvas, Page page, const Plan* plan);

} // namespace lumen::demo
