#pragma once

#include "animation_clip.hpp"
#include "animator.hpp"
#include "camera.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "pmx_types.hpp"
#include "pose.hpp"
#include "renderer.hpp"
#include "scene.hpp"
#include "skeleton.hpp"
#include "skinned_mesh.hpp"
#include "timer.hpp"
#include "window.hpp"
#include "free_camera_controller.hpp"
#include "texture.hpp"
#include "vmd_types.hpp"
#include "pbd_world.hpp"
#include "pmx_physics_builder.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

namespace app {

struct MmdModelAsset {
    asset::PmxAsset pmx;
    animation::Skeleton skeleton;
    animation::AnimationClip clip;
    render::SkinnedMesh mesh;
    std::vector<render::SkinnedVertex> baseVertices;
    std::vector<render::SkinnedVertex> morphedVertices;
    asset::VmdClip motionVmd;
    asset::VmdClip faceVmd;

    // Disabled by default to avoid model-specific physics issues.
    // You can toggle at runtime (see Application::update).
    bool physicsEnabled = true;
    bool physicsBuilt = false;
    std::unique_ptr<physics::pbd::World> clothWorld;
    std::unique_ptr<physics::pbd::PmxPhysicsBinding> clothBinding;
};

struct MmdModelInstance {
    const MmdModelAsset* asset = nullptr;
    animation::Animator animator;
    animation::Pose pose;
    std::vector<glm::mat4> skinningMatrices;
    glm::mat4 modelMatrix {1.0f};
};

struct ModelTextureSlot {
    std::string path;
    std::unique_ptr<render::Texture> texture;
};

class Application {
public:
    bool init();
    void run();
    void shutdown();

private:
    bool buildFromMmdFiles(
        const std::string& pmxPath,
        const std::string& motionVmdPath,
        const std::string& faceVmdPath
    );
    void update(float dt);
    void renderFrame();

private:
    std::unique_ptr<platform::Window> window_;
    render::Renderer renderer_;
    scene::Scene scene_;
    scene::Camera camera_;
    core::Timer timer_;

    render::Mesh demoMesh_;
    render::Material demoMaterial_;

    MmdModelAsset modelAsset_;
    MmdModelInstance modelInstance_;
    std::vector<ModelTextureSlot> modelTextures_;
    std::string modelTextureBaseDir_;

    FreeCameraController cameraController_;
};

} // namespace app