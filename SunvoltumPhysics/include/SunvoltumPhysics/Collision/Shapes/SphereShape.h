#pragma once

#include "SunvoltumPhysics/Collision/Shapes/Shape.h"

namespace SunvoltumPhysics {

    class SphereShape : public Shape {
    public:
        explicit SphereShape(float radius) : Shape(ShapeType::Sphere), m_radius(radius) {}

        float GetRadius() const { return m_radius; }

        AABB ComputeAABB(const Transform& transform) const override {
            Vector3 rVec(m_radius, m_radius, m_radius);
            return AABB(transform.position - rVec, transform.position + rVec);
        }

        Matrix3x3 ComputeInertia(float mass) const override {
            Matrix3x3 I;
            float val = (2.0f / 5.0f) * mass * m_radius * m_radius;
            I.m[0][0] = val;
            I.m[1][1] = val;
            I.m[2][2] = val;
            return I;
        }

    private:
        float m_radius{ 0.5f };
    };

} // namespace SunvoltumPhysics
