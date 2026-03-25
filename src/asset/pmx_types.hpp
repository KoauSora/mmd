#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>
#include <cstdint>


namespace asset {

// PMX 2.1 soft body (used by Bullet in MMD). Many models omit this section.
struct PmxSoftBodyConfig {
    float vcf = 0.0f;
    float dp = 0.0f;
    float dg = 0.0f;
    float lf = 0.0f;
    float pr = 0.0f;
    float vc = 0.0f;
    float df = 0.0f;
    float mt = 0.0f;
    float chr = 0.0f;
    float khr = 0.0f;
    float shr = 0.0f;
    float ahr = 0.0f;
};

struct PmxSoftBodyClusterConfig {
    float srhrCl = 0.0f;
    float skhrCl = 0.0f;
    float sshrCl = 0.0f;
    float srSplitCl = 0.0f;
    float skSplitCl = 0.0f;
    float ssSplitCl = 0.0f;
};

struct PmxSoftBodyIterationConfig {
    int vIt = 0;
    int pIt = 0;
    int dIt = 0;
    int cIt = 0;
};

struct PmxSoftBodyMaterialConfig {
    float lst = 0.0f;
    float ast = 0.0f;
    float vst = 0.0f;
};

struct PmxSoftBodyAnchor {
    int rigidBodyIndex = -1;
    int vertexIndex = -1;
    int nearMode = 0; // 0/1 in spec (treat as int for simplicity)
};

struct PmxSoftBody {
    std::string name;
    std::string universalName;

    int shape = 0;
    int materialIndex = -1;
    int group = 0;
    std::uint16_t collisionMask = 0;
    int flags = 0;

    int bLinkDistance = 0;
    int clusterCount = 0;
    float totalMass = 0.0f;
    float collisionMargin = 0.0f;
    int aeroModel = 0;

    PmxSoftBodyConfig config;
    PmxSoftBodyClusterConfig cluster;
    PmxSoftBodyIterationConfig iteration;
    PmxSoftBodyMaterialConfig material;

    std::vector<PmxSoftBodyAnchor> anchors;
    std::vector<int> pinVertexIndices;
};

struct PmxIkLink {
    int boneIndex = -1;
    bool hasLimit = false;
    glm::vec3 limitMin {0.0f};
    glm::vec3 limitMax {0.0f};
};

struct PmxIkInfo {
    int targetBoneIndex = -1;
    int loopCount = 0;
    float limitAngle = 0.0f;
    std::vector<PmxIkLink> links;
};

struct PmxVertex {
    glm::vec3 position {0.0f};
    glm::vec3 normal {0.0f, 1.0f, 0.0f};
    glm::vec2 uv {0.0f};
    glm::ivec4 boneIds {0, 0, 0, 0};
    glm::vec4 boneWeights {1.0f, 0.0f, 0.0f, 0.0f};
};

struct PmxMaterial {
    std::string name;
    int indexOffset = 0;
    int indexCount = 0;
    std::string diffuseTexturePath;
    std::string toonTexturePath;
    bool twoSided = false;
};

struct PmxVertexMorphOffset {
    int vertexIndex = -1;
    glm::vec3 offset {0.0f};
};

struct PmxMorph {
    std::string name;
    int panel = 0;
    int type = 0;
    std::vector<PmxVertexMorphOffset> vertexOffsets; // only for type=1 (vertex morph)
};

struct PmxRigidBody {
    std::string name;
    int boneIndex = -1;
    int group = 0;
    std::uint16_t collisionMask = 0;

    int shapeType = 0; // 0:sphere 1:box 2:capsule
    glm::vec3 shapeSize {0.0f}; // sphere: (r,0,0), box: (x,y,z), capsule: (r,h,0)
    glm::vec3 shapePosition {0.0f};
    glm::vec3 shapeRotation {0.0f}; // radians

    float mass = 0.0f;
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    float restitution = 0.0f;
    float friction = 0.0f;

    int mode = 0; // 0: follow bone (kinematic) 1: physics 2: physics+bone
};

struct PmxJoint {
    std::string name;
    int type = 0; // usually 0 (6DOF spring)
    int rigidBodyA = -1;
    int rigidBodyB = -1;

    glm::vec3 position {0.0f};
    glm::vec3 rotation {0.0f}; // radians

    glm::vec3 limitPosMin {0.0f};
    glm::vec3 limitPosMax {0.0f};
    glm::vec3 limitRotMin {0.0f};
    glm::vec3 limitRotMax {0.0f};

    glm::vec3 springPos {0.0f};
    glm::vec3 springRot {0.0f};
};

struct PmxBone {
    std::string name;
    int parentIndex = -1;
    glm::vec3 position {0.0f};
    // Append/Inheritance (PMX): bone can inherit rotation and/or translation from another bone.
    bool inheritRotation = false;
    bool inheritTranslation = false;
    int inheritParentIndex = -1;
    float inheritRate = 0.0f;
    bool hasIk = false;
    PmxIkInfo ik;
};

struct PmxAsset {
    std::vector<PmxVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<PmxMaterial> materials;
    std::vector<PmxBone> bones;
    std::vector<PmxMorph> morphs;
    std::vector<PmxRigidBody> rigidBodies;
    std::vector<PmxJoint> joints;
    std::vector<PmxSoftBody> softBodies;
};

} // namespace asset