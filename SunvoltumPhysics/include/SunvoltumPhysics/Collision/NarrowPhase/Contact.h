#pragma once

#include "SunvoltumPhysics/Common/Math.h"
#include <cstdint>
#include <vector>

namespace SunvoltumPhysics {

    class RigidBody;

    struct ContactPoint {
        Vector3 position{ 0.0f, 0.0f, 0.0f }; // World space
        Vector3 localPointA{ 0.0f, 0.0f, 0.0f };
        Vector3 localPointB{ 0.0f, 0.0f, 0.0f };
        float penetration{ 0.0f };
        
        // Solver accumulated impulses (Warm starting)
        float normalImpulse{ 0.0f };
        float tangentImpulse1{ 0.0f };
        float tangentImpulse2{ 0.0f };

        uint32_t featureId{ 0 };
    };

    struct ContactManifold {
        RigidBody* bodyA{ nullptr };
        RigidBody* bodyB{ nullptr };

        Vector3 normal{ 0.0f, 1.0f, 0.0f }; // Points from A to B
        ContactPoint points[4];
        int pointCount{ 0 };

        float friction{ 0.5f };
        float restitution{ 0.0f };

        void AddPoint(const ContactPoint& cp) {
            if (pointCount < 4) {
                points[pointCount++] = cp;
            }
        }
    };

} // namespace SunvoltumPhysics
