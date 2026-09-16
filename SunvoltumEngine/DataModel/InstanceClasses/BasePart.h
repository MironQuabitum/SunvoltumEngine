#pragma once

#include "../PropertyId.h"

// Диапазоны ClassId смотри в BasePlan.txt
// ShapePart = 3
// Будущие: MeshPart = 51, TrussPart = 52, ...

namespace Sunvoltum {
namespace Classes {

    // -----------------------------------------------------------------------
    // Константа ClassId для ShapePart уже объявлена в ShapePart.h.
    // Здесь объявляем только зарезервированные будущие ClassId.
    // -----------------------------------------------------------------------
    // constexpr int8_t CLASS_MESHPART  = 51;  — добавить когда появится
    // constexpr int8_t CLASS_TRUSSPART = 52;  — добавить когда появится

    // -----------------------------------------------------------------------
    // IsBasePart — проверяет, является ли classId физическим объектом-частью.
    //
    // Все три системы (PhysicsBridge, RenderBridge, SharedBindings) должны
    // использовать эту функцию вместо жёсткой проверки CLASS_SHAPEPART,
    // чтобы любой новый подтип автоматически получал физику, рендер и биндинги.
    // -----------------------------------------------------------------------
    inline bool IsBasePart(int8_t classId)
    {
        return classId == 3   // CLASS_SHAPEPART
            // || classId == CLASS_MESHPART   — раскомментировать когда появится
            // || classId == CLASS_TRUSSPART  — раскомментировать когда появится
            ;
    }

    // -----------------------------------------------------------------------
    // BasePart — общие PropertyId для всех физических частей.
    //
    // ShapePart использует те же индексы (0..9) — полная совместимость.
    // Новые подтипы должны использовать эти же индексы для одинаковых свойств,
    // а специфичные свойства начинать с 10.
    // -----------------------------------------------------------------------
    struct BasePart
    {
        // Позиция и ориентация объекта в мировом пространстве
        static constexpr PropertyId CFrame       = 0; // CFrame

        // Угловая скорость (Vector3, рад/с)
        static constexpr PropertyId RotVelocity  = 1; // Vector3

        // Линейная скорость (Vector3, м/с)
        static constexpr PropertyId PosVelocity  = 2; // Vector3

        // Прозрачность: 0.0 = непрозрачный, 1.0 = невидимый
        static constexpr PropertyId Transparency = 3; // Float

        // Отражение: 0.0 = матовый, 1.0 = зеркальный
        static constexpr PropertyId Reflectance  = 4; // Float

        // Цвет объекта (Color3)
        static constexpr PropertyId Color        = 5; // Color3

        // Тип геометрии (Ball, Block, Cylinder)
        static constexpr PropertyId Shape        = 6; // Shape

        // Размер по трём осям (Vector3, в стадах)
        static constexpr PropertyId Size         = 7; // Vector3

        // Закреплён ли объект (true = static/kinematic в PhysX, нет гравитации)
        static constexpr PropertyId Anchored     = 8; // Bool

        // Участвует ли в коллизиях
        static constexpr PropertyId CanCollide   = 9; // Bool

        // Будущие свойства начинаются с 10:
        // static constexpr PropertyId Material   = 10;
        // static constexpr PropertyId Elasticity = 11;
        // static constexpr PropertyId Friction   = 12;
        // static constexpr PropertyId Mass       = 13;
    };

} // namespace Classes
} // namespace Sunvoltum
