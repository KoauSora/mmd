#include "renderer.hpp"

#include "camera.hpp"
#include "log.hpp"
#include "texture.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>

namespace render {

bool Renderer::initHdrTargets(int width, int height) {
    destroyHdrTargets();
    if (width <= 0 || height <= 0) {
        return false;
    }

    glGenFramebuffers(1, &hdrFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo_);

    glGenTextures(1, &hdrColorTex_);
    glBindTexture(GL_TEXTURE_2D, hdrColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColorTex_, 0);

    glGenRenderbuffers(1, &hdrDepthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, hdrDepthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, hdrDepthRbo_);

    const bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!ok) {
        core::log::error("HDR framebuffer incomplete.");
        destroyHdrTargets();
        return false;
    }

    hdrWidth_ = width;
    hdrHeight_ = height;
    return true;
}

void Renderer::destroyHdrTargets() {
    if (hdrDepthRbo_ != 0) {
        glDeleteRenderbuffers(1, &hdrDepthRbo_);
        hdrDepthRbo_ = 0;
    }
    if (hdrColorTex_ != 0) {
        glDeleteTextures(1, &hdrColorTex_);
        hdrColorTex_ = 0;
    }
    if (hdrFbo_ != 0) {
        glDeleteFramebuffers(1, &hdrFbo_);
        hdrFbo_ = 0;
    }
    hdrWidth_ = 0;
    hdrHeight_ = 0;
}

bool Renderer::initScreenQuad() {
    destroyScreenQuad();
    const float quadVertices[] = {
        // pos      // uv
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };
    glGenVertexArrays(1, &screenQuadVao_);
    glGenBuffers(1, &screenQuadVbo_);
    glBindVertexArray(screenQuadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, screenQuadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);
    return screenQuadVao_ != 0 && screenQuadVbo_ != 0;
}

void Renderer::destroyScreenQuad() {
    if (screenQuadVbo_ != 0) {
        glDeleteBuffers(1, &screenQuadVbo_);
        screenQuadVbo_ = 0;
    }
    if (screenQuadVao_ != 0) {
        glDeleteVertexArrays(1, &screenQuadVao_);
        screenQuadVao_ = 0;
    }
}

bool Renderer::init(GLFWwindow* window) {
    window_ = window;

    if (!window_) {
        core::log::error("Renderer::init got null GLFWwindow.");
        return false;
    }

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        core::log::error("Failed to initialize GLAD.");
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!staticShader_.loadFromFiles("shaders/static.vert", "shaders/static.frag")) {
        core::log::error("Failed to load static shader.");
        return false;
    }

    if (!skinnedShader_.loadFromFiles("shaders/skinned.vert", "shaders/skinned.frag")) {
        core::log::error("Failed to load skinned shader.");
        return false;
    }
    if (!postShader_.loadFromFiles("shaders/post.vert", "shaders/post.frag")) {
        core::log::error("Failed to load post shader.");
        return false;
    }

    if (!initScreenQuad()) {
        core::log::error("Failed to create post-process screen quad.");
        return false;
    }

    glGenBuffers(1, &boneMatrixBuffer_);
    glGenTextures(1, &boneMatrixTexture_);
    glBindTexture(GL_TEXTURE_BUFFER, boneMatrixTexture_);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, boneMatrixBuffer_);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    int width = 1;
    int height = 1;
    glfwGetFramebufferSize(window_, &width, &height);
    if (!initHdrTargets(width, height)) {
        return false;
    }

    return true;
}

void Renderer::shutdown() {
    if (boneMatrixTexture_ != 0) {
        glDeleteTextures(1, &boneMatrixTexture_);
        boneMatrixTexture_ = 0;
    }
    if (boneMatrixBuffer_ != 0) {
        glDeleteBuffers(1, &boneMatrixBuffer_);
        boneMatrixBuffer_ = 0;
    }
    destroyHdrTargets();
    destroyScreenQuad();
    staticShader_.destroy();
    skinnedShader_.destroy();
    postShader_.destroy();
}

void Renderer::beginFrame(const scene::Camera& camera) {
    int width = 1;
    int height = 1;
    glfwGetFramebufferSize(window_, &width, &height);

    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;

    view_ = camera.viewMatrix();
    proj_ = camera.projectionMatrix(aspect);
    cameraPos_ = camera.position();

    if (width != hdrWidth_ || height != hdrHeight_) {
        if (!initHdrTargets(width, height)) {
            core::log::error("Failed to resize HDR targets.");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo_);
    glViewport(0, 0, width, height);
    glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::endFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    postShader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrColorTex_);
    postShader_.setInt("uHdrColor", 0);
    postShader_.setFloat("uExposure", 1.0f);
    postShader_.setFloat("uGamma", 2.2f);

    glBindVertexArray(screenQuadVao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawMesh(const Mesh& mesh, const Material& material, const glm::mat4& model) {
    if (!mesh.valid() || !staticShader_.valid()) {
        return;
    }

    if (material.transparentBlend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    if (material.twoSided) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    if (std::abs(material.depthBias) > 1e-6f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(material.depthBias, material.depthBias);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glDepthMask(material.transparentBlend ? GL_FALSE : GL_TRUE);

    staticShader_.use();
    staticShader_.setMat4("uModel", model);
    staticShader_.setMat4("uView", view_);
    staticShader_.setMat4("uProj", proj_);
    staticShader_.setVec3("uCameraPos", cameraPos_);
    staticShader_.setVec4("uBaseColor", material.baseColor);
    staticShader_.setVec3("uLightDir", glm::normalize(glm::vec3(-0.2f, -1.0f, 0.8f)));
    staticShader_.setFloat("uAlphaCutoff", material.alphaCutoff);

    if (material.diffuse && material.diffuse->valid()) {
        material.diffuse->bind(0);
        staticShader_.setInt("uUseTexture", 1);
        staticShader_.setInt("uDiffuseTex", 0);
    } else {
        staticShader_.setInt("uUseTexture", 0);
    }
    if (material.toon && material.toon->valid()) {
        material.toon->bind(1);
        staticShader_.setInt("uUseToonTex", 1);
        staticShader_.setInt("uToonTex", 1);
    } else {
        staticShader_.setInt("uUseToonTex", 0);
    }

    mesh.draw();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
}

void Renderer::drawSkinnedMesh(
    const SkinnedMesh& mesh,
    const Material& material,
    const glm::mat4& model,
    const std::vector<glm::mat4>& bones
) {
    drawSkinnedMeshRange(mesh, material, model, bones, 0, 0);
}

void Renderer::drawSkinnedMeshRange(
    const SkinnedMesh& mesh,
    const Material& material,
    const glm::mat4& model,
    const std::vector<glm::mat4>& bones,
    int indexOffset,
    int indexCount
) {
    if (!mesh.valid() || !skinnedShader_.valid()) {
        return;
    }

    if (material.transparentBlend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    if (material.twoSided) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    if (std::abs(material.depthBias) > 1e-6f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(material.depthBias, material.depthBias);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glDepthMask(material.transparentBlend ? GL_FALSE : GL_TRUE);

    skinnedShader_.use();
    skinnedShader_.setMat4("uModel", model);
    skinnedShader_.setMat4("uView", view_);
    skinnedShader_.setMat4("uProj", proj_);
    skinnedShader_.setVec3("uCameraPos", cameraPos_);
    skinnedShader_.setVec4("uBaseColor", material.baseColor);
    skinnedShader_.setVec3("uLightDir", glm::normalize(glm::vec3(-0.2f, -1.0f, 0.8f)));
    skinnedShader_.setFloat("uAlphaCutoff", material.alphaCutoff);

    if (!bones.empty() && boneMatrixBuffer_ != 0 && boneMatrixTexture_ != 0) {
        glBindBuffer(GL_TEXTURE_BUFFER, boneMatrixBuffer_);
        glBufferData(
            GL_TEXTURE_BUFFER,
            static_cast<GLsizeiptr>(bones.size() * sizeof(glm::mat4)),
            bones.data(),
            GL_DYNAMIC_DRAW
        );
        glBindTexture(GL_TEXTURE_BUFFER, boneMatrixTexture_);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, boneMatrixBuffer_);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_BUFFER, boneMatrixTexture_);
        skinnedShader_.setInt("uBonesTex", 3);
        skinnedShader_.setInt("uBoneCount", static_cast<int>(bones.size()));
    }

    if (material.diffuse && material.diffuse->valid()) {
        material.diffuse->bind(0);
        skinnedShader_.setInt("uUseTexture", 1);
        skinnedShader_.setInt("uDiffuseTex", 0);
    } else {
        skinnedShader_.setInt("uUseTexture", 0);
    }
    if (material.toon && material.toon->valid()) {
        material.toon->bind(1);
        skinnedShader_.setInt("uUseToonTex", 1);
        skinnedShader_.setInt("uToonTex", 1);
    } else {
        skinnedShader_.setInt("uUseToonTex", 0);
    }

    if (indexCount > 0) {
        mesh.drawRange(indexOffset, indexCount);
    } else {
        mesh.draw();
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
}

} // namespace render