#pragma once

#include "../PropertyId.h"
#include "../Instance.h"
#include "../PropertyValue.h"
#include "../../Types/Vector3.h"
#include "BasePart.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_SHAPEPART = 3;

    // ShapePart — конкретный тип физического объекта, наследует PropertyId от BasePart.
    // Все свойства (CFrame, Size, Color, и т.д.) берутся из BasePart::
    // и остаются с теми же индексами (0..9) для обратной совместимости.
    struct ShapePart : BasePart
    {
        static constexpr int8_t ClassId = CLASS_SHAPEPART;

        // ShapePart полностью наследует PropertyId из BasePart (0..9).
        // Специфичные для ShapePart свойства начинаются с 10:
        // (пока таких нет — все свойства общие для BasePart)

        // Добавить линейный импульс: прибавляет delta к текущему PosVelocity.
        // PhysicsBridge::SyncIn подхватит новое значение на следующем тике.
        static void ApplyImpulse(Instance& inst, const Vector3& delta)
        {
            Vector3 cur{};
            auto* p = inst.GetProperty(PosVelocity);
            if (p && p->Type == PropertyType::Vector3)
                cur = p->Value.AsVector3;
            inst.SetProperty(PosVelocity,
                PropertyValue::Vector3(Vector3(cur.X + delta.X,
                                               cur.Y + delta.Y,
                                               cur.Z + delta.Z)));
        }

        // Добавить угловой импульс: прибавляет delta к текущему RotVelocity.
        static void ApplyRotationImpulse(Instance& inst, const Vector3& delta)
        {
            Vector3 cur{};
            auto* p = inst.GetProperty(RotVelocity);
            if (p && p->Type == PropertyType::Vector3)
                cur = p->Value.AsVector3;
            inst.SetProperty(RotVelocity,
                PropertyValue::Vector3(Vector3(cur.X + delta.X,
                                               cur.Y + delta.Y,
                                               cur.Z + delta.Z)));
        }
    };

} // namespace Classes
} // namespace Sunvoltum
