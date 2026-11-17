// Label.cpp - museum labels for paintings and sculpture.

#include "Label.h"

#include <algorithm>
#include <sstream>

#include "Font.h"

namespace lumen::label {
namespace {


const raster::Colour kCard   {244, 240, 231, 255};   // warm white
const raster::Colour kEdge   {214, 207, 194, 255};
const raster::Colour kTitle  { 26,  24,  22, 255};
const raster::Colour kBody   { 52,  48,  43, 255};
const raster::Colour kCredit {112, 105,  95, 255};


int lineHeight(int scale) { return font::kGlyphH * scale + scale * 4; }

// One block of wrapped text, with its scale and colour.
struct Block {
    std::vector<std::string> rows;
    int                      scale;
    raster::Colour           colour;
    int                      gapAbove;
};

} // namespace

std::vector<std::string> wrap(const std::string& text, int maxWidth, int scale) {
    std::vector<std::string> rows;
    std::istringstream words(text);
    std::string word, row;
    while (words >> word) {
        const std::string tryRow = row.empty() ? word : row + " " + word;
        if (!row.empty() && font::textWidth(tryRow, scale) > maxWidth) {
            rows.push_back(row);
            row = word;
        } else {
            row = tryRow;
        }
    }
    if (!row.empty()) rows.push_back(row);
    return rows;
}

raster::Canvas draw(const Text& text, int widthPx, int bodyScale) {
    const int body  = std::max(1, bodyScale);
    const int title = body + 1;
    const int width  = std::max(widthPx, 160);
    const int margin = std::max(20, width / 19);
    const int inner  = width - 2 * margin;

    std::vector<Block> blocks;
    blocks.push_back({wrap(text.title, inner, title), title, kTitle, 0});
    for (std::size_t i = 0; i < text.lines.size(); ++i)
        blocks.push_back({wrap(text.lines[i], inner, body), body, kBody, i == 0 ? 18 : 0});
    if (!text.caption.empty())
        blocks.push_back({wrap(text.caption, inner, body), body, kBody, 28});
    if (!text.credit.empty())
        blocks.push_back({wrap(text.credit, inner, body), body, kCredit, 28});

    int height = 2 * margin;
    for (const Block& b : blocks)
        height += b.gapAbove + static_cast<int>(b.rows.size()) * lineHeight(b.scale);
    height -= 4 * body;   // no spacing under the last row

    raster::Canvas c(width, height);
    c.clear(kCard);
    const int W = c.width() - 1, H = c.height() - 1;
    for (int i : {0, 1}) {
        raster::lineBresenham(c, i, i, W - i, i, kEdge);
        raster::lineBresenham(c, i, H - i, W - i, H - i, kEdge);
        raster::lineBresenham(c, i, i, i, H - i, kEdge);
        raster::lineBresenham(c, W - i, i, W - i, H - i, kEdge);
    }

    int y = margin;
    for (const Block& b : blocks) {
        y += b.gapAbove;
        for (const std::string& row : b.rows) {
            // Drawn twice, half a font pixel apart, for bolder strokes that
            // still read when the card is small on screen.
            font::drawText(c, margin, y, row, b.colour, b.scale);
            font::drawText(c, margin + std::max(1, b.scale / 2), y, row, b.colour, b.scale);
            y += lineHeight(b.scale);
        }
    }
    return c;
}

glm::vec2 fit(const raster::Canvas& card, float maxW, float maxH) {
    const float aspect = static_cast<float>(card.height()) / static_cast<float>(card.width());
    float w = maxW, h = maxW * aspect;
    if (h > maxH) { h = maxH; w = h / aspect; }
    return {w, h};
}

gfx::MeshData quad(const glm::vec3& centre, const glm::vec3& right,
                   const glm::vec3& normal, glm::vec2 size) {
    const glm::vec3 up = glm::normalize(glm::cross(normal, right));
    const glm::vec3 x = right * (size.x * 0.5f), y = up * (size.y * 0.5f);
    gfx::MeshData m;
    m.addQuad(centre - x - y, centre + x - y, centre + x + y, centre - x + y);
    return m;
}

gfx::Material material(const gfx::Texture* card) {
    gfx::Material m;
    m.albedo   = card;
    m.shading  = cfg::Shading::Lambert;   // paper does not shine
    m.specular = 0.02f;
    return m;
}

} // namespace lumen::label
