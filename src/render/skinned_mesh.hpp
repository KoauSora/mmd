#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace render {

struct SkinnedVertex {
    glm::vec3 position {0.0f};
    glm::vec3 normal {0.0f, 1.0f, 0.0f};
    glm::vec2 uv {0.0f};
    glm::ivec4 boneIds {0, 0, 0, 0};
    glm::vec4 boneWeights {1.0f, 0.0f, 0.0f, 0.0f};
};

class SkinnedMesh {
public:
    SkinnedMesh() = default;
    ~SkinnedMesh();

    SkinnedMesh(const SkinnedMesh&) = delete;
    SkinnedMesh& operator=(const SkinnedMesh&) = delete;

    bool create(const std::vector<SkinnedVertex>& vertices, const std::vector<std::uint32_t>& indices);
    bool updateVertices(const std::vector<SkinnedVertex>& vertices);
    void draw() const;
    void drawRange(int indexOffset, int indexCount) const;
    void destroy();

    bool valid() const;

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;
    GLsizei vertexCount_ = 0;
};

} // namespace render