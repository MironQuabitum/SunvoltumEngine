#pragma once

#include "../PropertyId.h"

// Диапазон ClassId для JointInstance и потомков: 20..29
//   20 = JointInstance  (базовый, не создаётся напрямую)
//   21 = Weld
//   22 = зарезервировано (WeldConstraint)
//   23 = Motor6D        (объявлен, реализация — Motor6D.h, добавить позже)
//   24..29 = зарезервировано

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_JOINT   = 20; // базовый, не создаётся через Instance.new
    constexpr int8_t CLASS_WELD    = 21;
    // constexpr int8_t CLASS_WELDCONSTRAINT = 22;  — добавить позже
    constexpr int8_t CLASS_MOTOR6D = 23; // объявлен; добавить Motor6D.h и раскомментировать в IsJoint() когда готов
    // 24..29 зарезервировано

    // -----------------------------------------------------------------------
    // IsJoint — проверяет, является ли classId суставом.
    //
    // Используется в:
    //   PhysicsBridge::RegisterBodyRecursive — маршрутизация в RegisterJoint
    //   SharedBindings::TryGetProperty / TrySetProperty — общие свойства 0..4
    //   ServerReplicator::IsFrequentProperty — все joint-свойства → reliable
    //
    // Чтобы добавить новый тип сустава: раскомментируйте строку ниже
    // и создайте Motor6D.h по образцу Weld.h.
    // -----------------------------------------------------------------------
    inline bool IsJoint(int8_t classId)
    {
        return classId == CLASS_WELD
            || classId == CLASS_MOTOR6D
            ;
    }

    // -----------------------------------------------------------------------
    // JointInstance — общие PropertyId для всех суставов.
    //
    // Все подтипы (Weld, Motor6D) используют эти же индексы (0..4)
    // для одинаковых свойств — совместимость сериализации сохраняется.
    // Специфичные свойства подтипов начинаются с 5.
    // -----------------------------------------------------------------------
    struct JointInstance
    {
        // Опорная часть — от неё считается трансформация
        static constexpr PropertyId Part0   = 0; // InstanceRef

        // Ведомая часть — её CFrame вычисляется через формулу Weld
        static constexpr PropertyId Part1   = 1; // InstanceRef

        // Смещение точки соединения в пространстве Part0
        static constexpr PropertyId C0      = 2; // CFrame

        // Смещение точки соединения в пространстве Part1
        static constexpr PropertyId C1      = 3; // CFrame

        // Активно ли соединение (false → PhysicsBridge не применяет формулу)
        static constexpr PropertyId Enabled = 4; // Bool
    };

} // namespace Classes
} // namespace Sunvoltum
