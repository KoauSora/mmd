#pragma once

#include "pose.hpp"
#include "skeleton.hpp"

namespace animation::ik {

// Minimal CCD IK solver for PMX-style IK constraints.
// Applies constraints in-place to outPose.local / outPose.global.
void solve(const Skeleton& skeleton, Pose& inOutPose);

} // namespace animation::ik

