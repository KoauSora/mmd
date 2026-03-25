#pragma once

namespace platform {

class Window;

namespace input {

enum class CursorMode {
    Normal,
    Hidden,
    Disabled
};

bool isKeyDown(const Window& window, int key);
bool isMouseButtonDown(const Window& window, int button);

void getCursorPosition(const Window& window, double& x, double& y);
void setCursorMode(const Window& window, CursorMode mode);
double consumeScrollDeltaY(const Window& window);

} // namespace input
} // namespace platform