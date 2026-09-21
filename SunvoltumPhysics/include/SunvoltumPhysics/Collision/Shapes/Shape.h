#pragma once

#include "SunvoltumPhysics/Common/Math.h"
#include "SunvoltumPhysics/Collision/AABB.h"

namespace SunvoltumPhysics {

    enum class ShapeType {
        Sphere,
        Box,
        Plane,
        Capsule,
        ConvexMesh,
        TriangleMesh
    };

    class Shape {
    public:
        explicit Shape(ShapeType type) : m_type(type) {}
        virtual ~Shape() = default;

        ShapeType GetType() const { return m_type; }

        virtual AABB ComputeAABB(const Transform& transform) const = 0;
        virtual Matrix3x3 ComputeInertia(float mass) const = 0;

    protected:
        ShapeType m_type;
    };

} // namespace SunvoltumPhysics
