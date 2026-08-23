#pragma once

#include "../PropertyId.h"
#include "../Instance.h"
#include "../PropertyValue.h"
#include "../../Types/Vector3.h"

namespace Sunover {
namespace Classes {

    constexpr int8_t CLASS_SHAPEPART = 3;

    struct ShapePart
    {
        static constexpr int8_t ClassId = CLASS_SHAPEPART;

        // Позиция и ориентация объекта в пространстве
        static constexpr PropertyId CFrame       = 0; // CFrame

        // Угловая скорость (Vector3, рад/с)
        static constexpr PropertyId RotVelocity  = 1; // Vector3

        // Линейная скорость (Vector3, м/с)
        static constexpr PropertyId PosVelocity  = 2; // Vector3

        // Прозрачность: 0.0 = непрозрачный, 1.0 = невидимый
        static constexpr PropertyId Transparency = 3; // Float

        // Отражение: 0.0 = матовый, 1.0 = зеркальный
        static constexpr PropertyId Reflectance  = 4; // Float

        // Цвет объекта
        static constexpr PropertyId Color        = 5; // Color3

        // Форма объекта (Ball, Block, Cylinder)
        static constexpr PropertyId Shape        = 6; // Shape

        // Размер объекта по трём осям (Vector3, в стадах)
        static constexpr PropertyId Size         = 7; // Vector3

        // Закреплён ли объект в пространстве.
        // true  — PhysX не применяет к нему гравитацию и динамику (static/kinematic actor).
        // false — объект участвует в полноценной симуляции (dynamic actor).
        static constexpr PropertyId Anchored     = 8; // Bool

        // Участвует ли объект в коллизиях.
        // true  — shape имеет флаг eSIMULATION_SHAPE (нормальное столкновение).
        // false — флаг eSIMULATION_SHAPE снят: тело продолжает симулироваться
        //         (падает под гравитацией если не Anchored), но ни с чем не сталкивается.
        static constexpr PropertyId CanCollide   = 9; // Bool

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
} // namespace Sunover
