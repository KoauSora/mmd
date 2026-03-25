#include "ik_solver.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace animation::ik {
namespace {

static glm::mat4 poseToMatrix(const JointPose& pose) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, pose.translation);
    m *= glm::mat4_cast(pose.rotation);
    m = glm::scale(m, pose.scale);
    return m;
}

static glm::quat rotationFromMatrix(const glm::mat4& m) {
    // glm::quat_cast expects a pure rotation matrix; our globals can contain scale=1 so ok.
    return glm::normalize(glm::quat_cast(m));
}

static glm::vec3 translationFromMatrix(const glm::mat4& m) {
    return glm::vec3(m[3]);
}

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

static void rebuildSubtree(const Skeleton& skeleton, Pose& pose, int rootIndex) {
    const auto& bones = skeleton.bones();
    const auto& children = skeleton.children();
    const int n = static_cast<int>(bones.size());
    if (rootIndex < 0 || rootIndex >= n) {
        return;
    }
    if (pose.local.size() != bones.size()) {
        pose.resize(bones.size());
    }

    // Update global of rootIndex from its parent, then propagate to descendants.
    const int parentIndex = bones[static_cast<std::size_t>(rootIndex)].parentIndex;
    const glm::mat4 localRoot = poseToMatrix(pose.local[static_cast<std::size_t>(rootIndex)]);
    if (parentIndex >= 0 && parentIndex < n) {
        pose.global[static_cast<std::size_t>(rootIndex)] =
            pose.global[static_cast<std::size_t>(parentIndex)] * localRoot;
    } else {
        pose.global[static_cast<std::size_t>(rootIndex)] = localRoot;
    }

    std::vector<int> stack;
    stack.reserve(64);
    stack.push_back(rootIndex);
    while (!stack.empty()) {
        const int u = stack.back();
        stack.pop_back();
        const glm::mat4 parentGlobal = pose.global[static_cast<std::size_t>(u)];
        for (const int v : children[static_cast<std::size_t>(u)]) {
            if (v < 0 || v >= n) {
                continue;
            }
            const glm::mat4 local = poseToMatrix(pose.local[static_cast<std::size_t>(v)]);
            pose.global[static_cast<std::size_t>(v)] = parentGlobal * local;
            stack.push_back(v);
        }
    }
}

static glm::quat quatFromTo(const glm::vec3& from, const glm::vec3& to) {
    const float fromLen = glm::length(from);
    const float toLen = glm::length(to);
    if (fromLen < 1e-6f || toLen < 1e-6f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    const glm::vec3 f = from / fromLen;
    const glm::vec3 t = to / toLen;
    const float d = std::clamp(glm::dot(f, t), -1.0f, 1.0f);

    if (d > 0.999999f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    if (d < -0.999999f) {
        // 180-degree turn: pick any orthogonal axis
        glm::vec3 axis = glm::cross(f, glm::vec3(1.0f, 0.0f, 0.0f));
        if (glm::length(axis) < 1e-6f) {
            axis = glm::cross(f, glm::vec3(0.0f, 1.0f, 0.0f));
        }
        axis = glm::normalize(axis);
        return glm::angleAxis(glm::pi<float>(), axis);
    }

    const glm::vec3 axis = glm::normalize(glm::cross(f, t));
    const float angle = std::acos(d);
    return glm::angleAxis(angle, axis);
}

static glm::quat clampAngle(const glm::quat& q, float maxAngleRad) {
    if (maxAngleRad <= 0.0f) {
        return q;
    }
    const glm::quat nq = glm::normalize(q);
    const float w = std::clamp(nq.w, -1.0f, 1.0f);
    float angle = 2.0f * std::acos(w);
    if (angle <= maxAngleRad) {
        return nq;
    }
    // Scale the rotation angle down.
    glm::vec3 axis(nq.x, nq.y, nq.z);
    const float axisLen = glm::length(axis);
    if (axisLen < 1e-6f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    axis /= axisLen;
    return glm::angleAxis(maxAngleRad, axis);
}

} // namespace

void solve(const Skeleton& skeleton, Pose& inOutPose) {
    const auto& constraints = skeleton.ikConstraints();
    if (constraints.empty()) {
        return;
    }

    const auto& bones = skeleton.bones();
    if (inOutPose.local.size() != bones.size()) {
        inOutPose.resize(bones.size());
    }

    // Ensure globals are up-to-date before solving.
    rebuildGlobals(skeleton, inOutPose);

    for (const auto& c : constraints) {
        if (c.ikBoneIndex < 0 || c.targetBoneIndex < 0) {
            continue;
        }

        const glm::vec3 goalPos = translationFromMatrix(inOutPose.global[static_cast<std::size_t>(c.ikBoneIndex)]);
        // PMX files sometimes carry very high loop counts. Clamping keeps runtime predictable.
        const int iterations = std::clamp(c.loopCount, 1, 16);
        const float stepMax = c.limitAngle; // PMX convention: radians

        for (int it = 0; it < iterations; ++it) {
            bool anyChange = false;

            // Early out if already close enough.
            {
                const glm::vec3 effPos = translationFromMatrix(inOutPose.global[static_cast<std::size_t>(c.targetBoneIndex)]);
                const glm::vec3 d = goalPos - effPos;
                const float err2 = glm::dot(d, d);
                if (err2 < 1e-6f) { // ~1mm if units are meters-ish; safe generic epsilon
                    break;
                }
            }

            for (std::size_t li = 0; li < c.links.size(); ++li) {
                const int linkIndex = c.links[li];
                if (linkIndex < 0) {
                    continue;
                }

                const std::size_t link = static_cast<std::size_t>(linkIndex);
                const std::size_t eff = static_cast<std::size_t>(c.targetBoneIndex);

                const glm::vec3 linkPos = translationFromMatrix(inOutPose.global[link]);
                const glm::vec3 effPos = translationFromMatrix(inOutPose.global[eff]);

                const glm::vec3 vCur = effPos - linkPos;
                const glm::vec3 vGoal = goalPos - linkPos;

                glm::quat deltaWorld = quatFromTo(vCur, vGoal);
                deltaWorld = clampAngle(deltaWorld, stepMax);
                if (glm::length(glm::vec3(deltaWorld.x, deltaWorld.y, deltaWorld.z)) < 1e-8f) {
                    continue;
                }

                // Convert world-space delta to the link's local-space delta.
                const int parentIndex = bones[link].parentIndex;
                glm::quat parentGlobalRot(1.0f, 0.0f, 0.0f, 0.0f);
                if (parentIndex >= 0) {
                    parentGlobalRot = rotationFromMatrix(inOutPose.global[static_cast<std::size_t>(parentIndex)]);
                }

                const glm::quat deltaLocal =
                    glm::inverse(parentGlobalRot) * deltaWorld * parentGlobalRot;

                inOutPose.local[link].rotation =
                    glm::normalize(deltaLocal * inOutPose.local[link].rotation);

                // Rebuild globals only for the affected subtree (link and its descendants).
                rebuildSubtree(skeleton, inOutPose, static_cast<int>(link));
                anyChange = true;
            }

            if (!anyChange) {
                break;
            }
        }
    }
}

} // namespace animation::ik

