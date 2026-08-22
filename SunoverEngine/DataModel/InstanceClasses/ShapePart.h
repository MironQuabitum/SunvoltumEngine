#pragma once

#include "../PropertyId.h"

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
    };

} // namespace Classes
} // namespace Sunover
