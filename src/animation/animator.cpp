#include "animator.hpp"

#include "ik_solver.hpp"
#include "log.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <chrono>

namespace animation {

static glm::mat4 poseToMatrix(const JointPose& pose) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, pose.translation);
    m *= glm::mat4_cast(pose.rotation);
    m = glm::scale(m, pose.scale);
    return m;
}

static JointPose sampleTrackAtTime(const BoneTrack& track, float time) {
    JointPose result {};

    if (track.keyframes.empty()) {
        return result;
    }

    if (time <= track.keyframes.front().time) {
        result.translation = track.keyframes.front().translation;
        result.rotation = track.keyframes.front().rotation;
        return result;
    }

    if (time >= track.keyframes.back().time) {
        result.translation = track.keyframes.back().translation;
        result.rotation = track.keyframes.back().rotation;
        return result;
    }

    for (std::size_t i = 0; i + 1 < track.keyframes.size(); ++i) {
        const auto& a = track.keyframes[i];
        const auto& b = track.keyframes[i + 1];

        if (time >= a.time && time <= b.time) {
            const float span = b.time - a.time;
            const float t = span > 0.0f ? (time - a.time) / span : 0.0f;

            result.translation = glm::mix(a.translation, b.translation, t);
            result.rotation = glm::normalize(glm::slerp(a.rotation, b.rotation, t));
            return result;
        }
    }

    return result;
}

void Animator::setSkeleton(const Skeleton* skeleton) {
    skeleton_ = skeleton;
}

void Animator::setClip(const AnimationClip* clip) {
    clip_ = clip;
    currentTime_ = 0.0f;
}

void Animator::setLoop(bool loop) {
    loop_ = loop;
}

void Animator::update(float dt) {
    if (!clip_ || clip_->duration <= 0.0f) {
        return;
    }

    currentTime_ += dt;

    if (loop_) {
        while (currentTime_ > clip_->duration) {
            currentTime_ -= clip_->duration;
        }
    } else if (currentTime_ > clip_->duration) {
        currentTime_ = clip_->duration;
    }
}

void Animator::sample(Pose& outPose) const {
    if (!skeleton_) {
        outPose.resize(0);
        return;
    }

    const auto& bones = skeleton_->bones();
    outPose.resize(bones.size());

    for (std::size_t i = 0; i < bones.size(); ++i) {
        outPose.local[i].translation = bones[i].bindTranslation;
        outPose.local[i].rotation = bones[i].bindRotation;
        outPose.local[i].scale = bones[i].bindScale;
    }

    if (clip_) {
        for (const auto& track : clip_->boneTracks) {
            const int boneIndex = skeleton_->findBoneIndex(track.boneName);
            if (boneIndex < 0) {
                continue;
            }

            JointPose sampled = sampleTrackAtTime(track, currentTime_);
            auto& dst = outPose.local[static_cast<std::size_t>(boneIndex)];

            dst.translation += sampled.translation;
            dst.rotation = glm::normalize(dst.rotation * sampled.rotation);
        }
    }

    // Apply PMX append/inherit (rotation/translation) in local space.
    // Many modern rigs rely on this (e.g. control bones driving deform bones like "*D"/"*補助").
    {
        const std::size_t n = bones.size();
        std::vector<glm::vec3> deltaT(n, glm::vec3(0.0f));
        std::vector<glm::quat> deltaR(n, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        for (std::size_t i = 0; i < n; ++i) {
            deltaT[i] = outPose.local[i].translation - bones[i].bindTranslation;
            deltaR[i] = glm::normalize(glm::inverse(bones[i].bindRotation) * outPose.local[i].rotation);
        }

        for (std::size_t i = 0; i < n; ++i) {
            const auto& b = bones[i];
            if (!b.inheritRotation && !b.inheritTranslation) {
                continue;
            }
            const int src = b.inheritParentIndex;
            if (src < 0 || src >= static_cast<int>(n)) {
                continue;
            }
            const float w = std::clamp(b.inheritRate, -1.0f, 1.0f);
            if (b.inheritTranslation) {
                outPose.local[i].translation += deltaT[static_cast<std::size_t>(src)] * w;
            }
            if (b.inheritRotation) {
                const glm::quat add = glm::normalize(glm::slerp(
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                    deltaR[static_cast<std::size_t>(src)],
                    w
                ));
                outPose.local[i].rotation = glm::normalize(outPose.local[i].rotation * add);
            }
        }
    }

    for (std::size_t i = 0; i < bones.size(); ++i) {
        const glm::mat4 local = poseToMatrix(outPose.local[i]);
        const int parentIndex = bones[i].parentIndex;

        if (parentIndex >= 0) {
            outPose.global[i] = outPose.global[static_cast<std::size_t>(parentIndex)] * local;
        } else {
            outPose.global[i] = local;
        }
    }

    // Apply IK after FK + PMX inherit.
    if (skeleton_ != nullptr && !skeleton_->ikConstraints().empty()) {
        using clock = std::chrono::steady_clock;
        static clock::time_point windowStart = clock::now();
        static std::uint64_t calls = 0;
        static double sumMs = 0.0;
        static double maxMs = 0.0;

        const auto t0 = clock::now();
        animation::ik::solve(*skeleton_, outPose);
        const auto t1 = clock::now();

        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        ++calls;
        sumMs += ms;
        maxMs = std::max(maxMs, ms);

        const auto now = clock::now();
        if (std::chrono::duration<double>(now - windowStart).count() >= 1.0) {
            const double avg = calls > 0 ? (sumMs / static_cast<double>(calls)) : 0.0;
            core::log::info(
                "IK solve perf: calls=" + std::to_string(calls) +
                " avgMs=" + std::to_string(avg) +
                " maxMs=" + std::to_string(maxMs)
            );
            windowStart = now;
            calls = 0;
            sumMs = 0.0;
            maxMs = 0.0;
        }
    }
}

float Animator::currentTime() const {
    return currentTime_;
}

} // namespace animation