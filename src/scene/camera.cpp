#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace scene {

void Camera::setPosition(const glm::vec3& position) {
    position_ = position;
}

void Camera::setTarget(const glm::vec3& target) {
    target_ = target;
}

void Camera::setUp(const glm::vec3& up) {
    up_ = up;
}

void Camera::setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane) {
    fovDegrees_ = fovDegrees;
    aspect_ = aspect;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
}

const glm::vec3& Camera::position() const {
    return position_;
}

const glm::vec3& Camera::target() const {
    return target_;
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position_, target_, up_);
}

glm::mat4 Camera::projectionMatrix(float overrideAspect) const {
    const float finalAspect = overrideAspect > 0.0f ? overrideAspect : aspect_;
    return glm::perspective(glm::radians(fovDegrees_), finalAspect, nearPlane_, farPlane_);
}

} // namespace scene