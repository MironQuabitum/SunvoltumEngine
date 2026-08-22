#pragma once

#include <cmath>
#include "Vector3.h"

namespace Sunover {

    struct Matrix3x3
    {
        float R00 = 0.0f, R01 = 0.0f, R02 = 0.0f;
        float R10 = 0.0f, R11 = 0.0f, R12 = 0.0f;
        float R20 = 0.0f, R21 = 0.0f, R22 = 0.0f;

        Matrix3x3() = default;

        Matrix3x3(
            float r00, float r01, float r02,
            float r10, float r11, float r12,
            float r20, float r21, float r22)
            : R00(r00), R01(r01), R02(r02)
            , R10(r10), R11(r11), R12(r12)
            , R20(r20), R21(r21), R22(r22)
        {}

        /// Единичная матрица
        static Matrix3x3 Identity()
        {
            return Matrix3x3(
                1.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 1.0f
            );
        }

        /// Матрица поворота из углов Эйлера (pitch=X, yaw=Y, roll=Z), радианы.
        /// Порядок применения: Yaw → Pitch → Roll (Y → X → Z)
        static Matrix3x3 FromEuler(float pitch, float yaw, float roll)
        {
            const float cp = std::cos(pitch), sp = std::sin(pitch);
            const float cy = std::cos(yaw),   sy = std::sin(yaw);
            const float cr = std::cos(roll),   sr = std::sin(roll);

            // Y * X * Z
            return Matrix3x3(
                cy*cr + sy*sp*sr,  -cy*sr + sy*sp*cr,  sy*cp,
                cp*sr,              cp*cr,             -sp,
               -sy*cr + cy*sp*sr,   sy*sr + cy*sp*cr,  cy*cp
            );
        }

        /// Инкрементальный поворот вокруг оси X на угол в радианах.
        /// Используется для плавного вращения: rot = rot * RotateX(dt)
        static Matrix3x3 RotateX(float angle)
        {
            const float c = std::cos(angle), s = std::sin(angle);
            return Matrix3x3(
                1.0f, 0.0f, 0.0f,
                0.0f,    c,   -s,
                0.0f,    s,    c
            );
        }

        /// Инкрементальный поворот вокруг оси Y на угол в радианах.
        static Matrix3x3 RotateY(float angle)
        {
            const float c = std::cos(angle), s = std::sin(angle);
            return Matrix3x3(
                   c, 0.0f,    s,
                0.0f, 1.0f, 0.0f,
                  -s, 0.0f,    c
            );
        }

        /// Инкрементальный поворот вокруг оси Z на угол в радианах.
        static Matrix3x3 RotateZ(float angle)
        {
            const float c = std::cos(angle), s = std::sin(angle);
            return Matrix3x3(
                   c,   -s, 0.0f,
                   s,    c, 0.0f,
                0.0f, 0.0f, 1.0f
            );
        }

        /// Умножение матрицы на матрицу
        Matrix3x3 operator*(const Matrix3x3& o) const
        {
            return Matrix3x3(
                R00*o.R00 + R01*o.R10 + R02*o.R20,  R00*o.R01 + R01*o.R11 + R02*o.R21,  R00*o.R02 + R01*o.R12 + R02*o.R22,
                R10*o.R00 + R11*o.R10 + R12*o.R20,  R10*o.R01 + R11*o.R11 + R12*o.R21,  R10*o.R02 + R11*o.R12 + R12*o.R22,
                R20*o.R00 + R21*o.R10 + R22*o.R20,  R20*o.R01 + R21*o.R11 + R22*o.R21,  R20*o.R02 + R21*o.R12 + R22*o.R22
            );
        }

        /// Умножение матрицы на вектор (поворот вектора)
        Vector3 operator*(const Vector3& v) const
        {
            return Vector3(
                R00*v.X + R01*v.Y + R02*v.Z,
                R10*v.X + R11*v.Y + R12*v.Z,
                R20*v.X + R21*v.Y + R22*v.Z
            );
        }

        /// Извлечение углов Эйлера (pitch=X, yaw=Y, roll=Z) в радианах.
        /// Обратная операция к FromEuler(pitch, yaw, roll) с порядком Y → X → Z.
        /// Возвращает Vector3(pitch, yaw, roll).
        /// Внимание: при pitch ≈ ±90° возникает gimbal lock.
        /// Для непрерывного вращения лучше использовать RotateX/Y/Z.
        Vector3 ToEuler() const
        {
            float pitch = std::asin(-R12);

            float yaw, roll;
            if (std::abs(R22) > 1e-6f || std::abs(R02) > 1e-6f)
            {
                yaw  = std::atan2( R02, R22);
                roll = std::atan2( R10, R11);
            }
            else
            {
                yaw  = 0.0f;
                roll = std::atan2(-R01, R00);
            }

            return Vector3(pitch, yaw, roll);
        }

        /// Извлечение углов Эйлера в градусах.
        Vector3 ToEulerDeg() const
        {
            constexpr float RAD2DEG = 180.0f / 3.14159265358979323846f;
            Vector3 e = ToEuler();
            return Vector3(e.X * RAD2DEG, e.Y * RAD2DEG, e.Z * RAD2DEG);
        }
    };

} // namespace Sunover
