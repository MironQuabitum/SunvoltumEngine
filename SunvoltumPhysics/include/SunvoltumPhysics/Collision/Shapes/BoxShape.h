#pragma once

#include "SunvoltumPhysics/Collision/Shapes/Shape.h"
#include "SunvoltumPhysics/Collision/OBB.h"

namespace SunvoltumPhysics {

    class BoxShape : public Shape {
    public:
        explicit BoxShape(const Vector3& halfExtents)
            : Shape(ShapeType::Box), m_halfExtents(halfExtents) {}

        const Vector3& GetHalfExtents() const { return m_halfExtents; }

        AABB ComputeAABB(const Transform& transform) const override {
            OBB obb(transform.position, m_halfExtents, transform.rotation);
            return obb.ComputeAABB();
        }

        Matrix3x3 ComputeInertia(float mass) const override {
            Matrix3x3 I;
            float factor = (1.0f / 12.0f) * mass;
            float dx = m_halfExtents.x * 2.0f;
            float dy = m_halfExtents.y * 2.0f;
            float dz = m_halfExtents.z * 2.0f;

            I.m[0][0] = factor * (dy * dy + dz * dz);
            I.m[1][1] = factor * (dx * dx + dz * dz);
            I.m[2][2] = factor * (dx * dx + dy * dy);
            return I;
        }

    private:
        Vector3 m_halfExtents{ 0.5f, 0.5f, 0.5f };
    };

} // namespace SunvoltumPhysics
