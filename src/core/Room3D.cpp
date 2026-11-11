// Room3D.cpp - Gallery Two in 3D, as a page and as an animation. Owner: Erykah.
//
// The room is built from the floor plan as a wireframe, then moved, turned and
// projected with 4x4 matrices. Every line is still drawn by our own Bresenham.

#include <cmath>
#include <cstdio>
#include <string>

#include "PageKit.h"
#include "Transform3D.h"

namespace lumen::demo {
namespace {

using xf3::Mat4;
using xf3::Vec3;

const RoomPlan* galleryTwo(const Plan& plan) {
    for (const RoomPlan& r : plan.rooms)
        if (r.id == "R2") return &r;
    return plan.rooms.empty() ? nullptr : &plan.rooms.front();
}

// The room moved so its centre sits on the origin, ready to be turned.
Mat4 centred(const RoomPlan& r) {
    const Vec2 c = r.rect.centre();
    return xf3::translation(-c.x, 0.0f, c.y);
}

// Draws the room in a rectangle of the canvas, seen through `view`.
void drawRoom(Canvas& c, int x, int y, int w, int h, const Plan& plan, const Mat4& model,
              const Mat4& view) {
    const RoomPlan* room = galleryTwo(plan);
    if (!room) return;
    Canvas view2d(w, h);
    view2d.clear(kPanel);
    const Mat4 proj = xf3::perspective(50.0f * kPi / 180.0f, static_cast<float>(w) / h, 0.1f, 100.0f);
    const xf3::Wire wire = xf3::roomWire(*room, plan.wallHeight, plan.vaultApex);
    xf3::drawWire(view2d, wire, proj * view * model, kInk);
    for (int yy = 0; yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx)
            if (view2d.get(xx, yy) != kPanel) c.set(x + xx, y + yy, view2d.get(xx, yy));
}

} // namespace

// ---- the page ----------------------------------------------------------------

void page3D(Canvas& c, const Plan* plan, float /*time*/) {
    struct View {
        const char* title;
        const char* note;
        float turn;     // about the vertical axis, degrees
        Vec3 eye;
    };
    const View views[] = {
        {"FROM THE DOOR", "CAMERA SOUTH OF THE ROOM, NO TURN", 0.0f, {0, 2.5f, 13}},
        {"TURNED 45 DEGREES", "MODEL ROTATED ABOUT Y BY 45", 45.0f, {0, 3.0f, 14}},
        {"TURNED 120 DEGREES", "MODEL ROTATED ABOUT Y BY 120", 120.0f, {0, 3.0f, 14}},
        {"FROM ABOVE", "CAMERA HIGH, LOOKING DOWN INTO THE VAULTS", 30.0f, {0, 16.0f, 7}},
    };
    for (int i = 0; i < 4; ++i) {
        const int px = 30 + (i % 2) * 620, py = 60 + (i / 2) * 280;
        panel(c, px, py, 600, 265, views[i].title);
        if (!plan) continue;
        const RoomPlan* room = galleryTwo(*plan);
        if (!room) continue;
        const Mat4 model = xf3::rotationY(views[i].turn * kPi / 180.0f) * centred(*room);
        const Mat4 view = xf3::lookAt(views[i].eye, Vec3{0, 2.2f, 0});
        drawRoom(c, px + 10, py + 24, 580, 205, *plan, model, view);
        caption(c, px + 12, py + 240, views[i].note);
    }
}

// ---- the animation -----------------------------------------------------------

// The camera stays still while the room turns once about its vertical axis.
void animRoom3D(Canvas& c, const Plan* plan, float t) {
    c.clear(kBg);
    font::drawText(c, 20, 18, "GALLERY TWO IN 3D", kGold, 2);
    if (!plan) return;
    const RoomPlan* room = galleryTwo(*plan);
    if (!room) return;

    const float angle = 2.0f * kPi * t / 8.0f;
    const Mat4 model = xf3::rotationY(angle) * centred(*room);
    const Mat4 view = xf3::lookAt(Vec3{0, 5.0f, 17.5f}, Vec3{0, 2.6f, 0});
    const Mat4 proj = xf3::perspective(50.0f * kPi / 180.0f, static_cast<float>(c.width()) / c.height(), 0.1f, 100.0f);
    xf3::drawWire(c, xf3::roomWire(*room, plan->wallHeight, plan->vaultApex), proj * view * model, kInk);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "ROTATION ABOUT Y: %3.0f DEGREES", angle * 180.0f / kPi);
    font::drawText(c, 20, 44, buf, kDim, 1);
    font::drawText(c, 20, c.height() - 24, "PROJECTION X VIEW X ROTATION X MOVE TO ORIGIN", kDim, 1);
}

} // namespace lumen::demo
