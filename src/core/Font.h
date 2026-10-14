// Font.h - a 5x7 bitmap font, drawn pixel by pixel so no text library is needed.
//
// Seven bytes per letter, one per row, using the low five bits.
// Covers A to Z, 0 to 9 and common punctuation. Lower case is drawn as upper case.

#pragma once
#include <string>

#include "Raster.h"

namespace lumen::font {

inline constexpr int kGlyphW = 5;
inline constexpr int kGlyphH = 7;

// Advance per character at scale 1, including the one pixel gap.
inline constexpr int kAdvance = kGlyphW + 1;

// Draws text with its top left at (x, y). scale 2 makes each letter 10x14.
// Returns the x just after the last letter.
int drawText(raster::Canvas& c, int x, int y, const std::string& text,
             raster::Colour col, int scale = 1);

// Width of the text in pixels.
int textWidth(const std::string& text, int scale = 1);

// Draws text centred on cx.
int drawTextCentred(raster::Canvas& c, int cx, int y, const std::string& text,
                    raster::Colour col, int scale = 1);

} // namespace lumen::font
