#pragma once

#include "../PropertyId.h"
#include "../Instance.h"
#include "../PropertyValue.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_HUMANOID = 30;

    // -----------------------------------------------------------------------
    // Humanoid — физический контроллер персонажа, аналог Roblox Humanoid.
    //
    // Добавляется как потомок модели Character (например "Character1").
    // Требует наличия BasePart с именем "HumanoidRootPart" среди братьев —
    // PhysicsBridge автоматически вызывает LockUpright() на этот Part при
    // обнаружении Humanoid в родительской Model.
    //
    // Свойства:
    //   0 = Health        (Float) — текущее здоровье. 0 = смерть.
    //   1 = MaxHealth     (Float) — максимальное здоровье.
    //   2 = WalkSpeed     (Float) — скорость ходьбы (стадов/с).
    //   3 = JumpPower     (Float) — начальная вертикальная скорость при прыжке.
    //   4 = MoveDirection (Vector3) — нормализованный вектор движения в мировом пространстве.
    //                                 Задаётся контроллером (сервером или клиентом).
    //                                 PhysicsBridge применяет его к HRP каждый тик.
    //   5 = HipHeight     (Float) — высота бёдер над полом (используется для расчёта
    //                               нижней точки raycast проверки земли).
    //   6 = State         (Int)   — текущее состояние: 0=Idle, 1=Walking, 2=Jumping, 3=Falling, 4=Dead.
    //   7 = Jump          (Bool)  — выставить true чтобы инициировать прыжок.
    //                               Humanoid-система сбрасывает в false после применения.
    //
    // Пример (Lua):
    //   local humanoid = Instance.new("Humanoid", charModel)
    //   humanoid.WalkSpeed = 16
    //   humanoid.JumpPower = 50
    //   humanoid.MoveDirection = { X = 0, Y = 0, Z = -1 }  -- движение вперёд
    // -----------------------------------------------------------------------
    struct Humanoid
    {
        static constexpr int8_t ClassId = CLASS_HUMANOID;

        // Текущее здоровье персонажа. При достижении 0 — State=Dead.
        static constexpr PropertyId Health        = 0; // Float

        // Максимальное здоровье.
        static constexpr PropertyId MaxHealth     = 1; // Float

        // Скорость горизонтального движения (стадов/с).
        // Roblox по умолчанию: 16.
        static constexpr PropertyId WalkSpeed     = 2; // Float

        // Вертикальная скорость, придаваемая HRP при прыжке (стадов/с).
        // Roblox по умолчанию: 50.
        static constexpr PropertyId JumpPower     = 3; // Float

        // Нормализованный вектор желаемого направления движения (мировое пространство).
        // X/Z — горизонталь; Y компонента игнорируется при применении ходьбы.
        // Контроллер персонажа пишет сюда каждый кадр.
        static constexpr PropertyId MoveDirection = 4; // Vector3

        // Высота точки опоры над полом (нижней грани HRP) для raycast.
        // Roblox R6 по умолчанию: 0.1 (почти у нижней грани HRP).
        static constexpr PropertyId HipHeight     = 5; // Float

        // Текущее состояние Humanoid (HumanoidStateType).
        // 0 = Idle, 1 = Walking, 2 = Jumping, 3 = Falling, 4 = Dead.
        static constexpr PropertyId State         = 6; // Int

        // Триггер прыжка. Контроллер пишет true → PhysicsBridge применяет импульс → сбрасывает в false.
        static constexpr PropertyId Jump          = 7; // Bool

        // Текущий угол поворота тела вокруг Y (радианы, мировое пространство).
        // PhysicsBridge обновляет плавно в сторону MoveDirection каждый тик.
        // Контроллер может читать для синхронизации визуального поворота.
        // Записывать напрямую не нужно — управляется движком автоматически.
        static constexpr PropertyId FacingYaw     = 8; // Float

        // ---- Состояния (HumanoidStateType) ----
        static constexpr int32_t STATE_IDLE    = 0;
        static constexpr int32_t STATE_WALKING = 1;
        static constexpr int32_t STATE_JUMPING = 2;
        static constexpr int32_t STATE_FALLING = 3;
        static constexpr int32_t STATE_DEAD    = 4;

        // ---- Init ----

        // Инициализация свойств нового Humanoid.
        // Вызывается из SharedBindings::Instance.new("Humanoid") через initFn.
        static void Init(Instance& inst)
        {
            inst.SetProperty(Health,        PropertyValue::Float(100.0f));
            inst.SetProperty(MaxHealth,     PropertyValue::Float(100.0f));
            inst.SetProperty(WalkSpeed,     PropertyValue::Float(16.0f));
            inst.SetProperty(JumpPower,     PropertyValue::Float(50.0f));
            inst.SetProperty(MoveDirection, PropertyValue::Vector3(Vector3(0.0f, 0.0f, 0.0f)));
            inst.SetProperty(HipHeight,     PropertyValue::Float(0.1f));
            inst.SetProperty(State,         PropertyValue::Int(STATE_IDLE));
            inst.SetProperty(Jump,          PropertyValue::Bool(false));
            inst.SetProperty(FacingYaw,     PropertyValue::Float(0.0f));
        }
    };

} // namespace Classes
} // namespace Sunvoltum
