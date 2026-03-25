#pragma once

#include "animation_clip.hpp"
#include "pose.hpp"
#include "skeleton.hpp"

namespace animation {

class Animator {
public:
    void setSkeleton(const Skeleton* skeleton);
    void setClip(const AnimationClip* clip);
    void setLoop(bool loop);

    void update(float dt);
    void sample(Pose& outPose) const;

    float currentTime() const;

private:
    const Skeleton* skeleton_ = nullptr;
    const AnimationClip* clip_ = nullptr;
    float currentTime_ = 0.0f;
    bool loop_ = true;
};

} // namespace animation