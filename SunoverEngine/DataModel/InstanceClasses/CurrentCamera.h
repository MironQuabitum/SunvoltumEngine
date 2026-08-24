#pragma once

#include "../PropertyId.h"

namespace Sunover {
namespace Classes {

    constexpr int8_t CLASS_CURRENTCAMERA = 2;

    struct CurrentCamera
    {
        static constexpr int8_t ClassId = CLASS_CURRENTCAMERA;

        // Позиция и ориентация камеры в пространстве
        static constexpr PropertyId CFrame       = 0; // CFrame

        // Точка, на которую смотрит камера
        static constexpr PropertyId Focus        = 1; // CFrame

        // Угол обзора в градусах (вертикальный FOV)
        static constexpr PropertyId FieldOfView  = 2; // Number

        // Режим поведения камеры
        static constexpr PropertyId CameraMode    = 3; // CameraType

        // Объект, вокруг которого вращается орбитальная (Follow) камера.
        // Тип PropertyType::InstanceRef — слабый указатель на Instance, не владеет.
        static constexpr PropertyId CameraSubject  = 4; // InstanceRef (Instance*)

        // Минимальное расстояние зума (радиус орбиты). 0 = first-person.
        static constexpr PropertyId MinZoomDistance = 5; // Number

        // Максимальное расстояние зума (радиус орбиты).
        static constexpr PropertyId MaxZoomDistance = 6; // Number
    };

} // namespace Classes
} // namespace Sunover
