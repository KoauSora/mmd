#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace physics::pbd {

struct Particle {
    glm::vec3 pos {0.0f};
    glm::vec3 prevPos {0.0f};
    float invMass = 0.0f; // 0 => pinned/kinematic
};

struct DistanceConstraint {
    int a = -1;
    int b = -1;
    float restLength = 0.0f;
    float stiffness = 1.0f; // [0..1]
};

struct PinConstraint {
    int p = -1;
    glm::vec3 target {0.0f};
    float stiffness = 1.0f;
};

struct WorldConfig {
    int solverIterations = 8;
    int substeps = 2;
    glm::vec3 gravity {0.0f, -9.8f, 0.0f};
    float globalDamping = 0.01f; // velocity damping per step

    // Minimal collisions (for visible cloth response).
    bool enableGroundPlane = true;
    float groundY = -0.6f;
};

class World {
public:
    void clear();

    int addParticle(const Particle& p);
    void addDistanceConstraint(const DistanceConstraint& c);
    void addPinConstraint(const PinConstraint& c);

    std::vector<Particle>& particles();
    const std::vector<Particle>& particles() const;

    std::vector<PinConstraint>& pins();
    const std::vector<PinConstraint>& pins() const;

    void setConfig(const WorldConfig& cfg);
    const WorldConfig& config() const;

    void step(float dt);

private:
    void integrate(float dt);
    void projectConstraints();
    void applyDamping(float factor);

private:
    WorldConfig cfg_;
    std::vector<Particle> particles_;
    std::vector<DistanceConstraint> distance_;
    std::vector<PinConstraint> pins_;
};

} // namespace physics::pbd

