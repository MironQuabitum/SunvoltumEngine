#pragma once

#include "SunvoltumPhysics/Dynamics/RigidBody.h"
#include "SunvoltumPhysics/Dynamics/Joint.h"
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

    struct SolverJointConstraint {
        Joint* joint{ nullptr };
        RigidBody* bodyA{ nullptr };
        RigidBody* bodyB{ nullptr };

        // Linear (anchor) constraint (3 DOF)
        Vector3 rA;
        Vector3 rB;
        Matrix3x3 linearEffectiveMass;
        Vector3 linearBias;
        Vector3 accumulatedLinearImpulse;

        // Angular constraint
        // For Fixed: 3 DOF locked
        Matrix3x3 angularEffectiveMass;
        Vector3 angularBias;
        Vector3 accumulatedAngularImpulse;

        // For Motor: 1 DOF twist (X-axis), 2 DOF swing (Y, Z axes)
        Vector3 twistAxis;   // in world space
        Vector3 swingAxis1;  // in world space
        Vector3 swingAxis2;  // in world space

        float twistEffectiveMass{ 0.0f };
        float twistTargetVelocity{ 0.0f };
        float accumulatedTwistImpulse{ 0.0f };

        float swingEffectiveMass1{ 0.0f };
        float swingEffectiveMass2{ 0.0f };
        float swingBias1{ 0.0f };
        float swingBias2{ 0.0f };
        float accumulatedSwingImpulse1{ 0.0f };
        float accumulatedSwingImpulse2{ 0.0f };
    };

    class PGSSolver {
    public:
        void Solve(std::vector<ContactManifold>& manifolds, float dt, uint32_t iterations = Settings::VELOCITY_ITERATIONS);
        void Solve(std::vector<ContactManifold>& manifolds, std::vector<std::shared_ptr<Joint>>& joints, float dt, uint32_t iterations = Settings::VELOCITY_ITERATIONS);

    private:
        void PrepareConstraints(std::vector<ContactManifold>& manifolds, float dt);
        void PrepareJointConstraints(std::vector<std::shared_ptr<Joint>>& joints, float dt);
        void WarmStart();
        void WarmStartJoints();
        void SolveVelocityConstraints();
        void SolveJointConstraints();
        void StoreImpulses();
        void StoreJointImpulses(float dt);

    private:
        std::vector<SolverConstraint> m_constraints;
        std::vector<SolverJointConstraint> m_jointConstraints;
    };

} // namespace SunvoltumPhysics
