#pragma once

#include "JointInstance.h"
#include "../Instance.h"
#include "../PropertyValue.h"
#include "../../Types/CFrame.h"

namespace Sunvoltum {
namespace Classes {

    // -----------------------------------------------------------------------
    // Weld — жёсткое соединение двух BasePart.
    //
    // Формула (применяется в PhysicsBridge::SyncJoints каждый тик):
    //   Part1.CFrame = Part0.CFrame * C0 * CFrameInverse(C1)
    //
    // C0 — смещение точки соединения в локальном пространстве Part0.
    // C1 — смещение точки соединения в локальном пространстве Part1.
    //
    // При создании через Instance.new("Weld") движок устанавливает
    // дефолтные C0/C1 = Identity, что означает Part1 совпадает с Part0
    // (или позже задаются из Luau).
    //
    // Weld наследует все PropertyId от JointInstance:
    //   0 = Part0, 1 = Part1, 2 = C0, 3 = C1, 4 = Enabled
    // -----------------------------------------------------------------------
    struct Weld : JointInstance
    {
        static constexpr int8_t ClassId = CLASS_WELD;

        // Инициализация дефолтных свойств нового Weld-инстанса.
        // Вызывается из SharedBindings::Instance_New при создании через Instance.new.
        static void Init(Instance& inst)
        {
            // C0 = Identity (без смещения)
            inst.SetProperty(C0,
                PropertyValue::CFrame(Sunvoltum::CFrame::FromPosition(0.0f, 0.0f, 0.0f)));

            // C1 = Identity
            inst.SetProperty(C1,
                PropertyValue::CFrame(Sunvoltum::CFrame::FromPosition(0.0f, 0.0f, 0.0f)));

            // Enabled = true по умолчанию
            inst.SetProperty(Enabled, PropertyValue::Bool(true));

            // Part0 / Part1 = nullptr (не задано)
            inst.SetProperty(Part0, PropertyValue::Ref(nullptr));
            inst.SetProperty(Part1, PropertyValue::Ref(nullptr));
        }
    };

} // namespace Classes
} // namespace Sunvoltum
