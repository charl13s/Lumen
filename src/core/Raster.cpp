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
    // General form: works in all eight octants with no special cases.
    int dx =  std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;   // step direction on x
    const int sy = (y0 < y1) ? 1 : -1;   // step direction on y

    // How far the pixel is from the true line, kept as an integer.
    int err = dx + dy;

    for (;;) {
        c.set(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;

        const int e2 = 2 * err;
        if (e2 >= dy) {          // step x
            if (x0 == x1) break;
            err += dy;
            x0  += sx;
        }
        if (e2 <= dx) {          // step y
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
    // Parallel lines offset sideways. Exact for straight walls, close enough on diagonals.
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
    // Decision value at the first midpoint.
    int d = 1 - r;

    plot8(c, cx, cy, x, y, col);
    while (x < y) {
        ++x;
        if (d < 0) {
            // Midpoint inside: keep y.
            d += 2 * x + 1;
        } else {
            // Midpoint outside: step y in.
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

// ---- ellipses --------------------------------------------------------------

namespace {
void plot4(Canvas& c, int cx, int cy, int x, int y, Colour col) {
    c.set(cx + x, cy + y, col);
    c.set(cx - x, cy + y, col);
    c.set(cx + x, cy - y, col);
    c.set(cx - x, cy - y, col);
}
} // namespace

void ellipseMidpoint(Canvas& c, int cx, int cy, int rx, int ry, Colour col) {
    if (rx < 0 || ry < 0) return;
    if (rx == 0 && ry == 0) {
        c.set(cx, cy, col);
        return;
    }

    // long long, because these values overflow an int at normal sizes.
    const long long rx2 = static_cast<long long>(rx) * rx;
    const long long ry2 = static_cast<long long>(ry) * ry;

    long long x = 0;
    long long y = ry;

    // Region 1: start at the top and step x while the curve is flat.
    long long d1 = ry2 - rx2 * ry + rx2 / 4;
    long long dx = 2 * ry2 * x;
    long long dy = 2 * rx2 * y;

    while (dx < dy) {
        plot4(c, cx, cy, static_cast<int>(x), static_cast<int>(y), col);
        ++x;
        dx += 2 * ry2;
        if (d1 < 0) {
            d1 += ry2 + dx;
        } else {
            --y;
            dy -= 2 * rx2;
            d1 += ry2 + dx - dy;
        }
    }

    // Region 2: the curve is steep, so step y instead.
    long long d2 = ry2 * (x * 2 + 1) * (x * 2 + 1) / 4
                 + rx2 * (y - 1) * (y - 1)
                 - rx2 * ry2;

    while (y >= 0) {
        plot4(c, cx, cy, static_cast<int>(x), static_cast<int>(y), col);
        --y;
        dy -= 2 * rx2;
        if (d2 > 0) {
            d2 += rx2 - dy;
        } else {
            ++x;
            dx += 2 * ry2;
            d2 += rx2 - dy + dx;
        }
    }
}

void ellipseFilled(Canvas& c, int cx, int cy, int rx, int ry, Colour col) {
    if (rx < 0 || ry < 0) return;
    // Fills each row using the ellipse equation directly.
    const double frx = (rx > 0) ? static_cast<double>(rx) : 0.5;
    for (int dyi = -ry; dyi <= ry; ++dyi) {
        const double t = (ry > 0) ? static_cast<double>(dyi) / ry : 0.0;
        const double w = frx * std::sqrt(std::max(0.0, 1.0 - t * t));
        const int half = static_cast<int>(std::floor(w + 0.5));
        for (int dxi = -half; dxi <= half; ++dxi)
            c.set(cx + dxi, cy + dyi, col);
    }
}

// ---- polylines -------------------------------------------------------------

void polyline(Canvas& c, const std::vector<Point>& pts, int thickness,
              Colour col, bool closed) {
    if (pts.size() < 2) {
        if (pts.size() == 1) c.set(pts[0].x, pts[0].y, col);
        return;
    }
    for (std::size_t i = 0; i + 1 < pts.size(); ++i)
        lineThick(c, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, thickness, col);
    if (closed)
        lineThick(c, pts.back().x, pts.back().y, pts.front().x, pts.front().y,
                  thickness, col);
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
        if (!(c0 | c1)) return true;    // both inside: keep
        if (c0 & c1)    return false;   // both outside the same edge: drop

        // Move the outside endpoint onto the edge.
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

            // Count the bottom vertex but not the top, so shared vertices count once.
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
