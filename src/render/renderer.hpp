#pragma once

#include "material.hpp"
#include "mesh.hpp"
#include "shader.hpp"
#include "skinned_mesh.hpp"

#include <glm/glm.hpp>

struct GLFWwindow;

namespace scene {
class Camera;
}

namespace render {

class Renderer {
public:
    bool init(GLFWwindow* window);
    void shutdown();

    void beginFrame(const scene::Camera& camera);
    void endFrame();

    void drawMesh(const Mesh& mesh, const Material& material, const glm::mat4& model);
    void drawSkinnedMesh(
        const SkinnedMesh& mesh,
        const Material& material,
        const glm::mat4& model,
        const std::vector<glm::mat4>& bones
    );
    void drawSkinnedMeshRange(
        const SkinnedMesh& mesh,
        const Material& material,
        const glm::mat4& model,
        const std::vector<glm::mat4>& bones,
        int indexOffset,
        int indexCount
    );

private:
    bool initHdrTargets(int width, int height);
    void destroyHdrTargets();
    bool initScreenQuad();
    void destroyScreenQuad();

private:
    GLFWwindow* window_ = nullptr;
    Shader staticShader_;
    Shader skinnedShader_;
    Shader postShader_;
    glm::mat4 view_ {1.0f};
    glm::mat4 proj_ {1.0f};
    glm::vec3 cameraPos_ {0.0f};
    unsigned int boneMatrixBuffer_ = 0;
    unsigned int boneMatrixTexture_ = 0;
    unsigned int hdrFbo_ = 0;
    unsigned int hdrColorTex_ = 0;
    unsigned int hdrDepthRbo_ = 0;
    int hdrWidth_ = 0;
    int hdrHeight_ = 0;
    unsigned int screenQuadVao_ = 0;
    unsigned int screenQuadVbo_ = 0;
};

} // namespace render