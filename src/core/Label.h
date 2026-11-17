// Label.h - museum labels for paintings and sculpture.
//
// A label is a warm white card with the title, the artist and date, the
// medium, a short note and the credit line, like the cards beside works in a
// real gallery. The card is drawn by our own 2D code with the bitmap font,
// then shown as a texture on a flat quad.

#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx.h"
#include "Raster.h"

namespace lumen::label {

struct Text {
    std::string              title;
    std::vector<std::string> lines;     // artist and date, then the medium
    std::string              caption;   // a sentence or two, wrapped to fit
    std::string              credit;    // small, at the foot
};

// The card, as tall as the words need. bodyScale sizes the font (the title is
// one step bigger). Larger letters on a narrower card read bigger on the wall.
raster::Canvas draw(const Text& text, int widthPx = 768, int bodyScale = 3);

// Breaks text into lines no wider than maxWidth pixels at this font scale.
// A single word that is too long gets a line of its own.
std::vector<std::string> wrap(const std::string& text, int maxWidth, int scale);

// The largest card of this canvas's shape inside maxW by maxH metres.
glm::vec2 fit(const raster::Canvas& card, float maxW, float maxH);

// A flat card centred at `centre`, facing along `normal`, with `right` along
// its width. Textured so a Canvas reads the right way round.
gfx::MeshData quad(const glm::vec3& centre, const glm::vec3& right,
                   const glm::vec3& normal, glm::vec2 size);

// A matte material for a card drawn with draw().
gfx::Material material(const gfx::Texture* card);

} // namespace lumen::label
