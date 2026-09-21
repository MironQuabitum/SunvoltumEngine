#pragma once

#include "SunvoltumPhysics/Dynamics/RigidBody.h"
#include "SunvoltumPhysics/Collision/NarrowPhase/Contact.h"
#include "SunvoltumPhysics/Solver/PGSSolver.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace SunvoltumPhysics {

    struct RaycastHit {
        bool hit{ false };
        float distance{ 0.0f };
        Vector3 point{ 0.0f, 0.0f, 0.0f };
        Vector3 normal{ 0.0f, 1.0f, 0.0f };
        RigidBody* body{ nullptr };
    };

    class World {
    public:
        explicit World(const Vector3& gravity = Vector3(0.0f, Settings::DEFAULT_GRAVITY_Y, 0.0f))
            : m_gravity(gravity) {}

        void AddBody(std::shared_ptr<RigidBody> body) {
            m_bodies.push_back(body);
        }

        void RemoveBody(const std::shared_ptr<RigidBody>& body) {
            auto it = std::find(m_bodies.begin(), m_bodies.end(), body);
            if (it != m_bodies.end()) {
                m_bodies.erase(it);
            }
        }

        const std::vector<std::shared_ptr<RigidBody>>& GetBodies() const {
            return m_bodies;
        }

        const std::vector<ContactManifold>& GetManifolds() const {
            return m_manifolds;
        }

        void SetGravity(const Vector3& gravity) { m_gravity = gravity; }
        const Vector3& GetGravity() const { return m_gravity; }

        void Step(float dt);

        bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance,
                     RaycastHit& outHit, const RigidBody* ignoreBody = nullptr) const;

    private:
        void BroadPhase();
        void NarrowPhase();

    private:
        Vector3 m_gravity;
        std::vector<std::shared_ptr<RigidBody>> m_bodies;
        std::vector<std::pair<RigidBody*, RigidBody*>> m_broadPhasePairs;
        std::vector<ContactManifold> m_manifolds;
        PGSSolver m_solver;
    };

} // namespace SunvoltumPhysics
