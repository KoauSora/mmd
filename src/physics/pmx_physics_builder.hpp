#pragma once

#include "pbd_world.hpp"

#include "pmx_types.hpp"
#include "pose.hpp"
#include "skeleton.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace physics::pbd {

struct PmxPhysicsBinding {
    // rigidBodyIndex -> particle index
    std::vector<int> rigidBodyToParticle;
    // particle index -> bone index (or -1)
    std::vector<int> particleToBone;
    // Bind reference positions for stable pose writeback.
    // particleBindPos: particle world position at build time.
    std::vector<glm::vec3> particleBindPos;
    // particle index -> whether we should write back to pose
    std::vector<std::uint8_t> particleAffectsPose;
};

struct BuilderConfig {
    float distanceStiffness = 0.9f;
    float pinStiffness = 1.0f;
    float defaultInvMass = 1.0f;
    bool filterClothLike = true;
    // For "secondary motion" rigs, many cloth/hair bodies are mode==1 (physics only).
    // Allow them to write back when selected, otherwise motion can be invisible.
    bool allowWritebackMode1 = true;
    // Some models provide rigid bodies but no joints; still build pins+gravity.
    bool allowNoJoints = true;
    // If PMX has very few mode==2 bodies (bone-follow), enable a safe fallback:
    // allow mode==1/2 bodies connected to pinned anchors (mode==0) to write back.
    bool enableWritebackFallback = true;
    std::size_t minWritebackCandidates = 8;
};

// Build a PBD world from PMX rigid bodies/joints.
// The initial particle positions come from the current pose global transforms.
bool buildFromPmxPhysics(
    const asset::PmxAsset& pmx,
    const animation::Skeleton& skeleton,
    const animation::Pose& pose,
    World& outWorld,
    PmxPhysicsBinding& outBinding,
    const BuilderConfig& cfg = {}
);

// Update pin targets each frame from current pose.
void updatePinsFromPose(
    const asset::PmxAsset& pmx,
    const animation::Pose& pose,
    World& world,
    const PmxPhysicsBinding& binding
);

} // namespace physics::pbd

