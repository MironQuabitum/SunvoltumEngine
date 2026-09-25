#pragma once

#include <SunvoltumPhysics/Dynamics/World.h>
#include <SunvoltumPhysics/Dynamics/RigidBody.h>
#include <SunvoltumPhysics/Dynamics/Joint.h>
#include <SunvoltumPhysics/Collision/Shapes/BoxShape.h>
#include <SunvoltumPhysics/Collision/Shapes/SphereShape.h>
#include "../Types/Vector3.h"
#include "../Types/Matrix3x3.h"
#include "../Types/CFrame.h"
#include <cmath>

namespace Sunvoltum {

    // -----------------------------------------------------------------------
    // Конвертация между типами Sunvoltum и SunvoltumPhysics
    // -----------------------------------------------------------------------

    inline SunvoltumPhysics::Vector3 ToPhysicsVec3(const Vector3& v)
    {
        return SunvoltumPhysics::Vector3(v.X, v.Y, v.Z);
    }

    inline Vector3 FromPhysicsVec3(const SunvoltumPhysics::Vector3& v)
    {
        return Vector3(v.x, v.y, v.z);
    }

    // Построение SunvoltumPhysics::Quaternion из ротационной матрицы Sunvoltum::Matrix3x3 (row-major)
    inline SunvoltumPhysics::Quaternion ToPhysicsQuat(const Matrix3x3& r)
    {
        float trace = r.R00 + r.R11 + r.R22;
        if (trace > 0.0f)
        {
            float s = 0.5f / std::sqrt(trace + 1.0f);
            return SunvoltumPhysics::Quaternion(
                (r.R21 - r.R12) * s,
                (r.R02 - r.R20) * s,
                (r.R10 - r.R01) * s,
                0.25f / s
            );
        }
        else
        {
            if (r.R00 > r.R11 && r.R00 > r.R22)
            {
                float s = 2.0f * std::sqrt(1.0f + r.R00 - r.R11 - r.R22);
                return SunvoltumPhysics::Quaternion(
                    0.25f * s,
                    (r.R01 + r.R10) / s,
                    (r.R02 + r.R20) / s,
                    (r.R21 - r.R12) / s
                );
            }
            else if (r.R11 > r.R22)
            {
                float s = 2.0f * std::sqrt(1.0f + r.R11 - r.R00 - r.R22);
                return SunvoltumPhysics::Quaternion(
                    (r.R01 + r.R10) / s,
                    0.25f * s,
                    (r.R12 + r.R21) / s,
                    (r.R02 - r.R20) / s
                );
            }
            else
            {
                float s = 2.0f * std::sqrt(1.0f + r.R22 - r.R00 - r.R11);
                return SunvoltumPhysics::Quaternion(
                    (r.R02 + r.R20) / s,
                    (r.R12 + r.R21) / s,
                    0.25f * s,
                    (r.R10 - r.R01) / s
                );
            }
        }
    }

    // Построение Sunvoltum::Matrix3x3 из SunvoltumPhysics::Quaternion
    inline Matrix3x3 FromPhysicsQuat(const SunvoltumPhysics::Quaternion& q)
    {
        SunvoltumPhysics::Quaternion nq = q.Normalized();
        float xx = nq.x * nq.x;
        float yy = nq.y * nq.y;
        float zz = nq.z * nq.z;
        float xy = nq.x * nq.y;
        float xz = nq.x * nq.z;
        float yz = nq.y * nq.z;
        float wx = nq.w * nq.x;
        float wy = nq.w * nq.y;
        float wz = nq.w * nq.z;

        return Matrix3x3(
            1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz),        2.0f * (xz + wy),
            2.0f * (xy + wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx),
            2.0f * (xz - wy),        2.0f * (yz + wx),        1.0f - 2.0f * (xx + yy)
        );
    }

    inline SunvoltumPhysics::Transform ToPhysicsTransform(const CFrame& cf)
    {
        return SunvoltumPhysics::Transform(
            ToPhysicsVec3(cf.Position),
            ToPhysicsQuat(cf.Rotation)
        );
    }

    inline CFrame FromPhysicsTransform(const SunvoltumPhysics::Transform& t)
    {
        return CFrame(
            FromPhysicsVec3(t.position),
            FromPhysicsQuat(t.rotation)
        );
    }

} // namespace Sunvoltum
