#pragma once

#include "pose.hpp"
#include "skeleton.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace animation::skinning {

void buildSkinningMatrices(
    const Skeleton& skeleton,
    const Pose& pose,
    std::vector<glm::mat4>& outMatrices
);

} // namespace animation::skinning