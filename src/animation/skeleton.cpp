#include "skeleton.hpp"

#include "log.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <functional>
#include <string>
#include <vector>

namespace animation {
namespace {

static glm::mat4 composeTransform(
    const glm::vec3& translation,
    const glm::quat& rotation,
    const glm::vec3& scale
) {
    const glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
    const glm::mat4 r = glm::mat4_cast(rotation);
    const glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
    return t * r * s;
}

} // namespace

bool Skeleton::buildFromPmx(const asset::PmxAsset& pmx) {
    bones_.clear();
    nameToIndex_.clear();
    ikConstraints_.clear();
    children_.clear();

    bones_.reserve(pmx.bones.size());

    for (std::size_t i = 0; i < pmx.bones.size(); ++i) {
        const auto& src = pmx.bones[i];

        SkeletonBone bone;
        bone.name = src.name;
        bone.parentIndex = src.parentIndex;
        bone.bindRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        bone.bindScale = glm::vec3(1.0f);
        bone.inheritRotation = src.inheritRotation;
        bone.inheritTranslation = src.inheritTranslation;
        bone.inheritParentIndex = src.inheritParentIndex;
        bone.inheritRate = src.inheritRate;

        if (src.parentIndex >= 0 &&
            src.parentIndex < static_cast<int>(pmx.bones.size())) {
            bone.bindTranslation = src.position - pmx.bones[static_cast<std::size_t>(src.parentIndex)].position;
        } else {
            bone.bindTranslation = src.position;
            bone.parentIndex = -1;
        }

        bones_.push_back(bone);
        nameToIndex_[bone.name] = static_cast<int>(i);
    }

    // Build children adjacency for fast subtree updates.
    children_.assign(bones_.size(), {});
    for (std::size_t i = 0; i < bones_.size(); ++i) {
        const int p = bones_[i].parentIndex;
        if (p >= 0 && p < static_cast<int>(bones_.size())) {
            children_[static_cast<std::size_t>(p)].push_back(static_cast<int>(i));
        }
    }

    // Build IK constraints (minimal: store chain, solving done elsewhere)
    ikConstraints_.reserve(pmx.bones.size() / 16);
    for (std::size_t i = 0; i < pmx.bones.size(); ++i) {
        const auto& src = pmx.bones[i];
        if (!src.hasIk) {
            continue;
        }

        IkConstraint c;
        c.ikBoneIndex = static_cast<int>(i);
        c.targetBoneIndex = src.ik.targetBoneIndex;
        c.loopCount = src.ik.loopCount;
        c.limitAngle = src.ik.limitAngle;
        c.links.reserve(src.ik.links.size());
        c.linkLimits.reserve(src.ik.links.size());
        for (const auto& lk : src.ik.links) {
            c.links.push_back(lk.boneIndex);
            IkLinkLimit lim;
            lim.hasLimit = lk.hasLimit;
            lim.limitMin = lk.limitMin;
            lim.limitMax = lk.limitMax;
            c.linkLimits.push_back(lim);
        }

        // Basic validation
        const int boneCount = static_cast<int>(pmx.bones.size());
        if (c.targetBoneIndex < 0 || c.targetBoneIndex >= boneCount) {
            continue;
        }
        bool ok = true;
        for (const int linkIndex : c.links) {
            if (linkIndex < 0 || linkIndex >= boneCount) {
                ok = false;
                break;
            }
        }
        if (!ok || c.links.empty()) {
            continue;
        }
        ikConstraints_.push_back(std::move(c));
    }

    std::vector<glm::mat4> globalBind(bones_.size(), glm::mat4(1.0f));
    std::vector<bool> computed(bones_.size(), false);

    std::function<glm::mat4(int)> buildGlobal = [&](int index) -> glm::mat4 {
        if (index < 0 || index >= static_cast<int>(bones_.size())) {
            return glm::mat4(1.0f);
        }

        if (computed[static_cast<std::size_t>(index)]) {
            return globalBind[static_cast<std::size_t>(index)];
        }

        const auto& bone = bones_[static_cast<std::size_t>(index)];
        const glm::mat4 local =
            composeTransform(bone.bindTranslation, bone.bindRotation, bone.bindScale);

        glm::mat4 global = local;
        if (bone.parentIndex >= 0) {
            global = buildGlobal(bone.parentIndex) * local;
        }

        globalBind[static_cast<std::size_t>(index)] = global;
        computed[static_cast<std::size_t>(index)] = true;
        return global;
    };

    for (int i = 0; i < static_cast<int>(bones_.size()); ++i) {
        const glm::mat4 global = buildGlobal(i);
        bones_[static_cast<std::size_t>(i)].inverseBind = glm::inverse(global);
    }

    core::log::info("Skeleton built from PMX asset.");
    return true;
}

std::size_t Skeleton::size() const {
    return bones_.size();
}

int Skeleton::findBoneIndex(const std::string& boneName) const {
    const auto it = nameToIndex_.find(boneName);
    if (it == nameToIndex_.end()) {
        return -1;
    }
    return it->second;
}

const std::vector<SkeletonBone>& Skeleton::bones() const {
    return bones_;
}

const std::vector<IkConstraint>& Skeleton::ikConstraints() const {
    return ikConstraints_;
}

const std::vector<std::vector<int>>& Skeleton::children() const {
    return children_;
}

} // namespace animation