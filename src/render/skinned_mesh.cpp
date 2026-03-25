#include "skinned_mesh.hpp"

#include <cstddef>

namespace render {

SkinnedMesh::~SkinnedMesh() {
    destroy();
}

bool SkinnedMesh::create(const std::vector<SkinnedVertex>& vertices, const std::vector<std::uint32_t>& indices) {
    destroy();

    if (vertices.empty() || indices.empty()) {
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(SkinnedVertex)),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
        indices.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
        reinterpret_cast<void*>(offsetof(SkinnedVertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
        reinterpret_cast<void*>(offsetof(SkinnedVertex, normal))
    );

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
        reinterpret_cast<void*>(offsetof(SkinnedVertex, uv))
    );

    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(
        3, 4, GL_INT, sizeof(SkinnedVertex),
        reinterpret_cast<void*>(offsetof(SkinnedVertex, boneIds))
    );

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(
        4, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
        reinterpret_cast<void*>(offsetof(SkinnedVertex, boneWeights))
    );

    glBindVertexArray(0);

    indexCount_ = static_cast<GLsizei>(indices.size());
    vertexCount_ = static_cast<GLsizei>(vertices.size());
    return true;
}

bool SkinnedMesh::updateVertices(const std::vector<SkinnedVertex>& vertices) {
    if (!valid() || vbo_ == 0) {
        return false;
    }
    if (vertexCount_ <= 0 || static_cast<GLsizei>(vertices.size()) != vertexCount_) {
        return false;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(SkinnedVertex)),
        vertices.data()
    );
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void SkinnedMesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void SkinnedMesh::drawRange(int indexOffset, int indexCount) const {
    if (indexOffset < 0 || indexCount <= 0) {
        return;
    }
    if (indexOffset + indexCount > indexCount_) {
        return;
    }

    glBindVertexArray(vao_);
    const void* byteOffset =
        reinterpret_cast<const void*>(static_cast<std::size_t>(indexOffset) * sizeof(std::uint32_t));
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, byteOffset);
    glBindVertexArray(0);
}

void SkinnedMesh::destroy() {
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    indexCount_ = 0;
    vertexCount_ = 0;
}

bool SkinnedMesh::valid() const {
    return vao_ != 0;
}

} // namespace render