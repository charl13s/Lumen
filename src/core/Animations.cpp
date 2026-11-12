#include "Animations.h"

namespace lumen::demo {

const std::vector<Animation>& animations() {
    static const std::vector<Animation> list{
        {"transforms", "2D transformations", 8.0f, 640, 400, animTransforms},
        {"room3d", "Gallery Two in 3D", 8.0f, 640, 400, animRoom3D},
        {"tour", "The guided tour", 12.0f, 640, 420, animTour},
        {"lamp", "The lamp", 6.0f, 640, 400, animLamp},
        {"artwork", "Gallery Three, moving", 8.0f, 1150, 340, animArtwork},
    };
    return list;
}

} // namespace lumen::demo
