#include "mmd_diagnostics.hpp"

#include "log.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

namespace app::mmd_diagnostics {
namespace {

template <std::size_t N>
bool containsAny(std::string_view s, const std::string_view (&needles)[N]) {
    for (std::size_t i = 0; i < N; ++i) {
        const auto n = needles[i];
        if (!n.empty() && s.find(n) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

std::vector<std::size_t> computeInfluencedVertexCounts(
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

} // namespace

void logVmdBindingSummary(const animation::Skeleton& skeleton, const animation::AnimationClip& clip) {
    const auto& bones = skeleton.bones();

    std::size_t matchedTracks = 0;
    std::size_t missingTracks = 0;

    core::log::info("---- VMD binding summary ----");
    core::log::info(
        "skeletonBones=" + std::to_string(bones.size()) +
        " clipTracks=" + std::to_string(clip.boneTracks.size()) +
        " clipDuration=" + std::to_string(clip.duration) + "s"
    );

    for (const auto& track : clip.boneTracks) {
        const int idx = skeleton.findBoneIndex(track.boneName);
        if (idx >= 0) {
            ++matchedTracks;
        } else {
            ++missingTracks;
        }
    }

    core::log::info(
        "trackMatch: matched=" + std::to_string(matchedTracks) +
        " missing=" + std::to_string(missingTracks)
    );

    constexpr std::string_view kLegNeedles[] = {
        "足", "ひざ", "膝", "ankle", "knee", "leg", "Leg",
        "左足", "右足", "左ひざ", "右ひざ", "左膝", "右膝",
        "足首", "足ＩＫ", "足IK", "つま先", "toe"
    };

    core::log::info("---- Leg-related bones in skeleton (name/index/bindTranslation) ----");
    for (std::size_t i = 0; i < bones.size(); ++i) {
        const auto& b = bones[i];
        if (!containsAny(b.name, kLegNeedles)) {
            continue;
        }
        core::log::info(
            "bone[" + std::to_string(i) + "] name=" + b.name +
            " parent=" + std::to_string(b.parentIndex) +
            " bindT=(" +
                std::to_string(b.bindTranslation.x) + "," +
                std::to_string(b.bindTranslation.y) + "," +
                std::to_string(b.bindTranslation.z) + ")"
        );
    }

    core::log::info("---- Leg-related tracks in clip (name/keyframes/timeRange) ----");
    for (const auto& track : clip.boneTracks) {
        if (!containsAny(track.boneName, kLegNeedles)) {
            continue;
        }

        float t0 = 0.0f;
        float t1 = 0.0f;
        if (!track.keyframes.empty()) {
            t0 = track.keyframes.front().time;
            t1 = track.keyframes.back().time;
        }

        float maxTransDelta = 0.0f;
        float maxRotAngleDeg = 0.0f;
        if (track.keyframes.size() >= 2) {
            const auto baseT = track.keyframes.front().translation;
            const auto baseR = glm::normalize(track.keyframes.front().rotation);
            for (std::size_t i = 1; i < track.keyframes.size(); ++i) {
                const auto dt = track.keyframes[i].translation - baseT;
                maxTransDelta = std::max(maxTransDelta, glm::length(dt));

                const auto r = glm::normalize(track.keyframes[i].rotation);
                const float d = std::clamp(std::abs(glm::dot(baseR, r)), 0.0f, 1.0f);
                const float angleRad = 2.0f * std::acos(d);
                maxRotAngleDeg = std::max(maxRotAngleDeg, glm::degrees(angleRad));
            }
        }

        const int idx = skeleton.findBoneIndex(track.boneName);
        core::log::info(
            "track name=" + track.boneName +
            " boneIndex=" + std::to_string(idx) +
            " keyframes=" + std::to_string(track.keyframes.size()) +
            " time=[" + std::to_string(t0) + "," + std::to_string(t1) + "]" +
            " max|dT|=" + std::to_string(maxTransDelta) +
            " max|dR|deg=" + std::to_string(maxRotAngleDeg)
        );
    }

    if (missingTracks > 0) {
        core::log::warn("---- Missing tracks (no matching PMX bone) ----");
        constexpr std::size_t kMaxPrint = 80;
        std::size_t printed = 0;
        for (const auto& track : clip.boneTracks) {
            if (skeleton.findBoneIndex(track.boneName) >= 0) {
                continue;
            }
            core::log::warn("missing track boneName=" + track.boneName);
            if (++printed >= kMaxPrint) {
                core::log::warn("... (truncated, more missing tracks exist)");
                break;
            }
        }
    }
    core::log::info("---- VMD binding summary end ----");
}

void logBoneWeightUsageSummary(const asset::PmxAsset& pmx, const animation::Skeleton& skeleton) {
    const auto& bones = skeleton.bones();
    const auto influenceCount = computeInfluencedVertexCounts(pmx, bones.size());

    constexpr std::string_view kLegNeedles[] = {
        "足", "ひざ", "膝", "ankle", "knee", "leg", "Leg",
        "左足", "右足", "左ひざ", "右ひざ", "左膝", "右膝",
        "足首", "足ＩＫ", "足IK", "つま先", "toe", "D", "EX", "補助", "足2"
    };

    core::log::info("---- Bone weight usage (leg-related bones only) ----");
    for (std::size_t i = 0; i < bones.size(); ++i) {
        const auto& b = bones[i];
        if (!containsAny(b.name, kLegNeedles)) {
            continue;
        }
        core::log::info(
            "bone[" + std::to_string(i) + "] name=" + b.name +
            " influencedVertices=" + std::to_string(influenceCount[i])
        );
    }
    core::log::info("---- Bone weight usage end ----");
}

} // namespace app::mmd_diagnostics

