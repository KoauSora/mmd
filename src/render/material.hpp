#pragma once

#include <glm/glm.hpp>

namespace render {

class Texture;

struct Material {
    glm::vec4 baseColor {1.0f, 1.0f, 1.0f, 1.0f};
    Texture* diffuse = nullptr;
    Texture* toon = nullptr;
    bool twoSided = false;
    float depthBias = 0.0f;
    float alphaCutoff = 0.0f;
    bool transparentBlend = false;
};

} // namespace render