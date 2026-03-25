#include "free_camera_controller.hpp"

#include "input.hpp"

#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace app {
namespace {

glm::vec3 directionFromYawPitch(float yawDeg, float pitchDeg) {
    const float yaw = glm::radians(yawDeg);
    const float pitch = glm::radians(pitchDeg);

    glm::vec3 dir;
    dir.x = std::cos(yaw) * std::cos(pitch);
    dir.y = std::sin(pitch);
    dir.z = std::sin(yaw) * std::cos(pitch);
    return glm::normalize(dir);
}

} // namespace

void FreeCameraController::initialize(
    scene::Camera& camera,
    float aspect,
    const FreeCameraConfig& config
) {
    camera.setPosition(config.position);
    camera.setTarget(config.target);
    camera.setUp(config.up);
    camera.setPerspective(
        config.fovDegrees,
        aspect,
        config.nearPlane,
        config.farPlane
    );

    moveSpeed_ = config.moveSpeed;
    fastMultiplier_ = config.fastMultiplier;
    mouseSensitivity_ = config.mouseSensitivity;
    panSensitivity_ = config.panSensitivity;

    rotating_ = false;
    firstMouseSample_ = true;
    lastMouseX_ = 0.0;
    lastMouseY_ = 0.0;
    angularVelocityYaw_ = 0.0f;
    angularVelocityPitch_ = 0.0f;
    panning_ = false;
    firstPanSample_ = true;
    lastPanMouseX_ = 0.0;
    lastPanMouseY_ = 0.0;
    panVelocity_ = glm::vec3(0.0f);

    syncAnglesFromCamera(camera);
}

void FreeCameraController::syncAnglesFromCamera(const scene::Camera& camera) {
    const glm::vec3 dir = glm::normalize(camera.target() - camera.position());

    if (glm::length(dir) < 1e-6f) {
        return;
    }

    pitch_ = glm::degrees(std::asin(glm::clamp(dir.y, -1.0f, 1.0f)));
    yaw_ = glm::degrees(std::atan2(dir.z, dir.x));
}

void FreeCameraController::update(
    scene::Camera& camera,
    const platform::Window& window,
    float dt
) {
    const bool leftMouseDown =
        platform::input::isMouseButtonDown(window, GLFW_MOUSE_BUTTON_LEFT);
    const bool rightMouseDown =
        platform::input::isMouseButtonDown(window, GLFW_MOUSE_BUTTON_RIGHT);

    if (leftMouseDown) {
        if (!rotating_) {
            rotating_ = true;
            firstMouseSample_ = true;
        }
    } else {
        if (rotating_) {
            rotating_ = false;
            firstMouseSample_ = true;
        }
    }

    if (rotating_) {
        double mouseX = 0.0;
        double mouseY = 0.0;
        platform::input::getCursorPosition(window, mouseX, mouseY);

        if (firstMouseSample_) {
            lastMouseX_ = mouseX;
            lastMouseY_ = mouseY;
            firstMouseSample_ = false;
        }

        const float dx = static_cast<float>(mouseX - lastMouseX_);
        const float dy = static_cast<float>(mouseY - lastMouseY_);

        lastMouseX_ = mouseX;
        lastMouseY_ = mouseY;

        const float deltaYaw = dx * mouseSensitivity_;
        const float deltaPitch = -dy * mouseSensitivity_;
        const float safeDt = std::max(dt, 1e-4f);

        yaw_ += deltaYaw;
        pitch_ += deltaPitch;
        angularVelocityYaw_ = deltaYaw / safeDt;
        angularVelocityPitch_ = deltaPitch / safeDt;
        pitch_ = std::clamp(pitch_, -89.0f, 89.0f);
    } else {
        yaw_ += angularVelocityYaw_ * dt;
        pitch_ += angularVelocityPitch_ * dt;
        pitch_ = std::clamp(pitch_, -89.0f, 89.0f);

        const float damping = std::exp(-rotationDamping_ * dt);
        angularVelocityYaw_ *= damping;
        angularVelocityPitch_ *= damping;

        if (std::abs(angularVelocityYaw_) < 0.01f) {
            angularVelocityYaw_ = 0.0f;
        }
        if (std::abs(angularVelocityPitch_) < 0.01f) {
            angularVelocityPitch_ = 0.0f;
        }
    }

    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    const glm::vec3 forward = directionFromYawPitch(yaw_, pitch_);

    glm::vec3 right = glm::cross(forward, worldUp);
    if (glm::length(right) > 1e-6f) {
        right = glm::normalize(right);
    } else {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::vec3 flatForward(forward.x, 0.0f, forward.z);
    if (glm::length(flatForward) > 1e-6f) {
        flatForward = glm::normalize(flatForward);
    } else {
        flatForward = glm::vec3(0.0f, 0.0f, -1.0f);
    }

    glm::vec3 pos = camera.position();

    if (rightMouseDown) {
        if (!panning_) {
            panning_ = true;
            firstPanSample_ = true;
        }
    } else {
        if (panning_) {
            panning_ = false;
            firstPanSample_ = true;
        }
    }

    if (panning_) {
        double mouseX = 0.0;
        double mouseY = 0.0;
        platform::input::getCursorPosition(window, mouseX, mouseY);

        if (firstPanSample_) {
            lastPanMouseX_ = mouseX;
            lastPanMouseY_ = mouseY;
            firstPanSample_ = false;
        }

        const float dx = static_cast<float>(mouseX - lastPanMouseX_);
        const float dy = static_cast<float>(mouseY - lastPanMouseY_);
        lastPanMouseX_ = mouseX;
        lastPanMouseY_ = mouseY;

        const glm::vec3 panDelta = (-right * dx + worldUp * dy) * panSensitivity_;
        const float safeDt = std::max(dt, 1e-4f);
        pos += panDelta;
        panVelocity_ = panDelta / safeDt;
    } else {
        pos += panVelocity_ * dt;
        const float damping = std::exp(-panDamping_ * dt);
        panVelocity_ *= damping;
        if (glm::length(panVelocity_) < 0.001f) {
            panVelocity_ = glm::vec3(0.0f);
        }
    }

    float speed = moveSpeed_ * dt;
    if (platform::input::isKeyDown(window, GLFW_KEY_LEFT_SHIFT)) {
        speed *= fastMultiplier_;
    }

    if (platform::input::isKeyDown(window, GLFW_KEY_W)) {
        pos += flatForward * speed;
    }
    if (platform::input::isKeyDown(window, GLFW_KEY_S)) {
        pos -= flatForward * speed;
    }
    if (platform::input::isKeyDown(window, GLFW_KEY_A)) {
        pos -= right * speed;
    }
    if (platform::input::isKeyDown(window, GLFW_KEY_D)) {
        pos += right * speed;
    }
    if (platform::input::isKeyDown(window, GLFW_KEY_SPACE)) {
        pos += worldUp * speed;
    }
    if (platform::input::isKeyDown(window, GLFW_KEY_LEFT_CONTROL)) {
        pos -= worldUp * speed;
    }

    const float scrollY = static_cast<float>(platform::input::consumeScrollDeltaY(window));
    if (std::abs(scrollY) > 1e-6f) {
        const float zoomSpeed = moveSpeed_ * 0.75f;
        pos += forward * (scrollY * zoomSpeed);
    }

    camera.setPosition(pos);
    camera.setTarget(pos + forward);
    camera.setUp(worldUp);
}
} // namespace app