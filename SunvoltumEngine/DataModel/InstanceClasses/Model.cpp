#include "Model.h"
#include "ShapePart.h"
#include "../PropertyValue.h"
#include "../../Types/CFrame.h"
#include "../../Types/Matrix3x3.h"

#include <cmath>

namespace Sunvoltum {
namespace Classes {

// ---------------------------------------------------------------------------
//  Вспомогательные функции
// ---------------------------------------------------------------------------

// Возвращает CFrame дочернего ShapePart или нулевой если свойства нет.
static Sunvoltum::CFrame GetPartCFrame(const Instance& part)
{
    const PropertyValue* pv = part.GetProperty(ShapePart::CFrame);
    if (pv && pv->Type == PropertyType::CFrame)
        return pv->Value.AsCFrame;
    return Sunvoltum::CFrame::FromPosition(0.0f, 0.0f, 0.0f);
}

// Устанавливает CFrame дочернего ShapePart.
static void SetPartCFrame(Instance& part, const Sunvoltum::CFrame& cf)
{
    part.SetProperty(ShapePart::CFrame, PropertyValue::CFrame(cf));
}

// Вычисляет «pivot» модели — CFrame из которого будет считаться смещение.
// Если PrimaryPart задан — возвращает его CFrame.
// Иначе берём позицию первого встреченного ShapePart среди прямых детей.
// Если детей-ShapePart нет — возвращает identity в (0,0,0).
static Sunvoltum::CFrame GetModelPivot(const Instance& model)
{
    // Приоритет: PrimaryPart
    const PropertyValue* pv = model.GetProperty(Model::PrimaryPart);
    if (pv && pv->Type == PropertyType::InstanceRef && pv->Value.AsInstanceRef)
    {
        const Instance* pp = pv->Value.AsInstanceRef;
        if (pp->GetClassId() == ShapePart::ClassId)
            return GetPartCFrame(*pp);
    }

    // Fallback: первый ShapePart среди детей
    for (const auto& child : model.GetChildren())
    {
        if (child->GetClassId() == ShapePart::ClassId)
            return GetPartCFrame(*child);
    }

    return Sunvoltum::CFrame::FromPosition(0.0f, 0.0f, 0.0f);
}

// ---------------------------------------------------------------------------
//  Вычисление инверсии CFrame
//  inv(cf) такое что cf * inv(cf) = Identity
//  inv.Rotation = transpose(cf.Rotation)
//  inv.Position = -inv.Rotation * cf.Position
// ---------------------------------------------------------------------------
static Sunvoltum::CFrame InvertCFrame(const Sunvoltum::CFrame& cf)
{
    // Инверсия ортогональной матрицы вращения = транспонирование
    const Matrix3x3& r = cf.Rotation;
    Matrix3x3 invRot(
        r.R00, r.R10, r.R20,
        r.R01, r.R11, r.R21,
        r.R02, r.R12, r.R22
    );

    // Инверсия позиции: -R^T * P
    Vector3 invPos = invRot * Vector3(-cf.Position.X, -cf.Position.Y, -cf.Position.Z);

    return Sunvoltum::CFrame(invPos, invRot);
}

// ---------------------------------------------------------------------------
//  Перемещение модели со смещением deltaWorld (world-space)
//  Для каждого прямого дочернего ShapePart применяем: newCF = target * invPivot * partCF
// ---------------------------------------------------------------------------
static void MoveModelChildren(Instance& model, const Sunvoltum::CFrame& pivotCF,
                               const Sunvoltum::CFrame& targetCF)
{
    // Трансформация из старого pivot-пространства в новое:
    // newPartCF = targetCF * invPivotCF * oldPartCF
    Sunvoltum::CFrame invPivot = InvertCFrame(pivotCF);
    Sunvoltum::CFrame delta    = targetCF * invPivot; // итоговое мировое смещение

    for (const auto& child : model.GetChildren())
    {
        if (child->GetClassId() == ShapePart::ClassId)
        {
            Sunvoltum::CFrame oldCF = GetPartCFrame(*child);
            Sunvoltum::CFrame newCF = delta * oldCF;
            SetPartCFrame(*child, newCF);
        }
    }
}

// ---------------------------------------------------------------------------
//  PivotTo
// ---------------------------------------------------------------------------
void Model::PivotTo(Instance& model, const CFrame& targetCFrame)
{
    Sunvoltum::CFrame pivot = GetModelPivot(model);
    MoveModelChildren(model, pivot, targetCFrame);
}

// ---------------------------------------------------------------------------
//  SetPrimaryPartCFrame
// ---------------------------------------------------------------------------
void Model::SetPrimaryPartCFrame(Instance& model, const CFrame& targetCFrame)
{
    // Требуем PrimaryPart
    Instance* pp = GetPrimaryPart(model);
    if (!pp || pp->GetClassId() != ShapePart::ClassId)
        return;

    Sunvoltum::CFrame pivotCF = GetPartCFrame(*pp);
    MoveModelChildren(model, pivotCF, targetCFrame);
}

} // namespace Classes
} // namespace Sunvoltum
