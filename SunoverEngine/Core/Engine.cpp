#include "Engine.h"
#include <iostream>

namespace Sunover {

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

        std::cout << "[SunoverEngine] Init | mode: " << modeStr << std::endl;

        // TODO: инициализация подсистем в зависимости от режима
        //   Standalone — рендер + Luau VM
        //   Client     — рендер + Luau VM + сетевой клиент
        //   Server     — Luau VM + сетевой сервер (без рендера)
    }

    void Engine::Tick(float deltaTime)
    {
        if (!m_initialized) return;

        // TODO: тик сетевого стека, Luau VM, игровых систем
    }

    void Engine::Shutdown()
    {
        if (!m_initialized) return;

        std::cout << "[SunoverEngine] Shutdown" << std::endl;

        // TODO: остановка подсистем, освобождение ресурсов
        m_initialized = false;
    }

    EngineMode Engine::GetMode() const
    {
        return m_mode;
    }

} // namespace Sunover
