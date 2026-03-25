#pragma once

#include "entity.hpp"

#include <vector>

namespace scene {

class Scene {
public:
    void addEntity(const Entity& entity);
    const std::vector<Entity>& entities() const;

private:
    std::vector<Entity> entities_;
};

} // namespace scene