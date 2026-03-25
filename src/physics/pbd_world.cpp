#include "pbd_world.hpp"

#include <algorithm>
#include <cmath>

namespace physics::pbd {

void World::clear() {
    particles_.clear();
    distance_.clear();
    pins_.clear();
}

int World::addParticle(const Particle& p) {
    particles_.push_back(p);
    return static_cast<int>(particles_.size() - 1);
}

void World::addDistanceConstraint(const DistanceConstraint& c) {
    distance_.push_back(c);
}

void World::addPinConstraint(const PinConstraint& c) {
    pins_.push_back(c);
}

std::vector<Particle>& World::particles() {
    return particles_;
}

const std::vector<Particle>& World::particles() const {
    return particles_;
}

std::vector<PinConstraint>& World::pins() {
    return pins_;
}

const std::vector<PinConstraint>& World::pins() const {
    return pins_;
}

void World::setConfig(const WorldConfig& cfg) {
    cfg_ = cfg;
    cfg_.solverIterations = std::max(cfg_.solverIterations, 1);
    cfg_.substeps = std::max(cfg_.substeps, 1);
}

const WorldConfig& World::config() const {
    return cfg_;
}

void World::step(float dt) {
    if (particles_.empty()) {
        return;
    }
    if (dt <= 0.0f) {
        return;
    }

    const int substeps = std::max(cfg_.substeps, 1);
    const float h = dt / static_cast<float>(substeps);

    for (int s = 0; s < substeps; ++s) {
        integrate(h);

        const int iters = std::max(cfg_.solverIterations, 1);
        for (int it = 0; it < iters; ++it) {
            projectConstraints();
        }

        // Damping: blend velocity via position history.
        const float damp = std::clamp(cfg_.globalDamping, 0.0f, 1.0f);
        if (damp > 0.0f) {
            applyDamping(1.0f - damp);
        }
    }
}

void World::integrate(float dt) {
    for (auto& p : particles_) {
        if (p.invMass <= 0.0f) {
            p.prevPos = p.pos;
            continue;
        }

        const glm::vec3 vel = p.pos - p.prevPos;
        p.prevPos = p.pos;

        // Semi-implicit Verlet: x += v + a*dt^2
        p.pos += vel + cfg_.gravity * (dt * dt);
    }
}

void World::projectConstraints() {
    // Pins first (strong stabilization)
    for (const auto& pin : pins_) {
        if (pin.p < 0 || pin.p >= static_cast<int>(particles_.size())) {
            continue;
        }
        auto& p = particles_[static_cast<std::size_t>(pin.p)];
        const float k = std::clamp(pin.stiffness, 0.0f, 1.0f);
        p.pos = glm::mix(p.pos, pin.target, k);
    }

    for (const auto& c : distance_) {
        if (c.a < 0 || c.b < 0) {
            continue;
        }
        if (c.a >= static_cast<int>(particles_.size()) ||
            c.b >= static_cast<int>(particles_.size())) {
            continue;
        }

        auto& pa = particles_[static_cast<std::size_t>(c.a)];
        auto& pb = particles_[static_cast<std::size_t>(c.b)];

        const glm::vec3 delta = pb.pos - pa.pos;
        const float len = glm::length(delta);
        if (len < 1e-6f) {
            continue;
        }

        const float w1 = pa.invMass;
        const float w2 = pb.invMass;
        const float wsum = w1 + w2;
        if (wsum <= 0.0f) {
            continue;
        }

        const float diff = (len - c.restLength) / len;
        const glm::vec3 corr = delta * diff;
        const float k = std::clamp(c.stiffness, 0.0f, 1.0f);

        if (w1 > 0.0f) {
            pa.pos += -k * (w1 / wsum) * corr;
        }
        if (w2 > 0.0f) {
            pb.pos += k * (w2 / wsum) * corr;
        }
    }

    // Ground plane collision (very simple position projection).
    if (cfg_.enableGroundPlane) {
        for (auto& p : particles_) {
            if (p.invMass <= 0.0f) {
                continue;
            }
            if (p.pos.y < cfg_.groundY) {
                p.pos.y = cfg_.groundY;
            }
        }
    }
}

void World::applyDamping(float factor) {
    factor = std::clamp(factor, 0.0f, 1.0f);
    for (auto& p : particles_) {
        if (p.invMass <= 0.0f) {
            continue;
        }
        const glm::vec3 vel = p.pos - p.prevPos;
        p.prevPos = p.pos - vel * factor;
    }
}

} // namespace physics::pbd

