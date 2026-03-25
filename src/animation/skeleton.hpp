#pragma once

#include "pmx_types.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace animation {

struct IkLinkLimit {
    bool hasLimit = false;
    glm::vec3 limitMin {0.0f};
    glm::vec3 limitMax {0.0f};
};

struct IkConstraint {
    int ikBoneIndex = -1;      // the IK bone (goal)
    int targetBoneIndex = -1;  // end effector bone
    int loopCount = 0;
    float limitAngle = 0.0f;   // max angle per iteration (radians, PMX convention)
    std::vector<int> links;    // bones to rotate, in order
    std::vector<IkLinkLimit> linkLimits; // same size as links
};

struct SkeletonBone {
    std::string name;
    int parentIndex = -1;

    glm::vec3 bindTranslation {0.0f};
    glm::quat bindRotation {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 bindScale {1.0f, 1.0f, 1.0f};

    // PMX append/inherit (a.k.a. "追加/付与")
    bool inheritRotation = false;
    bool inheritTranslation = false;
    int inheritParentIndex = -1;
    float inheritRate = 0.0f;

    glm::mat4 inverseBind {1.0f};
};

class Skeleton {
public:
    bool buildFromPmx(const asset::PmxAsset& pmx);

    std::size_t size() const;
    int findBoneIndex(const std::string& boneName) const;

    const std::vector<SkeletonBone>& bones() const;
    const std::vector<IkConstraint>& ikConstraints() const;
    const std::vector<std::vector<int>>& children() const;

private:
    std::vector<SkeletonBone> bones_;
    std::unordered_map<std::string, int> nameToIndex_;
    std::vector<IkConstraint> ikConstraints_;
    std::vector<std::vector<int>> children_;
};

} // namespace animation