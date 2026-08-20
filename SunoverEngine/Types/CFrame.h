#pragma once

#include "Vector3.h"
#include "Matrix3x3.h"

namespace Sunover {

    // CFrame (Coordinate Frame) — позиция в пространстве + ориентация
    // Position : Vector3     (X, Y, Z)
    // Rotation : Matrix3x3   (без Gimbal Lock)
    struct CFrame
    {
        Vector3   Position;
        Matrix3x3 Rotation;

        CFrame() = default;

        explicit CFrame(const Vector3& position)
            : Position(position), Rotation(Matrix3x3::Identity()) {}

        CFrame(const Vector3& position, const Matrix3x3& rotation)
            : Position(position), Rotation(rotation) {}

        // Быстрое создание по координатам
        static CFrame FromPosition(float x, float y, float z)
        {
            return CFrame(Vector3(x, y, z));
        }

        // Комбинирование двух CFrame: self * other
        CFrame operator*(const CFrame& other) const
        {
            CFrame result;
            result.Rotation = Rotation * other.Rotation;
            result.Position = Position + (Rotation * other.Position);
            return result;
        }
    };

} // namespace Sunover
