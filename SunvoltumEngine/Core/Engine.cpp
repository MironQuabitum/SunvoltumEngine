#include "Engine.h"
#include <iostream>

namespace Sunvoltum {

    Engine::Engine()  = default;
    Engine::~Engine() = default;

    void Engine::Init(EngineMode mode)
    {
        m_mode        = mode;
        m_initialized = true;

        const char* modeStr = nullptr;
        switch (mode)
        {
            case EngineMode::Standalone: modeStr = "Standalone"; break;
            case EngineMode::Client:     modeStr = "Client";     break;
            case EngineMode::Server:     modeStr = "Server";     break;
        }

        std::cout << "[SunvoltumEngine] Init | mode: " << modeStr << std::endl;

        // TODO: инициализация подсистем в зависимости от режима
        //   Standalone — рендер + Luau VM
        //   Client     — рендер + Luau VM + сетевой клиент
        //   Server     — Luau VM + сетевой сервер (без рендера)
    }

    void Engine::PhysicsTick(float fixedDelta)
    {
        if (!m_initialized) return;

        // Клиент не запускает локальную физику — сервер авторитетен.
        // Позиции объектов обновляются через ClientReplicator (PropertyUpdate пакеты).
        // Локальная физика будет включена позже при реализации NetworkOwnership.
        if (m_mode == EngineMode::Client) return;

        // Инициализируем PhysicsBridge при первом тике —
        // к этому моменту DataModel уже заполнен объектами из main().
        if (!Physics.IsInitialized())
            Physics.Init(*this);

        Physics.Step(fixedDelta);
    }

    void Engine::Tick(float deltaTime)
    {
        if (!m_initialized) return;

        // Инициализируем SoundEngine при первом тике —
        // к этому моменту DataModel уже заполнен объектами из main().
        if (!Sound.IsInitialized())
            Sound.Init(*this);

        Sound.Tick(deltaTime);

        // TODO: тик сетевого стека, Luau VM, игровых систем
    }

    void Engine::Shutdown()
    {
        if (!m_initialized) return;

        Physics.Shutdown();
        Sound.Shutdown();

        std::cout << "[SunvoltumEngine] Shutdown" << std::endl;

        // TODO: остановка подсистем, освобождение ресурсов
        m_initialized = false;
    }

    EngineMode Engine::GetMode() const
    {
        return m_mode;
    }

} // namespace Sunvoltum
