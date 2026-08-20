#pragma once

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
    };

} // namespace Sunover
