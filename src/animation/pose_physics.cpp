#include "pose_physics.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstddef>
// #include <unordered_map>

#include "log.hpp"

namespace animation::pose_physics {
namespace {

static glm::mat4 poseToMatrix(const JointPose& pose) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, pose.translation);
    m *= glm::mat4_cast(pose.rotation);
    m = glm::scale(m, pose.scale);
    return m;
}

// static glm::vec3 translationFromMatrix(const glm::mat4& m) {
//     return glm::vec3(m[3]);
// }

// static glm::quat quatFromTo(const glm::vec3& from, const glm::vec3& to) {
//     const float fromLen = glm::length(from);
//     const float toLen = glm::length(to);
//     if (fromLen < 1e-6f || toLen < 1e-6f) {
//         return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
//     }
//     const glm::vec3 f = from / fromLen;
//     const glm::vec3 t = to / toLen;
//     const float d = std::clamp(glm::dot(f, t), -1.0f, 1.0f);
//     if (d > 0.999999f) {
//         return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
//     }
//     if (d < -0.999999f) {
//         glm::vec3 axis = glm::cross(f, glm::vec3(1.0f, 0.0f, 0.0f));
//         if (glm::length(axis) < 1e-6f) {
//             axis = glm::cross(f, glm::vec3(0.0f, 1.0f, 0.0f));
//         }
//         axis = glm::normalize(axis);
//         return glm::angleAxis(glm::pi<float>(), axis);
//     }
//     const glm::vec3 axis = glm::normalize(glm::cross(f, t));
//     const float angle = std::acos(d);
//     return glm::angleAxis(angle, axis);
// }

// static glm::mat4 composeTR(const glm::vec3& t, const glm::quat& r) {
//     glm::mat4 m = glm::mat4_cast(glm::normalize(r));
//     m[3] = glm::vec4(t, 1.0f);
//     return m;
// }

// static void decomposeTRS(const glm::mat4& m, glm::vec3& t, glm::quat& r, glm::vec3& s) {
//     t = translationFromMatrix(m);
//     // Assume uniform/no scale in skeleton usage; keep existing scale.
//     r = glm::normalize(glm::quat_cast(m));
//     s = glm::vec3(1.0f);
// }

static void rebuildGlobals(const Skeleton& skeleton, Pose& pose) {
    const auto& bones = skeleton.bones();
    const std::size_t n = bones.size();
    if (pose.local.size() != n) {
        pose.resize(n);
    }

    for (std::size_t i = 0; i < n; ++i) {
        const glm::mat4 local = poseToMatrix(pose.local[i]);
        const int parentIndex = bones[i].parentIndex;
        if (parentIndex >= 0) {
            pose.global[i] = pose.global[static_cast<std::size_t>(parentIndex)] * local;
        } else {
            pose.global[i] = local;
        }
    }
}

} // namespace

void applyParticleTranslations(
    const Skeleton& skeleton,
    Pose& inOutPose,
    const std::vector<glm::vec3>& particlePositions,
    const std::vector<int>& particleToBone,
    const std::vector<glm::vec3>* particleBindPos,
    const std::vector<std::uint8_t>* particleMask,
    float maxGlobalDelta
) {
    const auto& bones = skeleton.bones();
    if (bones.empty()) {
        return;
    }

    if (inOutPose.local.size() != bones.size()) {
        inOutPose.resize(bones.size());
    }
    if (inOutPose.global.size() != bones.size()) {
        inOutPose.resize(bones.size());
    }

    rebuildGlobals(skeleton, inOutPose);

    std::vector<glm::vec3> worldDeltaSum(bones.size(), glm::vec3(0.0f));
    std::vector<int> worldDeltaCount(bones.size(), 0);

    const std::size_t n = std::min(particlePositions.size(), particleToBone.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (particleMask != nullptr) {
            if (i >= particleMask->size() || (*particleMask)[i] == 0) {
                continue;
            }
        }

        const int boneIndex = particleToBone[i];
        if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size())) {
            continue;
        }

        if (particleBindPos == nullptr || i >= particleBindPos->size()) {
            continue;
        }

        glm::vec3 worldDelta = particlePositions[i] - (*particleBindPos)[i];
        const float len = glm::length(worldDelta);
        if (len < 1e-6f) {
            continue;
        }

        if (len > maxGlobalDelta) {
            worldDelta *= (maxGlobalDelta / len);
        }

        worldDeltaSum[static_cast<std::size_t>(boneIndex)] += worldDelta;
        worldDeltaCount[static_cast<std::size_t>(boneIndex)] += 1;
    }

    std::size_t appliedCount = 0;
    std::size_t rejectedCount = 0;

    // 只做温和的局部平移叠加，不碰 rotation。
    constexpr float kBlend = 0.35f;

    for (std::size_t i = 0; i < bones.size(); ++i) {
        if (worldDeltaCount[i] == 0) {
            continue;
        }

        glm::vec3 worldDelta = worldDeltaSum[i] / static_cast<float>(worldDeltaCount[i]);

        const int parentIndex = bones[i].parentIndex;
        const glm::mat4 parentGlobal =
            (parentIndex >= 0)
                ? inOutPose.global[static_cast<std::size_t>(parentIndex)]
                : glm::mat4(1.0f);

        glm::vec3 localDelta = glm::vec3(glm::inverse(parentGlobal) * glm::vec4(worldDelta, 0.0f));

        const float localLen = glm::length(localDelta);
        if (localLen > maxGlobalDelta) {
            localDelta *= (maxGlobalDelta / localLen);
            ++rejectedCount;
        }

        inOutPose.local[i].translation += localDelta * kBlend;
        ++appliedCount;
    }

    if (appliedCount == 0) {
        return;
    }

    rebuildGlobals(skeleton, inOutPose);

    static std::size_t s_frame = 0;
    ++s_frame;
    if ((s_frame % 60u) == 0u) {
        core::log::info(
            "Physics pose apply(local-only): applied=" + std::to_string(appliedCount) +
            " rejected=" + std::to_string(rejectedCount) +
            " maxDelta=" + std::to_string(maxGlobalDelta)
        );
    }
}

} // namespace animation::pose_physics

