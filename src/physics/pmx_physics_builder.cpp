#include "pmx_physics_builder.hpp"

#include "log.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstddef>
#include <queue>
#include <string_view>
#include <vector>

namespace physics::pbd {
namespace {

static glm::vec3 translationFromMatrix(const glm::mat4& m) {
    return glm::vec3(m[3]);
}

static std::vector<std::size_t> computeInfluencedVertexCounts(
    const asset::PmxAsset& pmx,
    std::size_t boneCount
) {
    std::vector<std::size_t> influenceCount(boneCount, 0);
    for (const auto& v : pmx.vertices) {
        const int ids[4] = {v.boneIds.x, v.boneIds.y, v.boneIds.z, v.boneIds.w};
        const float ws[4] = {v.boneWeights.x, v.boneWeights.y, v.boneWeights.z, v.boneWeights.w};
        for (int k = 0; k < 4; ++k) {
            const int idx = ids[k];
            if (idx < 0 || idx >= static_cast<int>(influenceCount.size())) {
                continue;
            }
            if (ws[k] <= 0.0f) {
                continue;
            }
            influenceCount[static_cast<std::size_t>(idx)]++;
        }
    }
    return influenceCount;
}

static glm::mat4 rigidLocalTransform(const asset::PmxRigidBody& rb) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, rb.shapePosition);
    const glm::quat q =
        glm::angleAxis(rb.shapeRotation.z, glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::angleAxis(rb.shapeRotation.y, glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::angleAxis(rb.shapeRotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    m *= glm::mat4_cast(glm::normalize(q));
    return m;
}

static bool containsAny(std::string_view s, std::initializer_list<std::string_view> needles) {
    for (const auto n : needles) {
        if (!n.empty() && s.find(n) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

static bool isClothLikeName(std::string_view name) {
    // Conservative defaults for clothing/hair/accessories that often use PMX physics.
    return containsAny(name, {
        "cloth", "skirt", "cape", "coat", "scarf",
        "スカート", "マント", "ケープ", "コート", "マフラー",
        "裙", "裙子", "下摆", "下擺", "披风", "披風", "袖",
        "髪", "髮", "hair"
    });
}

} // namespace

bool buildFromPmxPhysics(
    const asset::PmxAsset& pmx,
    const animation::Skeleton& skeleton,
    const animation::Pose& pose,
    World& outWorld,
    PmxPhysicsBinding& outBinding,
    const BuilderConfig& cfg
) {
    outWorld.clear();
    outBinding = {};

    if (pmx.rigidBodies.empty()) {
        return false;
    }
    if (pmx.joints.empty() && !cfg.allowNoJoints) {
        return false;
    }

    const std::size_t rbCount = pmx.rigidBodies.size();
    outBinding.rigidBodyToParticle.assign(rbCount, -1);
    std::vector<std::uint8_t> rbSelected(rbCount, 1);
    if (cfg.filterClothLike) {
        std::fill(rbSelected.begin(), rbSelected.end(), 0);
        const auto& bones = skeleton.bones();
        for (std::size_t i = 0; i < rbCount; ++i) {
            const auto& rb = pmx.rigidBodies[i];
            bool ok = isClothLikeName(rb.name);
            if (!ok && rb.boneIndex >= 0 && rb.boneIndex < static_cast<int>(bones.size())) {
                ok = isClothLikeName(bones[static_cast<std::size_t>(rb.boneIndex)].name);
            }
            rbSelected[i] = ok ? 1 : 0;
        }
    }

    if (cfg.filterClothLike) {
        std::size_t selectedCount = 0;
        for (const auto v : rbSelected) {
            selectedCount += (v != 0) ? 1u : 0u;
        }
        if (selectedCount == 0) {
            // Name-based filtering is brittle. Fall back to a safer heuristic:
            // select bodies that can actually affect pose (mode 1/2) and are bound to a bone.
            core::log::warn("Physics filter selected 0 rigid bodies. Fallback to mode-based selection.");
            for (std::size_t i = 0; i < rbCount; ++i) {
                const auto& rb = pmx.rigidBodies[i];
                const bool ok = (rb.mode == 1 || rb.mode == 2) && (rb.boneIndex >= 0);
                rbSelected[i] = ok ? 1 : 0;
            }
            selectedCount = 0;
            for (const auto v : rbSelected) {
                selectedCount += (v != 0) ? 1u : 0u;
            }
            if (selectedCount == 0) {
                core::log::warn("Physics fallback selection still 0. Disable physics.");
                return false;
            }
        }
    }

    // Create particles
    for (std::size_t i = 0; i < rbCount; ++i) {
        if (rbSelected[i] == 0) {
            continue;
        }
        const auto& rb = pmx.rigidBodies[i];

        glm::vec3 pos(0.0f);
        if (rb.boneIndex >= 0 && rb.boneIndex < static_cast<int>(pose.global.size())) {
            const glm::mat4 boneG = pose.global[static_cast<std::size_t>(rb.boneIndex)];
            const glm::mat4 world = boneG * rigidLocalTransform(rb);
            pos = translationFromMatrix(world);
        }

        Particle p;
        p.pos = pos;
        p.prevPos = pos;
        p.invMass = cfg.defaultInvMass;
        if (rb.mode == 0) { // follow bone / kinematic
            p.invMass = 0.0f;
        }

        const int pid = outWorld.addParticle(p);
        outBinding.rigidBodyToParticle[i] = pid;
    }

    // particle -> bone mapping
    outBinding.particleToBone.assign(outWorld.particles().size(), -1);
    outBinding.particleBindPos.assign(outWorld.particles().size(), glm::vec3(0.0f));
    outBinding.particleAffectsPose.assign(outWorld.particles().size(), 0);
    std::size_t affectsCount = 0;
    std::size_t mode0Count = 0;
    std::size_t mode1Count = 0;
    std::size_t mode2Count = 0;

    const auto influenceCount = computeInfluencedVertexCounts(pmx, skeleton.bones().size());
    std::size_t retargetedBones = 0;

    // Build reachability from pinned anchors (mode==0) within the selected rigid-body graph.
    // This is used to safely allow mode==1 writeback only for chains attached to anchors.
    std::vector<std::uint8_t> reachableFromAnchors(rbCount, 0);
    if ((cfg.allowWritebackMode1 || cfg.enableWritebackFallback) && !pmx.joints.empty()) {
        std::vector<std::vector<int>> adj(rbCount);
        adj.reserve(rbCount);
        for (const auto& j : pmx.joints) {
            if (j.rigidBodyA < 0 || j.rigidBodyB < 0) {
                continue;
            }
            if (j.rigidBodyA >= static_cast<int>(rbCount) ||
                j.rigidBodyB >= static_cast<int>(rbCount)) {
                continue;
            }
            const std::size_t a = static_cast<std::size_t>(j.rigidBodyA);
            const std::size_t b = static_cast<std::size_t>(j.rigidBodyB);
            if (rbSelected[a] == 0 || rbSelected[b] == 0) {
                continue;
            }
            if (outBinding.rigidBodyToParticle[a] < 0 || outBinding.rigidBodyToParticle[b] < 0) {
                continue;
            }
            adj[a].push_back(static_cast<int>(b));
            adj[b].push_back(static_cast<int>(a));
        }

        std::queue<int> q;
        for (std::size_t i = 0; i < rbCount; ++i) {
            if (rbSelected[i] == 0) {
                continue;
            }
            if (outBinding.rigidBodyToParticle[i] < 0) {
                continue;
            }
            if (pmx.rigidBodies[i].mode != 0) {
                continue;
            }
            reachableFromAnchors[i] = 1;
            q.push(static_cast<int>(i));
        }
        while (!q.empty()) {
            const int u = q.front();
            q.pop();
            for (const int v : adj[static_cast<std::size_t>(u)]) {
                const std::size_t vi = static_cast<std::size_t>(v);
                if (reachableFromAnchors[vi] != 0) {
                    continue;
                }
                reachableFromAnchors[vi] = 1;
                q.push(v);
            }
        }
    }

    for (std::size_t i = 0; i < rbCount; ++i) {
        const int pid = outBinding.rigidBodyToParticle[i];
        if (pid < 0) {
            continue;
        }
        int boneIndex = pmx.rigidBodies[i].boneIndex;

        // Prefer mapping to a bone that actually influences vertices.
        // Many rigs attach rigid bodies to control bones (0 weights) while deform bones use suffixes like "D".
        if (boneIndex >= 0 &&
            boneIndex < static_cast<int>(skeleton.bones().size()) &&
            influenceCount[static_cast<std::size_t>(boneIndex)] == 0) {
            const auto& boneName = skeleton.bones()[static_cast<std::size_t>(boneIndex)].name;
            const std::string candidate = boneName + "D";
            const int b2 = skeleton.findBoneIndex(candidate);
            if (b2 >= 0 && b2 < static_cast<int>(skeleton.bones().size()) &&
                influenceCount[static_cast<std::size_t>(b2)] > 0) {
                boneIndex = b2;
                ++retargetedBones;
            }
        }

        const bool mappedBoneDeforms =
    boneIndex >= 0 &&
    boneIndex < static_cast<int>(skeleton.bones().size()) &&
    influenceCount[static_cast<std::size_t>(boneIndex)] > 0;

    // 只把真正会影响顶点的骨头纳入 writeback。
    // 如果最终还是控制骨/辅助骨，就不要回写。
    outBinding.particleToBone[static_cast<std::size_t>(pid)] =
        mappedBoneDeforms ? boneIndex : -1;

    // Store bind reference positions for stable delta writeback.
    outBinding.particleBindPos[static_cast<std::size_t>(pid)] =
        outWorld.particles()[static_cast<std::size_t>(pid)].pos;

    if (pmx.rigidBodies[i].mode == 0) {
        ++mode0Count;
    } else if (pmx.rigidBodies[i].mode == 1) {
        ++mode1Count;
    } else if (pmx.rigidBodies[i].mode == 2) {
        ++mode2Count;
    }

    const int mode = pmx.rigidBodies[i].mode;

    // 只有真正 deform 的骨头才允许写回。
    bool affects = mappedBoneDeforms && (rbSelected[i] != 0) && (mode == 2);

    if (!affects && cfg.allowWritebackMode1) {
        affects =
            mappedBoneDeforms &&
            (rbSelected[i] != 0) &&
            (mode == 1) &&
            (reachableFromAnchors[i] != 0);
    }

    if (affects && cfg.filterClothLike) {
        const auto& bones = skeleton.bones();
        bool ok = isClothLikeName(pmx.rigidBodies[i].name);
        if (!ok && boneIndex >= 0 && boneIndex < static_cast<int>(bones.size())) {
            ok = isClothLikeName(bones[static_cast<std::size_t>(boneIndex)].name);
        }
        affects = ok;
    }

    outBinding.particleAffectsPose[static_cast<std::size_t>(pid)] = affects ? 1 : 0;
    affectsCount += affects ? 1u : 0u;
        if (affects && cfg.filterClothLike) {
            // Extra safety: require name hit either on rigidbody or bone.
            const auto& bones = skeleton.bones();
            bool ok = isClothLikeName(pmx.rigidBodies[i].name);
            if (!ok && boneIndex >= 0 && boneIndex < static_cast<int>(bones.size())) {
                ok = isClothLikeName(bones[static_cast<std::size_t>(boneIndex)].name);
            }
            affects = ok;
        }
        outBinding.particleAffectsPose[static_cast<std::size_t>(pid)] = affects ? 1 : 0;
        affectsCount += affects ? 1u : 0u;
    }

    // If writeback candidates are rare, allow a safe fallback:
    // write back bodies connected (through joints) to pinned anchors (mode==0).
    if (cfg.enableWritebackFallback && affectsCount < cfg.minWritebackCandidates) {
        std::size_t fallbackAdded = 0;
        for (std::size_t i = 0; i < rbCount; ++i) {
            const int pid = outBinding.rigidBodyToParticle[i];
            if (pid < 0) {
                continue;
            }
            if (rbSelected[i] == 0 || reachableFromAnchors[i] == 0) {
                continue;
            }
            const int mode = pmx.rigidBodies[i].mode;
            if (mode != 1 && mode != 2) {
                continue;
            }
            const int mappedBone = outBinding.particleToBone[static_cast<std::size_t>(pid)];
            if (mappedBone < 0) {
                continue;
            }
            
            auto& m = outBinding.particleAffectsPose[static_cast<std::size_t>(pid)];
            if (m == 0) {
                m = 1;
                ++fallbackAdded;
            }
        }
        affectsCount += fallbackAdded;
        core::log::info(
            "Physics writeback fallback: added=" + std::to_string(fallbackAdded) +
            " minCandidates=" + std::to_string(cfg.minWritebackCandidates)
        );
    }

    core::log::info(
        "Physics writeback candidates=" + std::to_string(affectsCount) +
        " / particles=" + std::to_string(outWorld.particles().size())
    );
    core::log::info(
        "Physics rigidbody modes: mode0=" + std::to_string(mode0Count) +
        " mode1=" + std::to_string(mode1Count) +
        " mode2=" + std::to_string(mode2Count)
    );
    if (retargetedBones > 0) {
        core::log::info("Physics bone mapping retargeted=" + std::to_string(retargetedBones));
    }

    // Add joint constraints as distance constraints between particles
    for (const auto& j : pmx.joints) {
        if (j.rigidBodyA < 0 || j.rigidBodyB < 0) {
            continue;
        }
        if (j.rigidBodyA >= static_cast<int>(rbCount) ||
            j.rigidBodyB >= static_cast<int>(rbCount)) {
            continue;
        }
        const int pa = outBinding.rigidBodyToParticle[static_cast<std::size_t>(j.rigidBodyA)];
        const int pb = outBinding.rigidBodyToParticle[static_cast<std::size_t>(j.rigidBodyB)];
        if (pa < 0 || pb < 0 || pa == pb) {
            continue;
        }

        const auto& particles = outWorld.particles();
        const float rest = glm::length(particles[static_cast<std::size_t>(pb)].pos - particles[static_cast<std::size_t>(pa)].pos);

        DistanceConstraint c;
        c.a = pa;
        c.b = pb;
        c.restLength = rest;
        c.stiffness = std::clamp(cfg.distanceStiffness, 0.0f, 1.0f);
        outWorld.addDistanceConstraint(c);
    }

    // Pins for kinematic rigid bodies
    for (std::size_t i = 0; i < rbCount; ++i) {
        const auto& rb = pmx.rigidBodies[i];
        if (rb.mode != 0) {
            continue;
        }
        const int pid = outBinding.rigidBodyToParticle[i];
        if (pid < 0) {
            continue;
        }
        glm::vec3 target = outWorld.particles()[static_cast<std::size_t>(pid)].pos;
        if (rb.boneIndex >= 0 && rb.boneIndex < static_cast<int>(pose.global.size())) {
            const glm::mat4 boneG = pose.global[static_cast<std::size_t>(rb.boneIndex)];
            const glm::mat4 world = boneG * rigidLocalTransform(rb);
            target = translationFromMatrix(world);
        }

        PinConstraint pin;
        pin.p = pid;
        pin.target = target;
        pin.stiffness = std::clamp(cfg.pinStiffness, 0.0f, 1.0f);
        outWorld.addPinConstraint(pin);
    }

    return true;
}

void updatePinsFromPose(
    const asset::PmxAsset& pmx,
    const animation::Pose& pose,
    World& world,
    const PmxPhysicsBinding& binding
) {
    if (pmx.rigidBodies.empty() || world.pins().empty()) {
        return;
    }

    // Update pin targets to follow their bone each frame.
    // Pins are created in the same order as rigid bodies iterated; we recompute by scanning.
    auto& pins = world.pins();
    std::size_t pinCursor = 0;
    for (std::size_t rbIndex = 0; rbIndex < pmx.rigidBodies.size(); ++rbIndex) {
        const auto& rb = pmx.rigidBodies[rbIndex];
        if (rb.mode != 0) {
            continue;
        }
        if (pinCursor >= pins.size()) {
            break;
        }

        const int pid = binding.rigidBodyToParticle[rbIndex];
        if (pid < 0) {
            continue;
        }

        glm::vec3 target = world.particles()[static_cast<std::size_t>(pid)].pos;
        if (rb.boneIndex >= 0 && rb.boneIndex < static_cast<int>(pose.global.size())) {
            const glm::mat4 boneG = pose.global[static_cast<std::size_t>(rb.boneIndex)];
            const glm::mat4 worldM = boneG * rigidLocalTransform(rb);
            target = translationFromMatrix(worldM);
        }

        pins[pinCursor].p = pid;
        pins[pinCursor].target = target;
        ++pinCursor;
    }
}

} // namespace physics::pbd

