#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace asset {

struct VmdBoneKeyframe {
    float time = 0.0f;
    glm::vec3 translation {0.0f};
    glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
};

struct VmdBoneTrack {
    std::string boneName;
    std::vector<VmdBoneKeyframe> keyframes;
};

struct VmdMorphKeyframe {
    float time = 0.0f;
    float weight = 0.0f;
};

struct VmdMorphTrack {
    std::string morphName;
    std::vector<VmdMorphKeyframe> keyframes;
};

struct VmdClip {
    float duration = 0.0f;
    std::vector<VmdBoneTrack> boneTracks;
    std::unordered_map<std::string, std::size_t> trackLookup;

    std::vector<VmdMorphTrack> morphTracks;
    std::unordered_map<std::string, std::size_t> morphLookup;
};

} // namespace asset