#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

namespace animation {

struct JointPose {
    glm::vec3 translation {0.0f};
    glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale {1.0f, 1.0f, 1.0f};
};

struct Pose {
    std::vector<JointPose> local;
    std::vector<glm::mat4> global;

    void resize(std::size_t count) {
        local.resize(count);
        global.resize(count, glm::mat4(1.0f));
    }
};

} // namespace animation