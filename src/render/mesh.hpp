#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace render {

struct Vertex {
    glm::vec3 position {0.0f};
    glm::vec3 normal {0.0f, 1.0f, 0.0f};
    glm::vec2 uv {0.0f};
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    bool create(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);
    void draw() const;
    void destroy();

    bool valid() const;

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;
};

} // namespace render