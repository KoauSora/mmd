#pragma once

#include "animation_clip.hpp"
#include "pmx_types.hpp"
#include "skeleton.hpp"

namespace app::mmd_diagnostics {

void logVmdBindingSummary(const animation::Skeleton& skeleton, const animation::AnimationClip& clip);
void logBoneWeightUsageSummary(const asset::PmxAsset& pmx, const animation::Skeleton& skeleton);

} // namespace app::mmd_diagnostics

