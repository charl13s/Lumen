// PageKit.h - colours and small drawing helpers shared by every page. Owner: Priscillah.

#pragma once
#include <string>
#include <vector>

#include "Curves.h"
#include "Font.h"
#include "Plan.h"
#include "Raster.h"

namespace lumen::demo {

using raster::Canvas;
using raster::Colour;
using raster::Point;

inline const Colour kBg     {20, 17, 13, 255};
inline const Colour kPanel  {28, 24, 18, 255};
inline const Colour kInk    {240, 229, 198, 255};
inline const Colour kDim    {154, 143, 114, 255};
inline const Colour kGold   {242, 212, 121, 255};
inline const Colour kGoldDim{150, 132, 86, 255};
inline const Colour kTeal   {120, 190, 170, 255};
inline const Colour kCoral  {216, 130, 100, 255};
inline const Colour kBlue   {130, 170, 220, 255};
inline const Colour kRule   {62, 55, 42, 255};

inline constexpr float kPi = 3.14159265358979f;

// A titled box to draw a page section in.
inline void panel(Canvas& c, int x, int y, int w, int h, const std::string& title) {
    const std::vector<Point> box{{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    raster::fillPolygonScanline(c, box, kPanel);
    raster::polyline(c, box, 1, kRule, true);
    font::drawText(c, x + 10, y + 9, title, kDim, 1);
}

inline void caption(Canvas& c, int x, int y, const std::string& s, Colour col = kDim) {
    font::drawText(c, x, y, s, col, 1);
}

// A colour swatch with a label.
inline void legend(Canvas& c, int x, int y, Colour swatch, const std::string& label) {
    raster::lineThick(c, x, y + 3, x + 14, y + 3, 3, swatch);
    font::drawText(c, x + 20, y, label, kDim, 1);
}

// Pages that live in their own files.
void pageComponents(Canvas& c, float time);                       // Components.cpp, Timothy
void pageArtwork(Canvas& c, float time);                          // ArtPage.cpp, Timothy
void pageMinimap(Canvas& c, const Plan* plan, float time);        // MapPages.cpp, Charles
void pageGallery(Canvas& c, const Plan* plan, float time);        // MapPages.cpp, Charles
std::vector<curves::P2> tourPath(const Plan& plan);              // MapPages.cpp, Charles

} // namespace lumen::demo
