#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace render {

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    void destroy();

    void use() const;
    bool valid() const;

    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    void setMat4Array(const std::string& name, const std::vector<glm::mat4>& values) const;

private:
    bool compileAndLink(const std::string& vertexSource, const std::string& fragmentSource);
    static bool readTextFile(const std::string& path, std::string& outText);

private:
    GLuint program_ = 0;
};

} // namespace render