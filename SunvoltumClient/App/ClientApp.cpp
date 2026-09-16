#include "ClientApp.h"
#include "Environment/ClientSceneInitializer.h"
#include "Scripting/ClientSide/ClientScriptBridge.h"
#include <iostream>

namespace Sunvoltum {
namespace Client {

    ClientApp::ClientApp() = default;
    ClientApp::~ClientApp() = default;

    int ClientApp::Run()
    {
        std::cout << "[SunvoltumClient] Starting in Client mode...\n";

        m_engine.Init(EngineMode::Client);
        auto& dm = m_engine.DataModel;

        // Инициализация сетевого контроллера
        m_networkController.Init();
        m_networkController.Connect("127.0.0.1", 7777, "Player1");

        // Инициализация базового окружения и камеры
        ClientSceneInitializer::InitEnvironment(dm);
        Instance& camera = m_cameraController.SetupCamera(dm);

        // Инициализация клиентского скрипт-моста
        Scripting::Client::ClientScriptBridge::Get().Init(&dm);

        // Настройка десериализации мира и авто-привязки камеры к персонажу
        m_networkController.SetupSceneSynchronization(dm, [this, &camera, &dm]()
        {
            m_cameraController.AttachToCharacter(camera, dm);
        });

        // Инициализация окна и рендер-бриджа
        if (!m_renderBridge.Init(m_engine, 1280, 720, "Sunvoltum — Client"))
            return -1;

        SunvoltumInput& input = m_renderBridge.GetInput();
        input.SetSystemCursorVisible(false);
        input.SetEngineCursorVisible(true);

        Scripting::Client::ClientScriptBridge::Get().SetInputSource(&input);

        // Настройка Runtime
        m_runtime.SetRenderBridge(&m_renderBridge);
        m_runtime.SetInputSource(&input);
        m_runtime.SetEngine(&m_engine);

        Scripting::Client::ClientScriptBridge::Get().WatchWorkspace();

        m_runtime.RenderStepped = [this, &input](float dt)
        {
            if (input.IsKeyPressed(KeyCode::Escape))
                m_runtime.Stop();
            Scripting::Client::ClientScriptBridge::Get().FireRenderStepped(static_cast<double>(dt));
        };

        m_runtime.PreSimulation = [](float) {};

        m_runtime.Heartbeat = [this](float dt)
        {
            m_networkController.Poll();
            m_engine.Tick(dt);

            static double s_time = 0.0;
            s_time += static_cast<double>(dt);
            Scripting::Client::ClientScriptBridge::Get().StepScheduler(s_time);
            Scripting::Client::ClientScriptBridge::Get().FireHeartbeat(static_cast<double>(dt));
        };

        // Главный цикл
        m_runtime.Start();

        // Завершение работы
        m_renderBridge.Shutdown();
        m_engine.Shutdown();
        Scripting::Client::ClientScriptBridge::Get().Shutdown();
        m_networkController.Shutdown();

        std::cout << "[SunvoltumClient] Shutdown complete.\n";
        return 0;
    }

} // namespace Client
} // namespace Sunvoltum
