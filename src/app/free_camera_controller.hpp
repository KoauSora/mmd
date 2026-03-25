#pragma once

#include "camera.hpp"
#include "window.hpp"

#include <glm/glm.hpp>

namespace app {

struct FreeCameraConfig {
    glm::vec3 position {0.0f, 1.0f, 4.0f};
    glm::vec3 target {0.0f, 0.8f, 0.0f};
    glm::vec3 up {0.0f, 1.0f, 0.0f};

    float fovDegrees = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    float moveSpeed = 3.5f;
    float fastMultiplier = 3.0f;
    float mouseSensitivity = 0.08f;
    float panSensitivity = 0.015f;
};

class FreeCameraController {
public:
    void initialize(scene::Camera& camera, float aspect, const FreeCameraConfig& config = {});
    void update(scene::Camera& camera, const platform::Window& window, float dt);

private:
    void syncAnglesFromCamera(const scene::Camera& camera);

private:
    float yaw_ = -90.0f;
    float pitch_ = 0.0f;

    float moveSpeed_ = 3.5f;
    float fastMultiplier_ = 3.0f;
    float mouseSensitivity_ = 0.08f;
    float panSensitivity_ = 0.015f;
    float rotationDamping_ = 8.0f;
    float panDamping_ = 10.0f;

    bool rotating_ = false;
    bool firstMouseSample_ = true;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
    float angularVelocityYaw_ = 0.0f;
    float angularVelocityPitch_ = 0.0f;

    bool panning_ = false;
    bool firstPanSample_ = true;
    double lastPanMouseX_ = 0.0;
    double lastPanMouseY_ = 0.0;
    glm::vec3 panVelocity_ {0.0f, 0.0f, 0.0f};
};

} // namespace app