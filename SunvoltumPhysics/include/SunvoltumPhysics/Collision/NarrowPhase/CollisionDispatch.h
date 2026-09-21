#pragma once

#include "SunvoltumPhysics/Dynamics/RigidBody.h"
#include "SunvoltumPhysics/Collision/NarrowPhase/Contact.h"

namespace SunvoltumPhysics {

    class CollisionDispatch {
    public:
        static bool TestCollision(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold);

    private:
        static bool SphereVsSphere(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold);
        static bool SphereVsPlane(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold);
        static bool SphereVsBox(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold);
        static bool BoxVsPlane(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold);
        static bool BoxVsBox(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold);
        static bool SphereVsTriangleMesh(RigidBody* bodySphere, RigidBody* bodyMesh, ContactManifold& outManifold);
        static bool BoxVsTriangleMesh(RigidBody* bodyBox, RigidBody* bodyMesh, ContactManifold& outManifold);
        static bool ConvexMeshVsConvexMesh(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold);
        static bool SphereVsConvexMesh(RigidBody* bodySphere, RigidBody* bodyConvex, ContactManifold& outManifold);
        static bool BoxVsConvexMesh(RigidBody* bodyBox, RigidBody* bodyConvex, ContactManifold& outManifold);
        static bool ConvexMeshVsTriangleMesh(RigidBody* bodyConvex, RigidBody* bodyMesh, ContactManifold& outManifold);
    };

} // namespace SunvoltumPhysics
