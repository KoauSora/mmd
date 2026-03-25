#pragma once

#include "pose.hpp"
#include "skeleton.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace animation::pose_physics {

// Applies particle positions to the pose by overriding the global translation
// of mapped bones, then recomputing local transforms accordingly.
// particleToBone: particle index -> bone index (or -1).
void applyParticleTranslations(
    const Skeleton& skeleton,
    Pose& inOutPose,
    const std::vector<glm::vec3>& particlePositions,
    const std::vector<int>& particleToBone,
    const std::vector<glm::vec3>* particleBindPos = nullptr,
    const std::vector<std::uint8_t>* particleMask = nullptr,
    float maxGlobalDelta = 5.0f
);

} // namespace animation::pose_physics

