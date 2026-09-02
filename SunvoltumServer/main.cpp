#include "SunvoltumEngine.h"
#include "DataModel/InstanceClasses/ShapePart.h"
#include "DataModel/InstanceClasses/Workspace.h"
#include "DataModel/InstanceClasses/Lighting.h"
#include "DataModel/InstanceClasses/Decal.h"
#include "DataModel/InstanceClasses/TextureSurface.h"
#include "DataModel/InstanceClasses/Model.h"
#include "DataModel/InstanceClasses/Motor6D.h"
#include "DataModel/InstanceClasses/Players.h"
#include "DataModel/InstanceClasses/Player.h"
#include "Scripting/ServerSide/ServerScriptBridge.h"
#include "Runtime/Runtime.h"
#include "Network/NetworkManager.h"
#include "Network/NetworkServer.h"
#include "Network/SceneSerializer.h"
#include "Network/ServerReplicator.h"
#include <iostream>
#include <cmath>
#include <string>

using namespace Sunvoltum;
using namespace Sunvoltum::Classes;
using namespace Sunvoltum::Scripting::Server;
using namespace Sunvoltum::Net;

// ---------------------------------------------------------------------------
//  Вспомогательная функция создания ShapePart
// ---------------------------------------------------------------------------
static Instance& MakePart(InstanceParent& parent, const std::string& name,
                           Shape shape, float r, float g, float b,
                           float sx, float sy, float sz,
                           float px, float py, float pz,
                           bool anchored, bool canCollide,
                           float transparency = 0.0f)
{
    auto& part = parent.AddInstance(name, ShapePart::ClassId);
    part.SetProperty(ShapePart::Shape,        PropertyValue::Shape(shape));
    part.SetProperty(ShapePart::Color,        PropertyValue::Color3({r, g, b}));
    part.SetProperty(ShapePart::Transparency, PropertyValue::Float(transparency));
    part.SetProperty(ShapePart::Reflectance,  PropertyValue::Float(0.0f));
    part.SetProperty(ShapePart::Size,         PropertyValue::Vector3({sx, sy, sz}));
    part.SetProperty(ShapePart::CFrame,       PropertyValue::CFrame(
        Sunvoltum::CFrame::FromPosition(px, py, pz)));
    part.SetProperty(ShapePart::Anchored,     PropertyValue::Bool(anchored));
    part.SetProperty(ShapePart::CanCollide,   PropertyValue::Bool(canCollide));
    part.SetProperty(ShapePart::PosVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));
    part.SetProperty(ShapePart::RotVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));
    return part;
}

int main()
{
    std::cout << "[SunvoltumServer] Starting in Server mode...\n";

    Engine engine;
    engine.Init(EngineMode::Server);

    // --- Сеть ---
    NetworkManager::Get().Init(EngineMode::Server);

    auto& dm = engine.DataModel;

    auto& ws       = dm.AddInstance("Workspace", Workspace::ClassId);
    auto& lighting = dm.AddInstance("Lighting",  Lighting::ClassId);
    auto& players  = dm.AddInstance("Players",   Players::ClassId);

    ServerScriptBridge::Get().Init(&dm);

    // --- SceneSerializer: колбэк завершения ---
    // Вызывается когда клиент прислал SerializationComplete — сцена на клиенте готова.
    SceneSerializer::Get().SetOnComplete([](NetworkId peerId)
    {
        std::cout << "[SunvoltumServer] Peer " << peerId
                  << " fully loaded the scene\n";

        // Помечаем пира готовым — теперь он будет получать live-репликацию.
        ServerReplicator::Get().MarkPeerReady(peerId);
    });

    // --- NetworkServer: колбэки и запуск ---
    NetworkServer::Get().SetOnPeerConnected([&](NetworkId id, const std::string& name)
    {
        std::cout << "[SunvoltumServer] Player connected: id=" << id
                  << " name=" << name << "\n";

        // Создаём Player в Players
        auto& playerInst = players.AddInstance(name, Player::ClassId);
        playerInst.SetProperty(Player::Username,  PropertyValue::String(name));
        playerInst.SetProperty(Player::UserId,    PropertyValue::Int(static_cast<int32_t>(id)));
        playerInst.SetProperty(Player::NetworkId, PropertyValue::Int(static_cast<int32_t>(id)));

        std::cout << "[SunvoltumServer] Created Player \"" << name
                  << "\" (NetworkId=" << id << ") in Players\n";

        // Запускаем сериализацию сцены для нового клиента.
        // Player-объекты (ClassId=14) пропускаются — они создаются индивидуально.
        SceneSerializer::Get().StartForPeer(id, dm);
    });

    NetworkServer::Get().SetOnPeerDisconnected([&](NetworkId id)
    {
        std::cout << "[SunvoltumServer] Player disconnected: id=" << id << "\n";
        // Убираем пира из репликации
        ServerReplicator::Get().MarkPeerGone(id);
        // TODO: удалить Character и Player из DataModel
    });

    NetworkServer::Get().SetOnPacketReceived([&](NetworkId id, Net::PacketReader& r)
    {
        using PT = Net::PacketType;
        switch (r.Type())
        {
        case PT::ReadySerialization:
            SceneSerializer::Get().OnReadySerialization(id, r);
            break;

        case PT::AskInstance:
            SceneSerializer::Get().OnAskInstance(id, r);
            break;

        case PT::SerializationComplete:
            SceneSerializer::Get().OnSerializationComplete(id, r);
            break;

        // TODO: обработка PropertyUpdate и других пакетов от клиента
        default:
            break;
        }
    });

    constexpr uint16_t SERVER_PORT = 7777;
    if (!NetworkServer::Get().Listen("0.0.0.0", SERVER_PORT))
    {
        std::cerr << "[SunvoltumServer] Failed to open UDP port " << SERVER_PORT << "\n";
        return -1;
    }

    ws.SetProperty(Workspace::Gravity,        PropertyValue::Number(196.2));
    ws.SetProperty(Workspace::PhysicsEnabled, PropertyValue::Bool(true), true);

    // -----------------------------------------------------------------------
    //  Освещение
    // -----------------------------------------------------------------------
    lighting.SetProperty(Lighting::Brightness,         PropertyValue::Number(2.0));
    lighting.SetProperty(Lighting::ClockTime,          PropertyValue::Number(14.0));
    lighting.SetProperty(Lighting::GeographicLatitude, PropertyValue::Number(45.0));
    lighting.SetProperty(Lighting::UseDefaultSky,      PropertyValue::Bool(true));

    // -----------------------------------------------------------------------
    //  Пол 95 x 1.5 x 95
    // -----------------------------------------------------------------------
    auto& floor = MakePart(ws, "Floor", Shape::Block,
                            0.6f, 0.65f, 0.7f,
                            95.0f, 1.5f, 95.0f,
                            0.0f, 0.0f, 0.0f,
                            true, true);
    {
        auto& g = floor.AddInstance("GrassSurface", TextureSurface::ClassId);
        g.SetProperty(TextureSurface::Face,          PropertyValue::Int(0));
        g.SetProperty(TextureSurface::Texture,       PropertyValue::String("PlatformContent/textures/grass/grass.dds"));
        g.SetProperty(TextureSurface::StudsPerTileU, PropertyValue::Float(2.25f));
        g.SetProperty(TextureSurface::StudsPerTileV, PropertyValue::Float(2.25f));
    }

    // -----------------------------------------------------------------------
    //  Стены
    // -----------------------------------------------------------------------
    const float fH = 47.5f, wH = 12.0f, wT = 2.0f;
    const float wY = 0.75f + wH * 0.5f;
    MakePart(ws, "WallNorth", Shape::Block, 0.45f, 0.45f, 0.5f,  95.0f, wH, wT,          0.0f, wY,  fH + wT * 0.5f, true, true);
    MakePart(ws, "WallSouth", Shape::Block, 0.45f, 0.45f, 0.5f,  95.0f, wH, wT,          0.0f, wY, -fH - wT * 0.5f, true, true);
    MakePart(ws, "WallEast",  Shape::Block, 0.45f, 0.45f, 0.5f,  wT, wH, 95.0f + wT * 2.f,  fH + wT * 0.5f, wY, 0.0f, true, true);
    MakePart(ws, "WallWest",  Shape::Block, 0.45f, 0.45f, 0.5f,  wT, wH, 95.0f + wT * 2.f, -fH - wT * 0.5f, wY, 0.0f, true, true);

    // -----------------------------------------------------------------------
    //  Персонаж (Character1) — создаётся сервером при старте.
    //  При сетевом подключении игрока этот объект будет переиспользован
    //  (или создан аналогично) и передан клиенту как его Character.
    //
    //  Конструкция R6:
    //    HumanoidRootPart — единственная физическая часть (Anchored=false)
    //    Все остальные части — Anchored=true, CanCollide=false,
    //    позиционируются через Motor6D каждый физический тик.
    // -----------------------------------------------------------------------
    const float FLOOR_TOP = 0.75f;
    const float HRP_H     = 4.0f;
    const float HRP_CY    = FLOOR_TOP + HRP_H * 0.5f;  // 2.75

    const float AX = 0.0f, AZ = 0.0f;

    // Цвета частей тела
    const float SK_R = 0.957f, SK_G = 0.800f, SK_B = 0.263f; // кожа
    const float LG_R = 0.647f, LG_G = 0.737f, LG_B = 0.314f; // штаны
    const float TR_R = 0.051f, TR_G = 0.412f, TR_B = 0.671f; // рубашка

    auto& charModel = ws.AddInstance("Character1", Model::ClassId);

    auto& hrp = MakePart(charModel, "HumanoidRootPart", Shape::Block,
                          0.0f, 0.0f, 0.0f,
                          2.0f, HRP_H, 1.0f,
                          AX, HRP_CY, AZ,
                          false, true, 1.0f);

    auto& torso    = MakePart(charModel, "Torso",     Shape::Block, TR_R, TR_G, TR_B, 2.0f, 2.0f, 1.0f,  AX,        HRP_CY + 1.0f, AZ, true, false);
    auto& leftLeg  = MakePart(charModel, "Left Leg",  Shape::Block, LG_R, LG_G, LG_B, 1.0f, 2.0f, 1.0f,  AX - 0.5f, HRP_CY - 1.0f, AZ, true, false);
    auto& rightLeg = MakePart(charModel, "Right Leg", Shape::Block, LG_R, LG_G, LG_B, 1.0f, 2.0f, 1.0f,  AX + 0.5f, HRP_CY - 1.0f, AZ, true, false);
    auto& leftArm  = MakePart(charModel, "Left Arm",  Shape::Block, SK_R, SK_G, SK_B, 1.0f, 2.0f, 1.0f,  AX - 1.5f, HRP_CY + 1.0f, AZ, true, false);
    auto& rightArm = MakePart(charModel, "Right Arm", Shape::Block, SK_R, SK_G, SK_B, 1.0f, 2.0f, 1.0f,  AX + 1.5f, HRP_CY + 1.0f, AZ, true, false);

    const float HEAD_CY = HRP_CY + 1.0f + 1.0f + 0.8f; // 5.55
    auto& head = MakePart(charModel, "Head", Shape::Ball,
                           SK_R, SK_G, SK_B,
                           1.6f, 1.6f, 1.6f,
                           AX, HEAD_CY, AZ,
                           true, false);
    {
        auto& face = head.AddInstance("face", Decal::ClassId);
        face.SetProperty(Decal::Face,         PropertyValue::Int(4));
        face.SetProperty(Decal::Texture,      PropertyValue::String("PlatformContent/textures/EpicFace.dds"));
        face.SetProperty(Decal::Transparency, PropertyValue::Float(0.0f));
    }

    charModel.SetProperty(Model::PrimaryPart, PropertyValue::Ref(&hrp));

    // -----------------------------------------------------------------------
    //  Motor6D — соединения конечностей
    // -----------------------------------------------------------------------
    auto MakeMotor = [&](const std::string& name,
                         Instance& part1,
                         float c0x, float c0y, float c0z,
                         float c1x, float c1y, float c1z) -> Instance&
    {
        auto& motor = charModel.AddInstance(name, Motor6D::ClassId);
        Motor6D::Init(motor);
        motor.SetProperty(Motor6D::Part0, PropertyValue::Ref(&hrp));
        motor.SetProperty(Motor6D::Part1, PropertyValue::Ref(&part1));
        motor.SetProperty(Motor6D::C0,
            PropertyValue::CFrame(Sunvoltum::CFrame::FromPosition(c0x, c0y, c0z)));
        motor.SetProperty(Motor6D::C1,
            PropertyValue::CFrame(Sunvoltum::CFrame::FromPosition(c1x, c1y, c1z)));
        return motor;
    };

    MakeMotor("RootJoint",     torso,     0.0f,  0.0f, 0.0f,   0.0f, -1.0f, 0.0f);
    MakeMotor("LeftHip",       leftLeg,  -0.5f,  0.0f, 0.0f,   0.0f, +1.0f, 0.0f);
    MakeMotor("RightHip",      rightLeg, +0.5f,  0.0f, 0.0f,   0.0f, +1.0f, 0.0f);
    MakeMotor("LeftShoulder",  leftArm,  -2.0f, +1.5f, 0.0f,  -0.5f, +0.5f, 0.0f);
    MakeMotor("RightShoulder", rightArm, +2.0f, +1.5f, 0.0f,  +0.5f, +0.5f, 0.0f);
    MakeMotor("Neck",          head,      0.0f, +2.0f, 0.0f,   0.0f, -0.8f, 0.0f);

    // -----------------------------------------------------------------------
    //  Тестовые кубики — падают с высоты, проверяют репликацию физики
    // -----------------------------------------------------------------------
    MakePart(ws, "FallingCube1", Shape::Block,
             0.9f, 0.3f, 0.3f,          // красный
             2.0f, 2.0f, 2.0f,
             5.0f, 20.0f, 5.0f,
             /*anchored=*/false, /*canCollide=*/true);

    MakePart(ws, "FallingCube2", Shape::Ball,
             0.3f, 0.6f, 0.9f,          // синий шар
             2.0f, 2.0f, 2.0f,
             -5.0f, 30.0f, -5.0f,
             /*anchored=*/false, /*canCollide=*/true);

    std::cout << "[SunvoltumServer] World built. Starting runtime...\n";

    // --- ServerReplicator: инициализация после построения сцены ---
    // Обходит всё дерево DataModel, присваивает InstanceNetId каждому объекту,
    // навешивает подписки на ChildAdded/ChildRemoved/PropertyChanged.
    // С этого момента любые изменения DataModel будут реплицироваться
    // всем пирам у которых сериализация уже завершена.
    ServerReplicator::Get().Init(dm);

    // -----------------------------------------------------------------------
    //  Runtime — без RenderBridge (сервер не рендерит)
    // -----------------------------------------------------------------------
    Runtime runtime;
    runtime.SetEngine(&engine);
    // SetRenderBridge намеренно не вызывается

    bool hrpLocked = false;

    // Плавное время суток — та же формула что в старом main.cpp:
    // dt * 0.05 → ~3 минуты игрового времени в секунду реального
    double clockTime = 14.0;

    runtime.PreSimulation = [&](float /*fixedDt*/)
    {
        if (!hrpLocked && engine.Physics.IsInitialized())
        {
            engine.Physics.LockUpright(hrp);
            hrpLocked = true;
        }
    };

    runtime.Heartbeat = [&](float dt)
    {
        // Тик сети — обрабатываем входящие UDP-датаграммы
        NetworkManager::Get().Poll();
        NetworkServer::Get().ResendPending();

        // Rate-limited PropertyUpdate — отправляем накопленные грязные значения
        ServerReplicator::Get().Tick();

        engine.Tick(dt);

        // Время суток: каждый кадр как в старом main.cpp
        clockTime += static_cast<double>(dt) * 0.5;
        if (clockTime >= 24.0) clockTime -= 24.0;
        lighting.SetProperty(Lighting::ClockTime, PropertyValue::Number(clockTime));

        static double s_time = 0.0;
        s_time += static_cast<double>(dt);
        ServerScriptBridge::Get().StepScheduler(s_time);
    };

    // TODO: сюда подключить NetworkServer после выбора транспорта.
    // runtime.RenderStepped — оставляем пустым на сервере,
    // входящий сетевой трафик (позиции игроков) будет обрабатываться здесь.

    runtime.Start();

    engine.Shutdown();
    ServerScriptBridge::Get().Shutdown();
    ServerReplicator::Get().Shutdown();
    NetworkServer::Get().Shutdown();
    NetworkManager::Get().Shutdown();

    std::cout << "[SunvoltumServer] Shutdown complete.\n";
    return 0;
}
