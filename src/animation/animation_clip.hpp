#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace animation {

struct BoneKeyframe {
    float time = 0.0f;
    glm::vec3 translation {0.0f};
    glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
};

struct BoneTrack {
    std::string boneName;
    std::vector<BoneKeyframe> keyframes;
};

struct AnimationClip {
    float duration = 0.0f;
    std::vector<BoneTrack> boneTracks;
};

} // namespace animation