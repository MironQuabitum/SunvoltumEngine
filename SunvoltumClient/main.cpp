#include "SunvoltumEngine.h"
#include "Rendering/RenderBridge.h"
#include "DataModel/InstanceClasses/CurrentCamera.h"
#include "DataModel/InstanceClasses/Workspace.h"
#include "DataModel/InstanceClasses/Lighting.h"
#include "Scripting/ServerSide/ServerScriptBridge.h"
#include "Network/NetworkManager.h"
#include "Network/NetworkClient.h"
#include "Network/SceneDeserializer.h"
#include "Network/ClientReplicator.h"
#include <iostream>

using namespace Sunvoltum;
using namespace Sunvoltum::Classes;
using namespace Sunvoltum::Scripting::Server;
using namespace Sunvoltum::Net;

int main()
{
    std::cout << "[SunvoltumClient] Starting in Client mode...\n";

    Engine engine;
    engine.Init(EngineMode::Client);

    // --- Сеть ---
    NetworkManager::Get().Init(EngineMode::Client);

    NetworkClient::Get().SetOnConnected([&](NetworkId myId)
    {
        std::cout << "[SunvoltumClient] Connected to server! NetworkId=" << myId << "\n";
        // Сериализация начнётся автоматически: сервер пришлёт StartSerialization,
        // SceneDeserializer ответит ReadySerialization и начнёт принимать объекты.
    });

    NetworkClient::Get().SetOnDisconnected([&]()
    {
        std::cout << "[SunvoltumClient] Disconnected from server.\n";
    });

    NetworkClient::Get().SetOnPacketReceived([&](Net::PacketReader& r)
    {
        using PT = Net::PacketType;
        switch (r.Type())
        {
        // --- Сериализация (начальная загрузка сцены) ---
        case PT::StartSerialization:
            Net::SceneDeserializer::Get().OnStartSerialization(r);
            break;
        case PT::NewInstance:
            Net::SceneDeserializer::Get().OnNewInstance(r);
            break;
        case PT::EndSerialization:
            Net::SceneDeserializer::Get().OnEndSerialization(r);
            break;

        // --- Live-репликация ---
        case PT::InstanceAdded:
            Net::ClientReplicator::Get().OnInstanceAdded(r);
            break;
        case PT::InstanceRemoved:
            Net::ClientReplicator::Get().OnInstanceRemoved(r);
            break;
        case PT::PropertyChanged:
            Net::ClientReplicator::Get().OnPropertyChanged(r);
            break;
        case PT::PropertyUpdate:
            Net::ClientReplicator::Get().OnPropertyUpdate(r);
            break;

        default:
            break;
        }
    });

    NetworkClient::Get().Connect("127.0.0.1", 7777, "Player1");

    auto& dm = engine.DataModel;

    // Workspace и Lighting нужны движку — создаём заглушки.
    // SceneDeserializer переиспользует их при ApplyTo (по имени + classId),
    // обновив свойства данными от сервера.
    auto& ws       = dm.AddInstance("Workspace", Workspace::ClassId);
    auto& lighting = dm.AddInstance("Lighting",  Lighting::ClassId);
    auto& camera   = dm.AddInstance("Camera",    CurrentCamera::ClassId);

    ServerScriptBridge::Get().Init(&dm);

    // --- SceneDeserializer: целевой DataModel и колбэк завершения ---
    // ApplyTo() вызовется автоматически когда все объекты получены.
    Net::SceneDeserializer::Get().SetDataModel(&dm);
    Net::SceneDeserializer::Get().SetOnComplete([&]()
    {
        std::cout << "[SunvoltumClient] Scene fully loaded from server!\n";

        // После загрузки сцены передаём DataModel репликатору —
        // теперь он будет принимать live-изменения от сервера.
        Net::ClientReplicator::Get().SetDataModel(&dm);

        // TODO: найти свой Character (по назначению NetworkOwner),
        //       установить CameraSubject на Head персонажа,
        //       подключить управление персонажем в RenderStepped.
    });

    ws.SetProperty(Workspace::Gravity,        PropertyValue::Number(196.2));
    ws.SetProperty(Workspace::PhysicsEnabled, PropertyValue::Bool(true), true);

    lighting.SetProperty(Lighting::Brightness,         PropertyValue::Number(2.0));
    lighting.SetProperty(Lighting::ClockTime,          PropertyValue::Number(14.0));
    lighting.SetProperty(Lighting::GeographicLatitude, PropertyValue::Number(45.0));
    lighting.SetProperty(Lighting::UseDefaultSky,      PropertyValue::Bool(true));

    camera.SetProperty(CurrentCamera::FieldOfView,     PropertyValue::Number(70.0));
    camera.SetProperty(CurrentCamera::CameraMode,      PropertyValue::CameraType(CameraType::Follow));
    camera.SetProperty(CurrentCamera::MinZoomDistance, PropertyValue::Number(0.0));
    camera.SetProperty(CurrentCamera::MaxZoomDistance, PropertyValue::Number(50.0));
    camera.SetProperty(CurrentCamera::CFrame,
        PropertyValue::CFrame(Sunvoltum::CFrame::FromPosition(0.0f, 10.0f, -15.0f)));

    // -----------------------------------------------------------------------
    //  RenderBridge + Runtime
    // -----------------------------------------------------------------------
    RenderBridge renderBridge;
    if (!renderBridge.Init(engine, 1280, 720, "Sunvoltum — Client"))
        return -1;

    // Ввод — берём из RenderBridge, он уже подключён к окну после Init
    SunvoltumInput& input = renderBridge.GetInput();
    input.SetCursorVisible(false);

    Runtime runtime;
    runtime.SetRenderBridge(&renderBridge); // RenderBridge : IRenderBridge
    runtime.SetInputSource(&input);         // SunvoltumInput : IInputSource
    runtime.SetEngine(&engine);

    runtime.RenderStepped = [&](float /*dt*/)
    {
        if (input.IsKeyPressed(KeyCode::Escape))
            runtime.Stop();
    };

    runtime.PreSimulation = [&](float /*fixedDt*/) {};

    runtime.Heartbeat = [&](float dt)
    {
        // Тик сети
        NetworkManager::Get().Poll();
        NetworkClient::Get().ResendPending();
        NetworkClient::Get().RetryHandshake();

        engine.Tick(dt);

        static double s_time = 0.0;
        s_time += static_cast<double>(dt);
        ServerScriptBridge::Get().StepScheduler(s_time);
    };

    runtime.Start();

    renderBridge.Shutdown();
    engine.Shutdown();
    ServerScriptBridge::Get().Shutdown();
    NetworkClient::Get().Shutdown();
    NetworkManager::Get().Shutdown();

    std::cout << "[SunvoltumClient] Shutdown complete.\n";
    return 0;
}
