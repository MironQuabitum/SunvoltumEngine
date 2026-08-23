#include "Runtime.h"
#include "../Rendering/RenderBridge.h"
#include "../Core/Engine.h"
#include <MeturmRender/Core/Window.h>

namespace Sunover {

    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration  = std::chrono::duration<float>;

    Runtime::Runtime() = default;

    void Runtime::SetRenderBridge(RenderBridge* bridge)
    {
        m_renderBridge = bridge;

        // Подключаем Input источник из окна RenderBridge
        if (bridge && bridge->IsInitialized())
            Input.SetSource(&bridge->GetWindow().GetInput());
    }

    void Runtime::SetEngine(Engine* engine)
    {
        m_engine = engine;
    }

    void Runtime::Start()
    {
        // Если SetRenderBridge вызван до Init окна — подключаем Input здесь
        if (m_renderBridge && m_renderBridge->IsInitialized() && !Input.IsValid())
            Input.SetSource(&m_renderBridge->GetWindow().GetInput());

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
            if (m_renderBridge && m_renderBridge->IsInitialized())
            {
                // Input::Update() — сбрасывает per-frame дельты до PollEvents
                m_renderBridge->GetWindow().GetInput().Update();

                if (!m_renderBridge->PollEvents())
                {
                    m_running = false;
                    break;
                }
            }

            TimePoint currentTime = Clock::now();
            float     deltaTime   = Duration(currentTime - previousTime).count();
            previousTime = currentTime;

            if (deltaTime > 0.25f) deltaTime = 0.25f;

            if (m_renderBridge && m_renderBridge->IsInitialized())
                m_renderBridge->Frame(deltaTime);

            if (RenderStepped) RenderStepped(deltaTime);

            accumulator += deltaTime;
            while (accumulator >= FIXED_TIMESTEP)
            {
                if (PreSimulation)  PreSimulation(FIXED_TIMESTEP);
                // Физический тик движка — между Pre и PostSimulation
                if (m_engine) m_engine->PhysicsTick(FIXED_TIMESTEP);
                if (PostSimulation) PostSimulation(FIXED_TIMESTEP);
                accumulator -= FIXED_TIMESTEP;
            }

            if (Heartbeat) Heartbeat(deltaTime);
        }
    }

} // namespace Sunover
