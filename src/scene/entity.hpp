#pragma once

#include "transform.hpp"

#include <string>

namespace scene {

struct Entity {
    std::string name;
    Transform transform;
};

} // namespace scene