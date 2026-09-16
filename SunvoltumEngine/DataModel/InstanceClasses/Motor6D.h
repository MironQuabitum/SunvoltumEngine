#pragma once

#include "JointInstance.h"
#include "../Instance.h"
#include "../PropertyValue.h"
#include "../../Types/CFrame.h"

namespace Sunvoltum {
namespace Classes {

    // -----------------------------------------------------------------------
    // Motor6D — вращательный сустав с приводом, аналог Roblox Motor6D.
    //
    // Наследует от JointInstance общие свойства (индексы 0..4):
    //   0 = Part0    (InstanceRef)
    //   1 = Part1    (InstanceRef)
    //   2 = C0       (CFrame) — attachment offset в пространстве Part0
    //   3 = C1       (CFrame) — attachment offset в пространстве Part1
    //   4 = Enabled  (Bool)
    //
    // Специфичные свойства (индексы 5..7):
    //   5 = DesiredAngle  (Float, радианы) — целевой угол вращения вокруг оси
    //   6 = MaxVelocity   (Float, рад/с)   — скорость привода к DesiredAngle
    //   7 = CurrentAngle  (Float, радианы) — текущий угол (readonly, пишет PhysicsBridge)
    //
    // Физика (PhysicsBridge):
    //   Реализован через PxD6Joint с одной свободной осью вращения (eTWIST = ось X
    //   local frame). PhysX DriveVelocity применяется каждый тик в SyncJoints
    //   чтобы двигать сустав от CurrentAngle к DesiredAngle со скоростью MaxVelocity.
    //
    // Клиент (ClientReplicator):
    //   PropagateJoint для Motor6D учитывает CurrentAngle: вместо фиксированного C1
    //   вычисляется дополнительный поворот вокруг оси X на CurrentAngle.
    // -----------------------------------------------------------------------
    struct Motor6D : JointInstance
    {
        static constexpr int8_t ClassId = CLASS_MOTOR6D; // 23

        // ---- Специфичные PropertyId (начинаются с 5, не пересекаются с JointInstance) ----

        // Целевой угол вращения вокруг оси X local-frame (радианы).
        // Аналог Roblox Motor6D.DesiredAngle.
        static constexpr PropertyId DesiredAngle = 5; // Float

        // Максимальная угловая скорость привода (рад/с).
        // При MaxVelocity = 0 мотор не вращается.
        // Аналог Roblox Motor6D.MaxVelocity.
        static constexpr PropertyId MaxVelocity  = 6; // Float

        // Текущий угол (рад). Только для чтения из Luau.
        // PhysicsBridge записывает его из PxD6Joint::getTwistAngle() каждый тик.
        // Аналог Roblox Motor6D.CurrentAngle.
        static constexpr PropertyId CurrentAngle = 7; // Float

        // ---- Init ----

        // Инициализация дефолтных свойств нового Motor6D-инстанса.
        // Вызывается из SharedBindings::GetJointInitFn при Instance.new("Motor6D").
        static void Init(Instance& inst)
        {
            // Общие JointInstance свойства
            inst.SetProperty(C0,      PropertyValue::CFrame(CFrame::FromPosition(0.0f, 0.0f, 0.0f)));
            inst.SetProperty(C1,      PropertyValue::CFrame(CFrame::FromPosition(0.0f, 0.0f, 0.0f)));
            inst.SetProperty(Enabled, PropertyValue::Bool(true));
            inst.SetProperty(Part0,   PropertyValue::Ref(nullptr));
            inst.SetProperty(Part1,   PropertyValue::Ref(nullptr));

            // Специфичные Motor6D свойства
            inst.SetProperty(DesiredAngle, PropertyValue::Float(0.0f));
            inst.SetProperty(MaxVelocity,  PropertyValue::Float(0.0f)); // мотор стоит до явного задания
            inst.SetProperty(CurrentAngle, PropertyValue::Float(0.0f));
        }
    };

} // namespace Classes
} // namespace Sunvoltum
