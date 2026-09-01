#pragma once

#include "Vector3.h"
#include "Matrix3x3.h"

namespace Sunvoltum {

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

        // Создание по координатам, без поворота
        static CFrame FromPosition(float x, float y, float z)
        {
            return CFrame(Vector3(x, y, z));
        }

        // Только ротация из углов Эйлера в радианах, позиция = (0, 0, 0)
        // Пример: CFrame::Angles(0, 1.57f, 0) — 90° вокруг Y
        static CFrame Angles(float pitch, float yaw, float roll)
        {
            return CFrame(Vector3(0.0f, 0.0f, 0.0f), Matrix3x3::FromEuler(pitch, yaw, roll));
        }

        // Позиция + ориентация из Vector3 углов в радианах
        static CFrame FromPositionOrientation(const Vector3& position, const Vector3& eulerRad)
        {
            return CFrame(position, Matrix3x3::FromEuler(eulerRad.X, eulerRad.Y, eulerRad.Z));
        }

        // Комбинирование двух CFrame: self * other
        CFrame operator*(const CFrame& other) const
        {
            CFrame result;
            result.Rotation = Rotation * other.Rotation;
            result.Position = Position + (Rotation * other.Position);
            return result;
        }

        // Возвращает Matrix3x3 из Vector3 углов Эйлера в радианах.
        // Используется как второй аргумент CFrame(pos, cframe.FromOrientation(rot)):
        //   auto rot = cframe.Rotation.ToEuler();
        //   rot.Z += dt;
        //   sphere.SetProperty(..., CFrame(pos, cframe.FromOrientation(rot)));
        Matrix3x3 FromOrientation(const Vector3& eulerRad) const
        {
            return Matrix3x3::FromEuler(eulerRad.X, eulerRad.Y, eulerRad.Z);
        }
    };

} // namespace Sunvoltum
