#include "input.hpp"

#include "window.hpp"

#include <GLFW/glfw3.h>
#include <unordered_map>

namespace platform::input {
namespace {

std::unordered_map<GLFWwindow*, double> sScrollDeltaY;

void ensureScrollCallbackInstalled(GLFWwindow* handle) {
    if (!handle) {
        return;
    }
    if (sScrollDeltaY.contains(handle)) {
        return;
    }

    sScrollDeltaY[handle] = 0.0;
    glfwSetScrollCallback(handle, [](GLFWwindow* window, double /*xoffset*/, double yoffset) {
        sScrollDeltaY[window] += yoffset;
    });
}

} // namespace

bool isKeyDown(const Window& window, int key) {
    GLFWwindow* handle = window.nativeHandle();
    if (!handle) {
        return false;
    }

    return glfwGetKey(handle, key) == GLFW_PRESS;
}

bool isMouseButtonDown(const Window& window, int button) {
    GLFWwindow* handle = window.nativeHandle();
    if (!handle) {
        return false;
    }

    return glfwGetMouseButton(handle, button) == GLFW_PRESS;
}

void getCursorPosition(const Window& window, double& x, double& y) {
    GLFWwindow* handle = window.nativeHandle();
    if (!handle) {
        x = 0.0;
        y = 0.0;
        return;
    }

    glfwGetCursorPos(handle, &x, &y);
}

void setCursorMode(const Window& window, CursorMode mode) {
    GLFWwindow* handle = window.nativeHandle();
    if (!handle) {
        return;
    }

    int glfwMode = GLFW_CURSOR_NORMAL;
    switch (mode) {
    case CursorMode::Normal:
        glfwMode = GLFW_CURSOR_NORMAL;
        break;
    case CursorMode::Hidden:
        glfwMode = GLFW_CURSOR_HIDDEN;
        break;
    case CursorMode::Disabled:
        glfwMode = GLFW_CURSOR_DISABLED;
        break;
    }

    glfwSetInputMode(handle, GLFW_CURSOR, glfwMode);
}

double consumeScrollDeltaY(const Window& window) {
    GLFWwindow* handle = window.nativeHandle();
    if (!handle) {
        return 0.0;
    }

    ensureScrollCallbackInstalled(handle);
    const double delta = sScrollDeltaY[handle];
    sScrollDeltaY[handle] = 0.0;
    return delta;
}

} // namespace platform::input