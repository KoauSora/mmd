#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace scene {

struct Transform {
    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale {1.0f, 1.0f, 1.0f};

    glm::mat4 matrix() const {
        const glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        const glm::mat4 r = glm::mat4_cast(rotation);
        const glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }
};

} // namespace scene