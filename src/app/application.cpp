#include "application.hpp"

#include "input.hpp"
#include "log.hpp"
#include "skinning.hpp"
#include "texture.hpp"
#include "free_camera_controller.hpp"
#include "pmx_loader.hpp"
#include "vmd_loader.hpp"
#include "vmd_types.hpp"
#include "mmd_diagnostics.hpp"
#include "pose_physics.hpp"

#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {
std::string pmxPath = "/home/sora/project/mmd_player/assets/models/nangong/南宫羽.pmx";
std::string motionVmdPath = "/home/sora/project/mmd_player/assets/motions/act2/action2/胡桃.vmd";
std::string faceVmdPath = "/home/sora/project/mmd_player/assets/motions/act2/action2/表情.vmd";

render::Texture sWhiteTexture;
render::Texture sModelTexture;

std::string normalizePmxTexturePath(std::string relPath) {
    for (char& c : relPath) {
        if (c == '\\') {
            c = '/';
        }
    }
    return relPath;
}

void buildBindPoseFromSkeleton(
    const animation::Skeleton& skeleton,
    animation::Pose& outPose
) {
    const auto& bones = skeleton.bones();
    outPose.resize(bones.size());

    for (std::size_t i = 0; i < bones.size(); ++i) {
        outPose.local[i].translation = bones[i].bindTranslation;
        outPose.local[i].rotation = bones[i].bindRotation;
        outPose.local[i].scale = bones[i].bindScale;
    }

    for (std::size_t i = 0; i < bones.size(); ++i) {
        const auto& bone = bones[i];

        const glm::mat4 local =
            glm::translate(glm::mat4(1.0f), outPose.local[i].translation) *
            glm::mat4_cast(outPose.local[i].rotation) *
            glm::scale(glm::mat4(1.0f), outPose.local[i].scale);

        if (bone.parentIndex >= 0) {
            outPose.global[i] = outPose.global[static_cast<std::size_t>(bone.parentIndex)] * local;
        } else {
            outPose.global[i] = local;
        }
    }
}

animation::AnimationClip toAnimationClip(const asset::VmdClip& vmd) {
    animation::AnimationClip clip;
    clip.duration = vmd.duration;
    clip.boneTracks.reserve(vmd.boneTracks.size());

    for (const auto& srcTrack : vmd.boneTracks) {
        animation::BoneTrack dst;
        dst.boneName = srcTrack.boneName;
        dst.keyframes.reserve(srcTrack.keyframes.size());
        for (const auto& kf : srcTrack.keyframes) {
            animation::BoneKeyframe out;
            out.time = kf.time;
            out.translation = kf.translation;
            out.rotation = kf.rotation;
            dst.keyframes.push_back(out);
        }
        if (!dst.keyframes.empty()) {
            clip.boneTracks.push_back(std::move(dst));
        }
    }

    return clip;
}

static float sampleMorphWeightAtTime(const asset::VmdMorphTrack& track, float time) {
    if (track.keyframes.empty()) {
        return 0.0f;
    }
    if (time <= track.keyframes.front().time) {
        return track.keyframes.front().weight;
    }
    if (time >= track.keyframes.back().time) {
        return track.keyframes.back().weight;
    }
    for (std::size_t i = 0; i + 1 < track.keyframes.size(); ++i) {
        const auto& a = track.keyframes[i];
        const auto& b = track.keyframes[i + 1];
        if (time >= a.time && time <= b.time) {
            const float span = b.time - a.time;
            const float t = span > 0.0f ? (time - a.time) / span : 0.0f;
            return glm::mix(a.weight, b.weight, t);
        }
    }
    return 0.0f;
}

static void applyVertexMorphsToMesh(
    app::MmdModelAsset& asset,
    float time
) {
    if (asset.faceVmd.morphTracks.empty() || asset.pmx.morphs.empty() || asset.baseVertices.empty()) {
        return;
    }
    if (asset.morphedVertices.size() != asset.baseVertices.size()) {
        asset.morphedVertices = asset.baseVertices;
    } else {
        asset.morphedVertices = asset.baseVertices;
    }

    // Build a transient morph name lookup (linear search is OK for now).
    auto findPmxMorph = [&](const std::string& name) -> const asset::PmxMorph* {
        for (const auto& m : asset.pmx.morphs) {
            if (m.name == name) {
                return &m;
            }
        }
        return nullptr;
    };

    for (const auto& track : asset.faceVmd.morphTracks) {
        const float w = sampleMorphWeightAtTime(track, time);
        if (std::abs(w) < 1e-6f) {
            continue;
        }
        const auto* morph = findPmxMorph(track.morphName);
        if (!morph) {
            continue;
        }
        // Facial VMDs should only drive eyebrow/eye/mouth panels.
        // Prevent accidental non-facial vertex morphs from stretching clothing/hair meshes.
        if (morph->panel < 1 || morph->panel > 3) {
            continue;
        }
        if (morph->type != 1) {
            continue;
        }
        for (const auto& off : morph->vertexOffsets) {
            if (off.vertexIndex < 0 || off.vertexIndex >= static_cast<int>(asset.morphedVertices.size())) {
                continue;
            }
            auto& v = asset.morphedVertices[static_cast<std::size_t>(off.vertexIndex)];
            v.position += off.offset * w;
        }
    }

    asset.mesh.updateVertices(asset.morphedVertices);
}

// Diagnostics moved to app::mmd_diagnostics.

} // namespace

namespace app {

bool Application::init() {
    core::log::info("Application init...");

    window_ = std::make_unique<platform::Window>();
    if (!window_->create(1280, 720, "MMD Player")) {
        core::log::error("Failed to create window.");
        window_.reset();
        return false;
    }

    if (!renderer_.init(window_->nativeHandle())) {
        core::log::error("Failed to initialize renderer.");
        window_->destroy();
        window_.reset();
        return false;
    }

    const float aspect =
        (window_->height() != 0)
            ? static_cast<float>(window_->width()) / static_cast<float>(window_->height())
            : (16.0f / 9.0f);

    FreeCameraConfig cameraConfig;
    cameraConfig.position = {0.0f, 1.0f, 4.0f};
    cameraConfig.target = {0.0f, 0.8f, 0.0f};
    cameraConfig.up = {0.0f, 1.0f, 0.0f};
    cameraConfig.fovDegrees = 60.0f;
    cameraConfig.nearPlane = 0.1f;
    cameraConfig.farPlane = 100.0f;
    cameraConfig.moveSpeed = 3.5f;
    cameraConfig.fastMultiplier = 3.0f;
    cameraConfig.mouseSensitivity = 0.08f;
    cameraConfig.panSensitivity = 0.02f;

    cameraController_.initialize(camera_, aspect, cameraConfig);

    if (!sWhiteTexture.createWhite()) {
        core::log::warn("Failed to create fallback white texture. Continue without it.");
    }

    demoMaterial_.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    demoMaterial_.diffuse = sWhiteTexture.valid() ? &sWhiteTexture : nullptr;
    demoMaterial_.twoSided = false;

    if (!buildFromMmdFiles(
            pmxPath,
            motionVmdPath,
            faceVmdPath)) {
        core::log::error("Failed to build demo scene.");
        shutdown();
        return false;
    }



    timer_.reset();
    core::log::info("Application init finished.");
    return true;
}

void Application::run() {
    if (!window_) {
        core::log::error("Application::run called before successful init.");
        return;
    }
    core::log::info("Entering main loop...");
    core::log::info(window_->shouldClose() ? "window already wants to close" : "window is alive");
    timer_.reset();

    while (!window_->shouldClose()) {
        window_->pollEvents();

        float dt = timer_.tick();
        if (dt < 0.0f) {
            dt = 0.0f;
        }
        if (dt > 0.1f) {
            dt = 0.1f;
        }

        update(dt);
        renderFrame();

        window_->swapBuffers();
    }
}

void Application::shutdown() {
    core::log::info("Application shutdown...");

    demoMaterial_.diffuse = nullptr;

    demoMesh_.destroy();
    modelAsset_.mesh.destroy();

    if (sModelTexture.valid()) {
        sModelTexture.destroy();
    }
    for (auto& slot : modelTextures_) {
        if (slot.texture) {
            slot.texture->destroy();
        }
    }
    modelTextures_.clear();

    if (sWhiteTexture.valid()) {
        sWhiteTexture.destroy();
    }

    renderer_.shutdown();

    if (window_) {
        window_->destroy();
        window_.reset();
    }

    core::log::info("Application shutdown finished.");
}




bool Application::buildFromMmdFiles(
    const std::string& pmxPath,
    const std::string& motionVmdPath,
    const std::string& faceVmdPath
) {
    // -------------------------------------------------------------------------
    // 1) 地面
    // -------------------------------------------------------------------------
    {
        std::vector<render::Vertex> vertices {
            {{-1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{ 1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{-1.0f, 0.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
            {{ 1.0f, 0.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        };

        std::vector<std::uint32_t> indices {
            0, 2, 1,
            2, 3, 1,
        };

        if (!demoMesh_.create(vertices, indices)) {
            core::log::error("Failed to create demo ground mesh.");
            return false;
        }
    }

    // -------------------------------------------------------------------------
    // 2) 真实 PMX
    // -------------------------------------------------------------------------
    {
        // 改成你的 pmx 路径

        if (!asset::PmxLoader::loadFromFile(pmxPath, modelAsset_.pmx)) {
            core::log::error("Failed to load PMX.");
            return false;
        }

        if (!modelAsset_.skeleton.buildFromPmx(modelAsset_.pmx)) {
            core::log::error("Failed to build skeleton from PMX.");
            return false;
        }
        app::mmd_diagnostics::logBoneWeightUsageSummary(modelAsset_.pmx, modelAsset_.skeleton);

        // Safety: clamp any out-of-range bone indices in vertex weights.
        // Some assets contain weights referencing missing bones; clamping avoids extreme stretching.
        {
            const int boneCount = static_cast<int>(modelAsset_.skeleton.bones().size());
            std::size_t fixedVertices = 0;
            std::size_t fixedSlots = 0;
            for (auto& v : modelAsset_.pmx.vertices) {
                int ids[4] = {v.boneIds.x, v.boneIds.y, v.boneIds.z, v.boneIds.w};
                float ws[4] = {v.boneWeights.x, v.boneWeights.y, v.boneWeights.z, v.boneWeights.w};
                bool changed = false;
                for (int k = 0; k < 4; ++k) {
                    if (ids[k] < 0 || ids[k] >= boneCount) {
                        ids[k] = 0;
                        ws[k] = 0.0f;
                        changed = true;
                        ++fixedSlots;
                    }
                }
                if (changed) {
                    const float sum = ws[0] + ws[1] + ws[2] + ws[3];
                    if (sum > 1e-8f) {
                        for (int k = 0; k < 4; ++k) {
                            ws[k] /= sum;
                        }
                    } else {
                        ids[0] = 0;
                        ws[0] = 1.0f;
                        ids[1] = ids[2] = ids[3] = 0;
                        ws[1] = ws[2] = ws[3] = 0.0f;
                    }
                    v.boneIds = {ids[0], ids[1], ids[2], ids[3]};
                    v.boneWeights = {ws[0], ws[1], ws[2], ws[3]};
                    ++fixedVertices;
                }
            }
            if (fixedVertices > 0) {
                core::log::warn(
                    "Clamped out-of-range bone weights: vertices=" + std::to_string(fixedVertices) +
                    " slots=" + std::to_string(fixedSlots) +
                    " boneCount=" + std::to_string(boneCount)
                );
            }
        }

        std::vector<render::SkinnedVertex> gpuVertices;
        gpuVertices.reserve(modelAsset_.pmx.vertices.size());

        for (const auto& v : modelAsset_.pmx.vertices) {
            render::SkinnedVertex sv;
            sv.position = v.position;
            sv.normal = v.normal;
            sv.uv = v.uv;
            sv.boneIds = v.boneIds;
            sv.boneWeights = v.boneWeights;
            gpuVertices.push_back(sv);
        }

        if (!modelAsset_.mesh.create(gpuVertices, modelAsset_.pmx.indices)) {
            core::log::error("Failed to create skinned mesh from PMX.");
            return false;
        }

        modelAsset_.baseVertices = gpuVertices;
        modelAsset_.morphedVertices.clear();

        modelAsset_.clip = {};
        modelAsset_.motionVmd = {};
        modelAsset_.faceVmd = {};

        if (!motionVmdPath.empty()) {
            asset::VmdClip vmd;
            if (asset::VmdLoader::loadFromFile(motionVmdPath, vmd)) {
                modelAsset_.motionVmd = vmd;
                modelAsset_.clip = toAnimationClip(vmd);
                app::mmd_diagnostics::logVmdBindingSummary(modelAsset_.skeleton, modelAsset_.clip);
            } else {
                core::log::warn("Failed to load motion VMD. Continue with bind pose: " + motionVmdPath);
            }
        }

        // If face VMD is not provided, fall back to motion VMD morph tracks.
        if (!faceVmdPath.empty()) {
            asset::VmdClip face;
            if (asset::VmdLoader::loadFromFile(faceVmdPath, face)) {
                modelAsset_.faceVmd = face;
            } else {
                core::log::warn("Failed to load face VMD: " + faceVmdPath);
                modelAsset_.faceVmd = modelAsset_.motionVmd;
            }
        } else {
            modelAsset_.faceVmd = modelAsset_.motionVmd;
        }

        modelInstance_.asset = &modelAsset_;
        modelInstance_.animator.setSkeleton(&modelAsset_.skeleton);
        modelInstance_.animator.setClip(&modelAsset_.clip);
        modelInstance_.animator.setLoop(true);

        if (modelAsset_.clip.duration > 0.0f && !modelAsset_.clip.boneTracks.empty()) {
            modelInstance_.animator.sample(modelInstance_.pose);
        } else {
            buildBindPoseFromSkeleton(modelAsset_.skeleton, modelInstance_.pose);
        }

        animation::skinning::buildSkinningMatrices(
            modelAsset_.skeleton,
            modelInstance_.pose,
            modelInstance_.skinningMatrices
        );

        // Build cloth-like physics from PMX rigid bodies/joints (optional).
        modelAsset_.clothWorld = std::make_unique<physics::pbd::World>();
        modelAsset_.clothBinding = std::make_unique<physics::pbd::PmxPhysicsBinding>();
        modelAsset_.physicsBuilt = false;
        if (modelAsset_.physicsEnabled) {
            physics::pbd::BuilderConfig pcfg;
            pcfg.distanceStiffness = 0.85f;
            pcfg.pinStiffness = 1.0f;
            pcfg.defaultInvMass = 1.0f;
            pcfg.filterClothLike = true;
            
            // 先只允许 PMX 明确要求“物理回写骨骼”的刚体写回。
            // 你现在 mode2 数量已经够多，不需要放开 mode1/fallback。
            pcfg.allowWritebackMode1 = false;
            pcfg.allowNoJoints = true;
            pcfg.enableWritebackFallback = false;
            pcfg.minWritebackCandidates = 8;

            physics::pbd::WorldConfig wcfg;
            wcfg.solverIterations = 10;
            wcfg.substeps = 2;
            wcfg.gravity = {0.0f, -9.8f, 0.0f};
            wcfg.globalDamping = 0.03f;
            wcfg.enableGroundPlane = true;
            wcfg.groundY = -0.6f;
            modelAsset_.clothWorld->setConfig(wcfg);

            const bool built = physics::pbd::buildFromPmxPhysics(
                modelAsset_.pmx,
                modelAsset_.skeleton,
                modelInstance_.pose,
                *modelAsset_.clothWorld,
                *modelAsset_.clothBinding,
                pcfg
            );
            if (!built) {
                core::log::warn("Physics build skipped (no PMX rigid bodies/joints?).");
            } else {
                modelAsset_.physicsBuilt = true;
                core::log::info(
                    "Physics built: particles=" + std::to_string(modelAsset_.clothWorld->particles().size()) +
                    " pins=" + std::to_string(modelAsset_.clothWorld->pins().size())
                );
            }
        }

        modelInstance_.modelMatrix = glm::mat4(1.0f);

        // 如果模型太大/太小，可以临时取消注释这一行自己调
        // modelInstance_.modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.08f));

        namespace fs = std::filesystem;
        const fs::path pmxBaseDir = fs::path(pmxPath).parent_path();
        modelTextureBaseDir_ = pmxBaseDir.string();
        modelTextures_.clear();
        auto loadModelTexture = [&](const std::string& inputPath, const std::string& label) {
            if (inputPath.empty()) {
                return;
            }
            const std::string normalized = normalizePmxTexturePath(inputPath);
            const std::string resolvedPath =
                (pmxBaseDir / fs::path(normalized)).lexically_normal().string();

            for (const auto& slot : modelTextures_) {
                if (slot.path == resolvedPath) {
                    return;
                }
            }

            ModelTextureSlot slot;
            slot.path = resolvedPath;
            slot.texture = std::make_unique<render::Texture>();
            if (!slot.texture->load2D(slot.path, false)) {
                core::log::warn("Failed to load PMX " + label + " texture: " + slot.path);
                return;
            }
            modelTextures_.push_back(std::move(slot));
        };

        for (const auto& mat : modelAsset_.pmx.materials) {
            loadModelTexture(mat.diffuseTexturePath, "diffuse");
            loadModelTexture(mat.toonTexturePath, "toon");
        }

        core::log::info("PMX material summary:");
        for (const auto& mat : modelAsset_.pmx.materials) {
            core::log::info(
                "  mat=" + mat.name +
                " idxOffset=" + std::to_string(mat.indexOffset) +
                " idxCount=" + std::to_string(mat.indexCount) +
                " twoSided=" + std::string(mat.twoSided ? "true" : "false") +
                " diffuse=" + (mat.diffuseTexturePath.empty() ? std::string("<none>") : mat.diffuseTexturePath) +
                " toon=" + (mat.toonTexturePath.empty() ? std::string("<none>") : mat.toonTexturePath)
            );
        }
    }

    {
        scene::Entity groundEntity;
        groundEntity.name = "ground";
        scene_.addEntity(groundEntity);

        scene::Entity modelEntity;
        modelEntity.name = "pmx_model";
        scene_.addEntity(modelEntity);
    }

    return true;
}




void Application::update(float dt) {
    if (!window_) {
        return;
    }

    static bool prevToggle = false;
    const bool toggleDown = platform::input::isKeyDown(*window_, GLFW_KEY_P);
    if (toggleDown && !prevToggle) {
        modelAsset_.physicsEnabled = !modelAsset_.physicsEnabled;
        core::log::info(std::string("Physics ") + (modelAsset_.physicsEnabled ? "enabled" : "disabled"));

        // If physics is enabled at runtime, build the world now (it is not built by default).
        if (modelAsset_.physicsEnabled &&
            modelAsset_.clothWorld &&
            modelAsset_.clothBinding &&
            !modelAsset_.physicsBuilt) {
            physics::pbd::BuilderConfig pcfg;
            pcfg.distanceStiffness = 0.85f;
            pcfg.pinStiffness = 1.0f;
            pcfg.defaultInvMass = 1.0f;
            pcfg.filterClothLike = true;
            pcfg.allowWritebackMode1 = false;
            pcfg.allowNoJoints = true;
            pcfg.enableWritebackFallback = false;
            pcfg.minWritebackCandidates = 8;
        
            physics::pbd::WorldConfig wcfg;
            wcfg.solverIterations = 10;
            wcfg.substeps = 2;
            wcfg.gravity = {0.0f, -9.8f, 0.0f};
            wcfg.globalDamping = 0.03f;
            wcfg.enableGroundPlane = true;
            wcfg.groundY = -0.6f;
            modelAsset_.clothWorld->setConfig(wcfg);
        
            const bool built = physics::pbd::buildFromPmxPhysics(
                modelAsset_.pmx,
                modelAsset_.skeleton,
                modelInstance_.pose,
                *modelAsset_.clothWorld,
                *modelAsset_.clothBinding,
                pcfg
            );
            if (!built) {
                core::log::warn("Physics build failed (filter may select 0 bodies).");
                modelAsset_.physicsBuilt = false;
            } else {
                modelAsset_.physicsBuilt = true;
                core::log::info(
                    "Physics built: particles=" + std::to_string(modelAsset_.clothWorld->particles().size()) +
                    " pins=" + std::to_string(modelAsset_.clothWorld->pins().size())
                );
            }
        }
    }
    prevToggle = toggleDown;

    if (platform::input::isKeyDown(*window_, GLFW_KEY_ESCAPE) ||
        platform::input::isKeyDown(*window_, GLFW_KEY_Q)) {
        window_->requestClose();
        return;
    }

    cameraController_.update(camera_, *window_, dt);

    if (modelInstance_.asset != nullptr &&
        modelInstance_.asset->clip.duration > 0.0f &&
        !modelInstance_.asset->clip.boneTracks.empty()) {
        modelInstance_.animator.update(dt);
        modelInstance_.animator.sample(modelInstance_.pose);

        // Cloth-like physics step (PBD) after sampling (includes IK).
        if (modelAsset_.physicsEnabled &&
            modelAsset_.physicsBuilt &&
            modelAsset_.clothWorld &&
            modelAsset_.clothBinding &&
            !modelAsset_.pmx.rigidBodies.empty()) {
            physics::pbd::updatePinsFromPose(
                modelAsset_.pmx,
                modelInstance_.pose,
                *modelAsset_.clothWorld,
                *modelAsset_.clothBinding
            );
            modelAsset_.clothWorld->step(dt);

            // Convert particle positions into pose overrides.
            std::vector<glm::vec3> particlePos;
            particlePos.reserve(modelAsset_.clothWorld->particles().size());
            for (const auto& p : modelAsset_.clothWorld->particles()) {
                particlePos.push_back(p.pos);
            }

            animation::pose_physics::applyParticleTranslations(
                modelAsset_.skeleton,
                modelInstance_.pose,
                particlePos,
                modelAsset_.clothBinding->particleToBone,
                &modelAsset_.clothBinding->particleBindPos,
                &modelAsset_.clothBinding->particleAffectsPose,
                // Secondary motion should be subtle; reject large teleports.
                0.15f
            );
        }

        animation::skinning::buildSkinningMatrices(
            modelInstance_.asset->skeleton,
            modelInstance_.pose,
            modelInstance_.skinningMatrices
        );

        // Apply morphs (facial expressions) after sampling time.
        applyVertexMorphsToMesh(modelAsset_, modelInstance_.animator.currentTime());
    }
}

// void Application::renderFrame() {
//     renderer_.beginFrame(camera_);

//     {
//         render::Material groundMaterial = demoMaterial_;
//         groundMaterial.baseColor = {0.75f, 0.78f, 0.84f, 1.0f};
//         groundMaterial.twoSided = false;

//         const glm::mat4 groundModel =
//             glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.6f, 0.0f)) *
//             glm::scale(glm::mat4(1.0f), glm::vec3(3.0f, 1.0f, 3.0f));

//         renderer_.drawMesh(demoMesh_, groundMaterial, groundModel);
//     }

//     if (modelInstance_.asset != nullptr && modelInstance_.asset->mesh.valid()) {
//         render::Material modelMaterial = demoMaterial_;
//         modelMaterial.baseColor = {0.95f, 0.82f, 0.88f, 1.0f};
//         modelMaterial.twoSided = true;

//         renderer_.drawSkinnedMesh(
//             modelInstance_.asset->mesh,
//             modelMaterial,
//             modelInstance_.modelMatrix,
//             modelInstance_.skinningMatrices
//         );
//     }

//     renderer_.endFrame();
// }



void Application::renderFrame() {
    renderer_.beginFrame(camera_);

    {
        render::Material groundMaterial = demoMaterial_;
        groundMaterial.baseColor = {0.75f, 0.78f, 0.84f, 1.0f};
        groundMaterial.twoSided = false;
        groundMaterial.diffuse = sWhiteTexture.valid() ? &sWhiteTexture : nullptr;

        const glm::mat4 groundModel =
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.6f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(20.0f, 1.0f, 20.0f));

        renderer_.drawMesh(demoMesh_, groundMaterial, groundModel);
    }

    if (modelInstance_.asset != nullptr && modelInstance_.asset->mesh.valid()) {
        if (modelAsset_.pmx.materials.empty()) {
            render::Material modelMaterial;
            modelMaterial.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
            modelMaterial.diffuse =
                sModelTexture.valid() ? &sModelTexture :
                (sWhiteTexture.valid() ? &sWhiteTexture : nullptr);
            modelMaterial.twoSided = true;
            renderer_.drawSkinnedMesh(
                modelInstance_.asset->mesh,
                modelMaterial,
                modelInstance_.modelMatrix,
                modelInstance_.skinningMatrices
            );
        } else {
            namespace fs = std::filesystem;
            const fs::path pmxBaseDir = fs::path(modelTextureBaseDir_);
            struct MaterialDrawItem {
                render::Material material;
                int indexOffset = 0;
                int indexCount = 0;
                std::string name;
            };
            std::vector<MaterialDrawItem> opaqueItems;
            std::vector<MaterialDrawItem> transparentItems;
            opaqueItems.reserve(modelAsset_.pmx.materials.size());
            transparentItems.reserve(modelAsset_.pmx.materials.size());

            for (const auto& mat : modelAsset_.pmx.materials) {
                render::Material modelMaterial;
                modelMaterial.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
                modelMaterial.diffuse = sWhiteTexture.valid() ? &sWhiteTexture : nullptr;
                modelMaterial.toon = nullptr;
                modelMaterial.twoSided = mat.twoSided;
                modelMaterial.depthBias = 0.0f;
                modelMaterial.alphaCutoff = 0.0f;
                modelMaterial.transparentBlend = false;
                const render::Texture* resolvedTexture = nullptr;

                if (!mat.diffuseTexturePath.empty()) {
                    const std::string normalized = normalizePmxTexturePath(mat.diffuseTexturePath);
                    const std::string resolvedPath =
                        (pmxBaseDir / fs::path(normalized)).lexically_normal().string();
                    for (auto& slot : modelTextures_) {
                        if (slot.path == resolvedPath && slot.texture && slot.texture->valid()) {
                            modelMaterial.diffuse = slot.texture.get();
                            resolvedTexture = slot.texture.get();
                            break;
                        }
                    }
                }

                if (!mat.toonTexturePath.empty()) {
                    const std::string normalizedToon = normalizePmxTexturePath(mat.toonTexturePath);
                    const std::string resolvedToonPath =
                        (pmxBaseDir / fs::path(normalizedToon)).lexically_normal().string();
                    for (auto& slot : modelTextures_) {
                        if (slot.path == resolvedToonPath && slot.texture && slot.texture->valid()) {
                            modelMaterial.toon = slot.texture.get();
                            break;
                        }
                    }
                }

                // Generic two-sided policy:
                // keep two-sided only for texture-cutout style materials.
                // Opaque shell materials should stay single-sided to avoid inner/outer z-fighting.
                if (resolvedTexture == nullptr || !resolvedTexture->hasTransparency()) {
                    modelMaterial.twoSided = false;
                }

                // Conservative generic rule:
                // apply a small depth bias only to likely decal-like transparent layers.
                if (resolvedTexture != nullptr &&
                    resolvedTexture->hasTransparency() &&
                    mat.twoSided &&
                    resolvedTexture->transparentPixelRatio() > 0.25f &&
                    mat.indexCount <= 3000) {
                    modelMaterial.depthBias = -0.2f;
                }

                // Stabilize alpha-edge flicker for packed PMX atlas textures:
                // avoid per-texture alpha-ratio blend switching (too coarse for atlas materials).
                // Keep depth-write path and use cutout only.
                if (resolvedTexture != nullptr && resolvedTexture->hasTransparency()) {
                    if (modelMaterial.twoSided) {
                        modelMaterial.alphaCutoff = 0.10f;
                        modelMaterial.transparentBlend = false;
                    } else {
                        // Single-sided opaque shells that share atlas textures
                        // should not be cut out by global texture alpha statistics.
                        modelMaterial.alphaCutoff = 0.0f;
                        modelMaterial.transparentBlend = false;
                    }
                }
                MaterialDrawItem item;
                item.material = modelMaterial;
                item.indexOffset = mat.indexOffset;
                item.indexCount = mat.indexCount;
                item.name = mat.name;
                if (modelMaterial.transparentBlend) {
                    transparentItems.push_back(item);
                } else {
                    opaqueItems.push_back(item);
                }
            }

            for (const auto& item : opaqueItems) {
                renderer_.drawSkinnedMeshRange(
                    modelInstance_.asset->mesh,
                    item.material,
                    modelInstance_.modelMatrix,
                    modelInstance_.skinningMatrices,
                    item.indexOffset,
                    item.indexCount
                );
            }
            for (const auto& item : transparentItems) {
                renderer_.drawSkinnedMeshRange(
                    modelInstance_.asset->mesh,
                    item.material,
                    modelInstance_.modelMatrix,
                    modelInstance_.skinningMatrices,
                    item.indexOffset,
                    item.indexCount
                );
            }
        }
    }

    renderer_.endFrame();
}

} // namespace app