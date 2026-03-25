#include "skinning.hpp"

namespace animation::skinning {

void buildSkinningMatrices(
    const Skeleton& skeleton,
    const Pose& pose,
    std::vector<glm::mat4>& outMatrices
) {
    const auto& bones = skeleton.bones();
    outMatrices.resize(bones.size(), glm::mat4(1.0f));

    for (std::size_t i = 0; i < bones.size(); ++i) {
        if (i < pose.global.size()) {
            outMatrices[i] = pose.global[i] * bones[i].inverseBind;
        } else {
            outMatrices[i] = glm::mat4(1.0f);
        }
    }
}

} // namespace animation::skinning