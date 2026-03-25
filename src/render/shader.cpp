#include "shader.hpp"

#include "log.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <vector>

namespace render {

static GLuint compileStage(GLenum stage, const std::string& source) {
    const GLuint shader = glCreateShader(stage);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        std::string log;
        log.resize(static_cast<std::size_t>(logLength > 0 ? logLength : 1));

        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        core::log::error(log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

Shader::~Shader() {
    destroy();
}

bool Shader::readTextFile(const std::string& path, std::string& outText) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        core::log::error("Failed to open shader file: " + path);
        return false;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    outText = oss.str();
    return true;
}

bool Shader::compileAndLink(const std::string& vertexSource, const std::string& fragmentSource) {
    destroy();

    const GLuint vert = compileStage(GL_VERTEX_SHADER, vertexSource);
    if (!vert) {
        return false;
    }

    const GLuint frag = compileStage(GL_FRAGMENT_SHADER, fragmentSource);
    if (!frag) {
        glDeleteShader(vert);
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vert);
    glAttachShader(program_, frag);
    glLinkProgram(program_);

    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint success = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &logLength);

        std::string log;
        log.resize(static_cast<std::size_t>(logLength > 0 ? logLength : 1));

        glGetProgramInfoLog(program_, logLength, nullptr, log.data());
        core::log::error(log);

        destroy();
        return false;
    }

    return true;
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexSource;
    std::string fragmentSource;

    if (!readTextFile(vertexPath, vertexSource)) {
        return false;
    }
    if (!readTextFile(fragmentPath, fragmentSource)) {
        return false;
    }

    return compileAndLink(vertexSource, fragmentSource);
}

void Shader::destroy() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

void Shader::use() const {
    glUseProgram(program_);
}

bool Shader::valid() const {
    return program_ != 0;
}

void Shader::setInt(const std::string& name, int value) const {
    const GLint loc = glGetUniformLocation(program_, name.c_str());
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

void Shader::setFloat(const std::string& name, float value) const {
    const GLint loc = glGetUniformLocation(program_, name.c_str());
    if (loc >= 0) {
        glUniform1f(loc, value);
    }
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    const GLint loc = glGetUniformLocation(program_, name.c_str());
    if (loc >= 0) {
        glUniform3fv(loc, 1, glm::value_ptr(value));
    }
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const {
    const GLint loc = glGetUniformLocation(program_, name.c_str());
    if (loc >= 0) {
        glUniform4fv(loc, 1, glm::value_ptr(value));
    }
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
    const GLint loc = glGetUniformLocation(program_, name.c_str());
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    }
}

void Shader::setMat4Array(const std::string& name, const std::vector<glm::mat4>& values) const {
    if (values.empty()) {
        return;
    }

    const GLint loc = glGetUniformLocation(program_, name.c_str());
    if (loc >= 0) {
        glUniformMatrix4fv(
            loc,
            static_cast<GLsizei>(values.size()),
            GL_FALSE,
            glm::value_ptr(values[0])
        );
    }
}

} // namespace render