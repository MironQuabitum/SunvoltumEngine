#pragma once

#include <functional>
#include <chrono>

#include "../LibSunover.h"
#include "../Input/SunoverInput.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunover {

    class RenderBridge;
    class Engine;

    class LibSunover Runtime
    {
    public:
        Runtime();
        ~Runtime() = default;

        Runtime(const Runtime&)            = delete;
        Runtime& operator=(const Runtime&) = delete;

        // Подключить RenderBridge — вызывать до Start().
        void SetRenderBridge(RenderBridge* bridge);

        // Подключить Engine для физического тика — вызывать до Start().
        void SetEngine(Engine* engine);

        // Запустить игровой цикл. Блокирует поток до закрытия окна или Stop().
        void Start();

        // Остановить игровой цикл
        void Stop();

        // -----------------------------------------------------------
        // Ввод — доступен в любом событии
        // -----------------------------------------------------------
        SunoverInput Input;

        // -----------------------------------------------------------
        // События
        // -----------------------------------------------------------

        // Начало кадра. Каждый кадр, частота не ограничена.
        std::function<void(float deltaTime)> RenderStepped;

        // Перед симуляцией физики. Фиксированный тик 60/с.
        std::function<void(float fixedDelta)> PreSimulation;

        // После симуляции физики. Фиксированный тик 60/с.
        std::function<void(float fixedDelta)> PostSimulation;

        // Конец кадра. Вызывается один раз за кадр.
        std::function<void(float deltaTime)> Heartbeat;

    private:
        bool          m_running      = false;
        RenderBridge* m_renderBridge = nullptr;
        Engine*       m_engine       = nullptr;

        // -----------------------------------------------------------
        // Follow-камера — внутреннее состояние орбиты
        // -----------------------------------------------------------
        float m_followYaw    =  0.0f;   // горизонтальный угол орбиты (рад)
        float m_followPitch  =  0.3f;   // вертикальный угол орбиты (рад)
        float m_followRadius = 15.0f;   // текущее расстояние от субъекта (стадов)

        // Позиция курсора движка в момент нажатия ПКМ (до лока).
        // При отпускании ПКМ системный курсор телепортируется сюда.
        int m_preLockMouseX = 0;
        int m_preLockMouseY = 0;

        // true пока движок удерживает лок мыши из-за first-person режима (radius == 0).
        // Нужно чтобы при выходе из first-person корректно снять именно этот лок.
        bool m_firstPersonLocked = false;

        static constexpr float FOLLOW_SENS        = 0.002618f; // 0.15 deg/px в рад
        static constexpr float FOLLOW_PITCH_MIN   = -1.48f;    // ~-85°
        static constexpr float FOLLOW_PITCH_MAX   =  1.48f;    // ~+85°
        static constexpr float FOLLOW_WHEEL_SPEED =  2.0f;     // стадов на клик колёса
        // Fallback-пределы зума если в камере не выставлены Min/MaxZoomDistance
        static constexpr float FOLLOW_RADIUS_MIN_DEFAULT =  0.0f;
        static constexpr float FOLLOW_RADIUS_MAX_DEFAULT = 60.0f;

        // Обновляет CFrame камеры если она в режиме Follow
        void UpdateFollowCamera();

        static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;

        void RunLoop();
    };

} // namespace Sunover

#pragma warning(pop)
