#pragma once

#include "SunvoltumPhysics/Common/Math.h"
#include "SunvoltumPhysics/Collision/AABB.h"
#include <cmath>

namespace SunvoltumPhysics {

    struct OBB {
        Vector3 center{ 0.0f, 0.0f, 0.0f };
        Vector3 extents{ 0.5f, 0.5f, 0.5f }; // Half-extents
        Matrix3x3 orientation;               // Column or Row rotation vectors

        OBB() = default;
        OBB(const Vector3& c, const Vector3& e, const Quaternion& q)
            : center(c), extents(e), orientation(Matrix3x3::FromQuaternion(q)) {}
        OBB(const Vector3& c, const Vector3& e, const Matrix3x3& rot)
            : center(c), extents(e), orientation(rot) {}

        Vector3 GetAxis(int index) const {
            return { orientation.m[0][index], orientation.m[1][index], orientation.m[2][index] };
        }

        AABB ComputeAABB() const {
            Vector3 a0 = GetAxis(0);
            Vector3 a1 = GetAxis(1);
            Vector3 a2 = GetAxis(2);

            Vector3 worldExtents(
                std::abs(a0.x) * extents.x + std::abs(a1.x) * extents.y + std::abs(a2.x) * extents.z,
                std::abs(a0.y) * extents.x + std::abs(a1.y) * extents.y + std::abs(a2.y) * extents.z,
                std::abs(a0.z) * extents.x + std::abs(a1.z) * extents.y + std::abs(a2.z) * extents.z
            );

            return AABB(center - worldExtents, center + worldExtents);
        }
    };

} // namespace SunvoltumPhysics
