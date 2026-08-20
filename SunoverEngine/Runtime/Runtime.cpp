#include "Runtime.h"
#include "../Rendering/RenderBridge.h"

namespace Sunover {

    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration  = std::chrono::duration<float>;

    Runtime::Runtime() = default;

    void Runtime::SetRenderBridge(RenderBridge* bridge)
    {
        m_renderBridge = bridge;
    }

    void Runtime::Start()
    {
        m_running = true;
        RunLoop();
    }

    void Runtime::Stop()
    {
        m_running = false;
    }

    void Runtime::RunLoop()
    {
        TimePoint previousTime = Clock::now();
        float     accumulator  = 0.0f;

        while (m_running)
        {
            // --- Обработка событий окна ---
            // Если окно закрыто — выходим из цикла
            if (m_renderBridge && m_renderBridge->IsInitialized())
            {
                if (!m_renderBridge->PollEvents())
                {
                    m_running = false;
                    break;
                }
            }

            TimePoint currentTime = Clock::now();
            float     deltaTime   = Duration(currentTime - previousTime).count();
            previousTime = currentTime;

            // Защита от слишком большого шага (пауза отладчика и т.д.)
            if (deltaTime > 0.25f) deltaTime = 0.25f;

            // --- RenderBridge::Frame — синхронизация сцены и рендер ---
            if (m_renderBridge && m_renderBridge->IsInitialized())
                m_renderBridge->Frame(deltaTime);

            // --- RenderStepped: пользовательский код после рендера ---
            if (RenderStepped) RenderStepped(deltaTime);

            // --- Фиксированный тик 60/с ---
            accumulator += deltaTime;

            while (accumulator >= FIXED_TIMESTEP)
            {
                if (PreSimulation)  PreSimulation(FIXED_TIMESTEP);
                if (PostSimulation) PostSimulation(FIXED_TIMESTEP);

                accumulator -= FIXED_TIMESTEP;
            }

            // --- Heartbeat: конец кадра ---
            if (Heartbeat) Heartbeat(deltaTime);
        }
    }

} // namespace Sunover
