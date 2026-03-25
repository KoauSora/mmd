#include "scene.hpp"

namespace scene {

void Scene::addEntity(const Entity& entity) {
    entities_.push_back(entity);
}

const std::vector<Entity>& Scene::entities() const {
    return entities_;
}

} // namespace scene