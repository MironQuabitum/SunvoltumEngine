#pragma once

#include "CFrame.h"
#include "Matrix3x3.h"
#include "Vector3.h"

namespace Sunvoltum {

    // -----------------------------------------------------------------------
    // CFrameInverse — обратный CFrame для ортонормальных матриц поворота.
    //
    // Для ортогональной матрицы R: R^-1 = R^T (транспозиция).
    // Обратная позиция: p_inv = R^T * (-p)
    //
    // Используется в Weld для расчёта:
    //   Part1.CFrame = Part0.CFrame * C0 * CFrameInverse(C1)
    // И при вычислении дефолтных C0/C1:
    //   C0 = CFrameInverse(Part0.CFrame) * Part1.CFrame
    //   C1 = Identity (т.е. CFrame::FromPosition(0,0,0))
    // -----------------------------------------------------------------------
    inline CFrame CFrameInverse(const CFrame& cf)
    {
        // Транспонирование ортогональной матрицы = её обратная
        const Matrix3x3& r = cf.Rotation;
        Matrix3x3 invR(
            r.R00, r.R10, r.R20,
            r.R01, r.R11, r.R21,
            r.R02, r.R12, r.R22
        );
        // Обратная позиция: invR * (-p)
        Vector3 invP = invR * Vector3(-cf.Position.X, -cf.Position.Y, -cf.Position.Z);
        return CFrame(invP, invR);
    }

} // namespace Sunvoltum
