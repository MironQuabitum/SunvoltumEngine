#pragma once

#include <functional>
#include <chrono>

#include "../LibSunover.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunover {

    class RenderBridge;

    class LibSunover Runtime
    {
    public:
        Runtime();
        ~Runtime() = default;

        Runtime(const Runtime&)            = delete;
        Runtime& operator=(const Runtime&) = delete;

        // Подключить RenderBridge — вызывать до Start().
        // Только для режимов Standalone и Client.
        void SetRenderBridge(RenderBridge* bridge);

        // Запустить игровой цикл. Блокирует поток до закрытия окна или Stop().
        void Start();

        // Остановить игровой цикл
        void Stop();

        // -----------------------------------------------------------
        // События
        // -----------------------------------------------------------

        // Начало кадра. Каждый кадр, частота не ограничена.
        // Вызывается ПОСЛЕ RenderBridge::Frame() — рендер уже выполнен.
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

        static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;

        void RunLoop();
    };

} // namespace Sunover

#pragma warning(pop)
