#include "window.hpp"

#include "log.hpp"

namespace platform {

Window::~Window() {
    destroy();
}

bool Window::create(int width, int height, const std::string& title) {
    if (!glfwInit()) {
        core::log::error("glfwInit failed.");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    handle_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!handle_) {
        core::log::error("glfwCreateWindow failed.");
        glfwTerminate();
        return false;
    }

    width_ = width;
    height_ = height;
    title_ = title;

    glfwMakeContextCurrent(handle_);
    // Disable VSync by default to avoid capping FPS.
    glfwSwapInterval(0);

    glfwSetWindowUserPointer(handle_, this);
    glfwSetFramebufferSizeCallback(handle_, [](GLFWwindow* window, int w, int h) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self) {
            self->width_ = w;
            self->height_ = h;
        }
    });

    return true;
}

void Window::destroy() {
    if (handle_) {
        glfwDestroyWindow(handle_);
        handle_ = nullptr;
        glfwTerminate();
    }
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::swapBuffers() const {
    if (handle_) {
        glfwSwapBuffers(handle_);
    }
}

bool Window::shouldClose() const {
    return handle_ ? glfwWindowShouldClose(handle_) != 0 : true;
}

void Window::requestClose() {
    if (handle_) {
        glfwSetWindowShouldClose(handle_, GLFW_TRUE);
    }
}

GLFWwindow* Window::nativeHandle() const {
    return handle_;
}

int Window::width() const {
    return width_;
}

int Window::height() const {
    return height_;
}

} // namespace platform