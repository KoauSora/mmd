#pragma once

#include "pmx_types.hpp"

#include <string>

namespace asset {

class PmxLoader {
public:
    static bool loadFromFile(const std::string& path, PmxAsset& outAsset);
};

} // namespace asset