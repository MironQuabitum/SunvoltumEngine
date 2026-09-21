#pragma once

#include "SunvoltumPhysics/Dynamics/RigidBody.h"
#include "SunvoltumPhysics/Collision/NarrowPhase/Contact.h"
#include "SunvoltumPhysics/Common/Settings.h"
#include <vector>

namespace SunvoltumPhysics {

    struct SolverContactPoint {
        Vector3 rA;
        Vector3 rB;
        float normalMass{ 0.0f };
        float tangentMass1{ 0.0f };
        float tangentMass2{ 0.0f };
        float bias{ 0.0f };
        float normalImpulse{ 0.0f };
        float tangentImpulse1{ 0.0f };
        float tangentImpulse2{ 0.0f };
        uint32_t featureId{ 0 };
    };

    struct SolverConstraint {
        RigidBody* bodyA{ nullptr };
        RigidBody* bodyB{ nullptr };
        Vector3 normal;
        Vector3 tangent1;
        Vector3 tangent2;
        SolverContactPoint points[4];
        int pointCount{ 0 };
        float friction{ 0.0f };
        ContactManifold* sourceManifold{ nullptr };
    };

    class PGSSolver {
    public:
        void Solve(std::vector<ContactManifold>& manifolds, float dt, uint32_t iterations = Settings::VELOCITY_ITERATIONS);

    private:
        void PrepareConstraints(std::vector<ContactManifold>& manifolds, float dt);
        void WarmStart();
        void SolveVelocityConstraints();
        void StoreImpulses();

    private:
        std::vector<SolverConstraint> m_constraints;
    };

} // namespace SunvoltumPhysics
