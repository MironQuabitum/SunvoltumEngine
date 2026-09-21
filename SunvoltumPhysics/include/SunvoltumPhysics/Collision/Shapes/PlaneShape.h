#pragma once

#include "SunvoltumPhysics/Collision/Shapes/Shape.h"
#include <limits>

namespace SunvoltumPhysics {

    class PlaneShape : public Shape {
    public:
        PlaneShape(const Vector3& normal, float distance)
            : Shape(ShapeType::Plane), m_normal(normal.Normalized()), m_distance(distance) {}

        const Vector3& GetNormal() const { return m_normal; }
        float GetDistance() const { return m_distance; }

        AABB ComputeAABB(const Transform& /*transform*/) const override {
            constexpr float INF = 1e6f;
            return AABB(Vector3(-INF, -INF, -INF), Vector3(INF, INF, INF));
        }

        Matrix3x3 ComputeInertia(float /*mass*/) const override {
            return Matrix3x3(); // Infinite inertia for static planes
        }

    private:
        Vector3 m_normal{ 0.0f, 1.0f, 0.0f };
        float m_distance{ 0.0f };
    };

} // namespace SunvoltumPhysics
