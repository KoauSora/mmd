#pragma once

#include "vmd_types.hpp"

#include <string>

namespace asset {

class VmdLoader {
public:
    static bool loadFromFile(const std::string& path, VmdClip& outClip);
};

} // namespace asset