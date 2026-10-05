#include "Raster.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace lumen::raster {

// ---- Canvas ----------------------------------------------------------------

Canvas::Canvas(int w, int h)
    : w_(w > 0 ? w : 1), h_(h > 0 ? h : 1),
      px_(static_cast<std::size_t>(w_) * h_ * 4, 0) {}

bool Canvas::inside(int x, int y) const {
    return x >= 0 && x < w_ && y >= 0 && y < h_;
}

void Canvas::set(int x, int y, Colour c) {
    if (!inside(x, y)) return;
    const std::size_t i = (static_cast<std::size_t>(y) * w_ + x) * 4;
    px_[i + 0] = c.r;
    px_[i + 1] = c.g;
    px_[i + 2] = c.b;
    px_[i + 3] = c.a;
}

Colour Canvas::get(int x, int y) const {
    if (!inside(x, y)) return Colour{0, 0, 0, 0};
    const std::size_t i = (static_cast<std::size_t>(y) * w_ + x) * 4;
    return Colour{px_[i], px_[i + 1], px_[i + 2], px_[i + 3]};
}

void Canvas::clear(Colour c) {
    for (std::size_t i = 0; i < px_.size(); i += 4) {
        px_[i + 0] = c.r;
        px_[i + 1] = c.g;
        px_[i + 2] = c.b;
        px_[i + 3] = c.a;
    }
}

// ---- lines -----------------------------------------------------------------

void lineDDA(Canvas& c, int x0, int y0, int x1, int y1, Colour col) {
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const int steps = std::max(std::abs(dx), std::abs(dy));
    if (steps == 0) {
        c.set(x0, y0, col);
        return;
    }
    // One of these is +-1 and the other is the fractional slope.
    const float xInc = static_cast<float>(dx) / steps;
    const float yInc = static_cast<float>(dy) / steps;

    float x = static_cast<float>(x0);
    float y = static_cast<float>(y0);
    for (int i = 0; i <= steps; ++i) {
        c.set(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)), col);
        x += xInc;
        y += yInc;
    }
}

void lineBresenham(Canvas& c, int x0, int y0, int x1, int y1, Colour col) {
    // The general ("integer midpoint") form: works in all eight octants without
    // swapping endpoints or special-casing steep lines.
    int dx =  std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;   // step direction on x
    const int sy = (y0 < y1) ? 1 : -1;   // step direction on y

    // err is twice the signed distance from the ideal line to the pixel centre,
    // scaled so that everything stays an integer.
    int err = dx + dy;

    for (;;) {
        c.set(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;

        const int e2 = 2 * err;
        if (e2 >= dy) {          // the line has drifted far enough to step x
            if (x0 == x1) break;
            err += dy;
            x0  += sx;
        }
        if (e2 <= dx) {          // and/or far enough to step y
            if (y0 == y1) break;
            err += dx;
            y0  += sy;
        }
    }
}

void lineThick(Canvas& c, int x0, int y0, int x1, int y1, int thickness, Colour col) {
    if (thickness <= 1) {
        lineBresenham(c, x0, y0, x1, y1, col);
        return;
    }
    // Offset parallel runs along the perpendicular. For axis-aligned walls (which
    // is all of LUMEN) this is exact; for diagonals it is close enough at minimap
    // scale, and it keeps the inner loop pure Bresenham.
    const float dx = static_cast<float>(x1 - x0);
    const float dy = static_cast<float>(y1 - y0);
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-5f) {
        c.set(x0, y0, col);
        return;
    }
    const float px = -dy / len;   // unit perpendicular
    const float py =  dx / len;

    const int half = thickness / 2;
    for (int i = -half; i <= half; ++i) {
        const int ox = static_cast<int>(std::lround(px * i));
        const int oy = static_cast<int>(std::lround(py * i));
        lineBresenham(c, x0 + ox, y0 + oy, x1 + ox, y1 + oy, col);
    }
}

// ---- circles ---------------------------------------------------------------

namespace {
void plot8(Canvas& c, int cx, int cy, int x, int y, Colour col) {
    c.set(cx + x, cy + y, col);
    c.set(cx - x, cy + y, col);
    c.set(cx + x, cy - y, col);
    c.set(cx - x, cy - y, col);
    c.set(cx + y, cy + x, col);
    c.set(cx - y, cy + x, col);
    c.set(cx + y, cy - x, col);
    c.set(cx - y, cy - x, col);
}
} // namespace

void circleMidpoint(Canvas& c, int cx, int cy, int r, Colour col) {
    if (r < 0) return;
    if (r == 0) {
        c.set(cx, cy, col);
        return;
    }
    int x = 0;
    int y = r;
    // Decision variable at the first midpoint, scaled to stay integral.
    int d = 1 - r;

    plot8(c, cx, cy, x, y, col);
    while (x < y) {
        ++x;
        if (d < 0) {
            // Midpoint is inside the circle: keep y, choose the east pixel.
            d += 2 * x + 1;
        } else {
            // Midpoint is outside: step y inward, choose the south-east pixel.
            --y;
            d += 2 * (x - y) + 1;
        }
        plot8(c, cx, cy, x, y, col);
    }
}

void circleFilled(Canvas& c, int cx, int cy, int r, Colour col) {
    if (r < 0) return;
    // Same decision variable, but draw spans instead of points.
    int x = 0, y = r, d = 1 - r;
    auto span = [&](int xa, int xb, int yy) {
        for (int i = xa; i <= xb; ++i) c.set(i, yy, col);
    };
    while (x <= y) {
        span(cx - x, cx + x, cy + y);
        span(cx - x, cx + x, cy - y);
        span(cx - y, cx + y, cy + x);
        span(cx - y, cx + y, cy - x);
        ++x;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            --y;
            d += 2 * (x - y) + 1;
        }
    }
}

// ---- Cohen-Sutherland ------------------------------------------------------

int outcode(float x, float y, float xmin, float ymin, float xmax, float ymax) {
    int code = kInside;
    if (x < xmin)      code |= kLeft;
    else if (x > xmax) code |= kRight;
    if (y < ymin)      code |= kBottom;
    else if (y > ymax) code |= kTop;
    return code;
}

bool clipCohenSutherland(float& x0, float& y0, float& x1, float& y1,
                         float xmin, float ymin, float xmax, float ymax) {
    int c0 = outcode(x0, y0, xmin, ymin, xmax, ymax);
    int c1 = outcode(x1, y1, xmin, ymin, xmax, ymax);

    for (;;) {
        if (!(c0 | c1)) return true;    // both inside: trivially accept
        if (c0 & c1)    return false;   // both share an outside region: reject

        // Pick the endpoint that is outside and pull it to the boundary.
        const int cOut = c0 ? c0 : c1;
        float x = 0.0f, y = 0.0f;

        if (cOut & kTop) {
            x = x0 + (x1 - x0) * (ymax - y0) / (y1 - y0);
            y = ymax;
        } else if (cOut & kBottom) {
            x = x0 + (x1 - x0) * (ymin - y0) / (y1 - y0);
            y = ymin;
        } else if (cOut & kRight) {
            y = y0 + (y1 - y0) * (xmax - x0) / (x1 - x0);
            x = xmax;
        } else {  // kLeft
            y = y0 + (y1 - y0) * (xmin - x0) / (x1 - x0);
            x = xmin;
        }

        if (cOut == c0) {
            x0 = x; y0 = y;
            c0 = outcode(x0, y0, xmin, ymin, xmax, ymax);
        } else {
            x1 = x; y1 = y;
            c1 = outcode(x1, y1, xmin, ymin, xmax, ymax);
        }
    }
}

// ---- area fills ------------------------------------------------------------

void fillPolygonScanline(Canvas& c, const std::vector<Point>& poly, Colour col) {
    const std::size_t n = poly.size();
    if (n < 3) return;

    int yMin = poly[0].y, yMax = poly[0].y;
    for (const Point& p : poly) {
        yMin = std::min(yMin, p.y);
        yMax = std::max(yMax, p.y);
    }
    yMin = std::max(yMin, 0);
    yMax = std::min(yMax, c.height() - 1);

    std::vector<float> xs;
    for (int y = yMin; y <= yMax; ++y) {
        xs.clear();
        const float yc = static_cast<float>(y) + 0.5f;   // sample at pixel centre

        for (std::size_t i = 0; i < n; ++i) {
            const Point& a = poly[i];
            const Point& b = poly[(i + 1) % n];
            const float ay = static_cast<float>(a.y);
            const float by = static_cast<float>(b.y);
            if (std::fabs(by - ay) < 1e-6f) continue;    // horizontal edge: skip

            // Half-open rule [min, max): counts each vertex once, so the parity
            // stays correct where two edges meet.
            const float lo = std::min(ay, by);
            const float hi = std::max(ay, by);
            if (yc < lo || yc >= hi) continue;

            const float t = (yc - ay) / (by - ay);
            xs.push_back(static_cast<float>(a.x) + t * static_cast<float>(b.x - a.x));
        }

        std::sort(xs.begin(), xs.end());
        for (std::size_t i = 0; i + 1 < xs.size(); i += 2) {
            int xa = static_cast<int>(std::ceil(xs[i] - 0.5f));
            int xb = static_cast<int>(std::floor(xs[i + 1] - 0.5f));
            xa = std::max(xa, 0);
            xb = std::min(xb, c.width() - 1);
            for (int x = xa; x <= xb; ++x) c.set(x, y, col);
        }
    }
}

void fillBoundary(Canvas& c, int x, int y, Colour fill, Colour boundary) {
    if (!c.inside(x, y)) return;
    if (fill == boundary) return;              // would never terminate

    std::vector<Point> stack;
    stack.push_back(Point{x, y});

    while (!stack.empty()) {
        const Point p = stack.back();
        stack.pop_back();
        if (!c.inside(p.x, p.y)) continue;

        const Colour here = c.get(p.x, p.y);
        if (here == boundary || here == fill) continue;

        c.set(p.x, p.y, fill);
        stack.push_back(Point{p.x + 1, p.y});
        stack.push_back(Point{p.x - 1, p.y});
        stack.push_back(Point{p.x, p.y + 1});
        stack.push_back(Point{p.x, p.y - 1});
    }
}

} // namespace lumen::raster
