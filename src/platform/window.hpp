#pragma once

#include <GLFW/glfw3.h>

#include <string>

namespace platform {

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(int width, int height, const std::string& title);
    void destroy();

    void pollEvents() const;
    void swapBuffers() const;

    bool shouldClose() const;
    void requestClose();

    GLFWwindow* nativeHandle() const;

    int width() const;
    int height() const;

private:
    GLFWwindow* handle_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    std::string title_;
};

} // namespace platform