#include "log.hpp"

#include <iostream>

namespace core::log {

void info(std::string_view message) {
    std::cout << "[INFO] " << message << '\n';
}

void warn(std::string_view message) {
    std::cout << "[WARN] " << message << '\n';
}

void error(std::string_view message) {
    std::cerr << "[ERROR] " << message << '\n';
}

} // namespace core::log