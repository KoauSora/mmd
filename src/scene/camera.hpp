#pragma once

#include <glm/glm.hpp>

namespace scene {

class Camera {
public:
    void setPosition(const glm::vec3& position);
    void setTarget(const glm::vec3& target);
    void setUp(const glm::vec3& up);

    void setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane);

    const glm::vec3& position() const;
    const glm::vec3& target() const;

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float overrideAspect = -1.0f) const;

private:
    glm::vec3 position_ {0.0f, 0.0f, 3.0f};
    glm::vec3 target_ {0.0f, 0.0f, 0.0f};
    glm::vec3 up_ {0.0f, 1.0f, 0.0f};

    float fovDegrees_ = 60.0f;
    float aspect_ = 16.0f / 9.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 100.0f;
};

} // namespace scene