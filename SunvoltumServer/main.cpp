#include "SunvoltumEngine.h"
#include "DataModel/InstanceClasses/ShapePart.h"
#include "DataModel/InstanceClasses/Workspace.h"
#include "DataModel/InstanceClasses/Lighting.h"
#include "DataModel/InstanceClasses/Players.h"
#include "DataModel/InstanceClasses/Player.h"
#include "DataModel/InstanceClasses/Script.h"
#include "DataModel/InstanceClasses/Motor6D.h"
#include "DataModel/InstanceClasses/Humanoid.h"
#include "Scripting/ServerSide/ServerScriptBridge.h"
#include "Runtime/Runtime.h"
#include "Network/NetworkManager.h"
#include "Network/NetworkServer.h"
#include "Network/SceneSerializer.h"
#include "Network/ServerReplicator.h"
#include "Network/NetworkSerializer.h"
#include "DataModel/InstanceRegistry.h"
#include "Types/CFrame.h"
#include "Types/Vector3.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace Sunvoltum;
using namespace Sunvoltum::Classes;
using namespace Sunvoltum::Scripting::Server;
using namespace Sunvoltum::Net;

static bool ReadFile(const std::string& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    if (out.size() >= 3 &&
        static_cast<unsigned char>(out[0]) == 0xEF &&
        static_cast<unsigned char>(out[1]) == 0xBB &&
        static_cast<unsigned char>(out[2]) == 0xBF)
        out.erase(0, 3);
    return true;
}

int main()
{
    std::cout << "[SunvoltumServer] Starting in Server mode...\n";

    Engine engine;
    engine.Init(EngineMode::Server);

    NetworkManager::Get().Init(EngineMode::Server);

    auto& dm = engine.DataModel;

    auto& ws      = dm.AddInstance("Workspace", Workspace::ClassId);
    auto& lighting = dm.AddInstance("Lighting",  Lighting::ClassId);
    auto& players  = dm.AddInstance("Players",   Players::ClassId);

    ServerScriptBridge::Get().Init(&dm);

    lighting.SetProperty(Lighting::UseDefaultSky, PropertyValue::Bool(true));

    ServerScriptBridge::Get().WatchWorkspace();

    // -----------------------------------------------------------------------
    //  Запуск скрипта сцены
    // -----------------------------------------------------------------------
    {
        const std::string scriptPath = "Scripts/ServerScene.lua";
        std::string source;
        if (!ReadFile(scriptPath, source))
        {
            std::cerr << "[SunvoltumServer] ERROR: cannot open " << scriptPath << "\n";
            return -1;
        }

        int scriptId = ServerScriptBridge::Get().LoadScriptFromSource(source);
        ws.AddInstance("ServerScene", Script::ClassId, [scriptId](Instance& inst)
        {
            inst.SetProperty(Script::ScriptId, PropertyValue::Int(scriptId));
            inst.SetProperty(Script::Disabled,  PropertyValue::Bool(false));
        });

        std::cout << "[SunvoltumServer] ServerScene.lua executed successfully.\n";
    }

    // -----------------------------------------------------------------------
    //  C++ ссылки на объекты сцены
    // -----------------------------------------------------------------------
    Instance* charModel = ws.FindByName("Character1");
    Instance* hrp       = charModel ? charModel->FindByName("HumanoidRootPart") : nullptr;
    Instance* humanoid  = charModel ? charModel->FindByName("Humanoid")         : nullptr;

    if (!hrp)
        std::cerr << "[SunvoltumServer] WARNING: HumanoidRootPart not found.\n";
    if (!humanoid)
        std::cerr << "[SunvoltumServer] WARNING: Humanoid not found.\n";

    std::vector<Instance*> ballInstances;
    ballInstances.reserve(20);
    for (int i = 1; i <= 20; ++i)
    {
        Instance* ball = ws.FindByName("Ball" + std::to_string(i));
        if (ball)
            ballInstances.push_back(ball);
        else
            std::cerr << "[SunvoltumServer] WARNING: Ball" << i << " not found.\n";
    }

    Instance* chairSeat  = ws.FindByName("ChairSeat");
    Instance* propMotor  = ws.FindByName("PropMotor");

    std::cout << "[SunvoltumServer] World ready: "
              << ballInstances.size() << " balls, "
              << (hrp      ? "HRP ok"      : "HRP missing")      << ", "
              << (humanoid ? "Humanoid ok" : "Humanoid missing")  << ".\n";

    // -----------------------------------------------------------------------
    //  Сетевые колбэки
    // -----------------------------------------------------------------------
    SceneSerializer::Get().SetOnComplete([&](NetworkId peerId)
    {
        std::cout << "[SunvoltumServer] Peer " << peerId << " fully loaded the scene\n";
        ServerReplicator::Get().MarkPeerReady(peerId);
    });

    NetworkServer::Get().SetOnPeerConnected([&](NetworkId id, const std::string& name)
    {
        std::cout << "[SunvoltumServer] Player connected: id=" << id
                  << " name=" << name << "\n";

        auto& playerInst = players.AddInstance(name, Player::ClassId);
        playerInst.SetProperty(Player::Username,  PropertyValue::String(name));
        playerInst.SetProperty(Player::UserId,    PropertyValue::Int(static_cast<int32_t>(id)));
        playerInst.SetProperty(Player::NetworkId, PropertyValue::Int(static_cast<int32_t>(id)));

        SceneSerializer::Get().StartForPeer(id, dm);
    });

    NetworkServer::Get().SetOnPeerDisconnected([&](NetworkId id)
    {
        std::cout << "[SunvoltumServer] Player disconnected: id=" << id << "\n";
        ServerReplicator::Get().MarkPeerGone(id);
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

    ServerReplicator::Get().Init(dm);

    // -----------------------------------------------------------------------
    //  Runtime
    // -----------------------------------------------------------------------
    Runtime runtime;
    runtime.SetEngine(&engine);

    double clockTime = 14.0;

    // -------------------------------------------------------------------
    //  AI-состояние персонажа: преследование случайного мяча + прыжок
    // -------------------------------------------------------------------
    int    aiTargetBallIdx = 0;           // индекс в ballInstances
    double aiJumpTimer     = 0.0;         // накопленное время до следующего прыжка
    constexpr double AI_JUMP_INTERVAL  = 6.0;   // прыжок каждые 6 секунд
    constexpr double AI_RETARGET_DIST  = 3.0;   // переключить мяч если подошли ближе N стадов
    // Инициализируем случайный мяч из ballInstances
    if (!ballInstances.empty())
    {
        // простой детерминированный "случайный" старт — берём мяч #7
        aiTargetBallIdx = static_cast<int>(ballInstances.size()) > 7 ? 7 : 0;
    }

    runtime.PreSimulation = [&](float fixedDt)
    {
        // -------------------------------------------------------------------
        //  AI: персонаж бежит к случайному мячу + прыгает каждые 6 секунд
        // -------------------------------------------------------------------
        if (humanoid && hrp && !ballInstances.empty() && engine.Physics.IsInitialized())
        {
            using H  = Classes::Humanoid;
            using BP = Classes::BasePart;

            // Позиция HRP
            Sunvoltum::Vector3 hrpPos{};
            {
                auto* cfProp = hrp->GetProperty(BP::CFrame);
                if (cfProp && cfProp->Type == PropertyType::CFrame)
                    hrpPos = cfProp->Value.AsCFrame.Position;
            }

            // Позиция текущего целевого мяча
            Instance* targetBall = ballInstances[aiTargetBallIdx];
            Sunvoltum::Vector3 ballPos{};
            {
                auto* cfProp = targetBall->GetProperty(BP::CFrame);
                if (cfProp && cfProp->Type == PropertyType::CFrame)
                    ballPos = cfProp->Value.AsCFrame.Position;
            }

            // Горизонтальный вектор к мячу
            float dx = ballPos.X - hrpPos.X;
            float dz = ballPos.Z - hrpPos.Z;
            float horizDist = std::sqrt(dx * dx + dz * dz);

            // Если подошли близко — выбираем следующий мяч (по кругу)
            if (horizDist < static_cast<float>(AI_RETARGET_DIST))
            {
                aiTargetBallIdx = (aiTargetBallIdx + 1) % static_cast<int>(ballInstances.size());
                targetBall = ballInstances[aiTargetBallIdx];
                // Пересчитываем позицию нового мяча
                auto* cfProp = targetBall->GetProperty(BP::CFrame);
                if (cfProp && cfProp->Type == PropertyType::CFrame)
                    ballPos = cfProp->Value.AsCFrame.Position;
                dx = ballPos.X - hrpPos.X;
                dz = ballPos.Z - hrpPos.Z;
                horizDist = std::sqrt(dx * dx + dz * dz);
            }

            // Нормализуем и записываем MoveDirection
            if (horizDist > 1e-4f)
            {
                float inv = 1.0f / horizDist;
                humanoid->SetProperty(H::MoveDirection,
                    PropertyValue::Vector3(Sunvoltum::Vector3(dx * inv, 0.0f, dz * inv)));
            }
            else
            {
                humanoid->SetProperty(H::MoveDirection,
                    PropertyValue::Vector3(Sunvoltum::Vector3(0.0f, 0.0f, 0.0f)));
            }

            // Прыжок каждые AI_JUMP_INTERVAL секунд
            aiJumpTimer += static_cast<double>(fixedDt);
            if (aiJumpTimer >= AI_JUMP_INTERVAL)
            {
                humanoid->SetProperty(H::Jump, PropertyValue::Bool(true));
                aiJumpTimer -= AI_JUMP_INTERVAL;
            }

            // --- Лог позиции раз в секунду ---
            static double s_logTimer = 0.0;
            s_logTimer += static_cast<double>(fixedDt);
            if (s_logTimer >= 1.0)
            {
                s_logTimer -= 1.0;
                auto* stProp = humanoid->GetProperty(H::State);
                int32_t st = (stProp && stProp->Type == PropertyType::Int)
                             ? stProp->Value.AsInt : -1;
                const char* stName[] = { "Idle", "Walking", "Jumping", "Falling", "Dead" };
                std::cout << "[AI] HRP=(" << hrpPos.X << ", " << hrpPos.Y << ", " << hrpPos.Z
                          << ")  target=Ball" << (aiTargetBallIdx + 1)
                          << " at (" << ballPos.X << ", " << ballPos.Z << ")"
                          << "  dist=" << horizDist
                          << "  state=" << (st >= 0 && st <= 4 ? stName[st] : "?")
                          << "\n";
            }
        }

        // -------------------------------------------------------------------
        //  Humanoid: движение, прыжок, обновление State
        //
        //  LockUpright вызывается автоматически PhysicsBridge при регистрации
        //  HumanoidRootPart — вручную здесь больше не нужен.
        // -------------------------------------------------------------------
        if (humanoid && hrp && engine.Physics.IsInitialized())
        {
            using H = Classes::Humanoid;
            using BP = Classes::BasePart;

            // Читаем свойства Humanoid
            auto* walkSpeedProp    = humanoid->GetProperty(H::WalkSpeed);
            auto* jumpPowerProp    = humanoid->GetProperty(H::JumpPower);
            auto* moveDirProp      = humanoid->GetProperty(H::MoveDirection);
            auto* jumpProp         = humanoid->GetProperty(H::Jump);
            auto* healthProp       = humanoid->GetProperty(H::Health);
            auto* stateProp        = humanoid->GetProperty(H::State);

            float walkSpeed = (walkSpeedProp && walkSpeedProp->Type == PropertyType::Float)
                              ? walkSpeedProp->Value.AsFloat : 16.0f;
            float jumpPower = (jumpPowerProp && jumpPowerProp->Type == PropertyType::Float)
                              ? jumpPowerProp->Value.AsFloat : 50.0f;
            float health    = (healthProp && healthProp->Type == PropertyType::Float)
                              ? healthProp->Value.AsFloat : 100.0f;
            int32_t state   = (stateProp && stateProp->Type == PropertyType::Int)
                              ? stateProp->Value.AsInt : H::STATE_IDLE;

            Sunvoltum::Vector3 moveDir{};
            if (moveDirProp && moveDirProp->Type == PropertyType::Vector3)
                moveDir = moveDirProp->Value.AsVector3;

            bool wantsJump = (jumpProp && jumpProp->Type == PropertyType::Bool)
                             && jumpProp->Value.AsBool;

            // Текущая скорость HRP
            auto* posVelProp = hrp->GetProperty(BP::PosVelocity);
            Sunvoltum::Vector3 vel{};
            if (posVelProp && posVelProp->Type == PropertyType::Vector3)
                vel = posVelProp->Value.AsVector3;

            // ------------------------------------------------------------------
            //  Grounded-проверка: raycast вниз от нижней грани HRP
            //  HRP Size.Y = 4 → нижняя грань = pos.Y - 2.0
            // ------------------------------------------------------------------
            bool grounded = false;
            {
                auto* cfProp = hrp->GetProperty(BP::CFrame);
                if (cfProp && cfProp->Type == PropertyType::CFrame)
                {
                    const auto& pos = cfProp->Value.AsCFrame.Position;
                    constexpr float HRP_HALF_H   = 2.0f;
                    constexpr float RAY_OFFSET    = 0.05f;
                    constexpr float RAY_MAX_DIST  = 0.20f;
                    Sunvoltum::Vector3 rayOrigin(
                        pos.X,
                        pos.Y - HRP_HALF_H + RAY_OFFSET,
                        pos.Z);
                    auto hit = engine.Physics.Raycast(
                        rayOrigin,
                        Sunvoltum::Vector3(0.0f, -1.0f, 0.0f),
                        RAY_MAX_DIST,
                        hrp);
                    grounded = hit.Hit;
                }
            }

            // ------------------------------------------------------------------
            //  Смерть — не применяем движение
            // ------------------------------------------------------------------
            if (health <= 0.0f)
            {
                if (state != H::STATE_DEAD)
                    humanoid->SetProperty(H::State, PropertyValue::Int(H::STATE_DEAD));
            }
            else
            {
                // ------------------------------------------------------------------
                //  Прыжок
                //  JUMP_POWER_SCALE: коэффициент совместимости SunvoltumPhysics ↔ PhysX 5.0.
                //  В PhysX JumpPower=50 давало определённую высоту прыжка.
                //  SunvoltumPhysics интегрирует скорость иначе (нет внутреннего масштаба),
                //  поэтому для того же визуального результата нужно умножить на 3.0.
                //  Итог: в скриптах/параметрах JumpPower=50 → применяется как 150 studs/s.
                // ------------------------------------------------------------------
                static constexpr float JUMP_POWER_SCALE = 3.0f;
                if (wantsJump && grounded)
                {
                    vel.Y = jumpPower * JUMP_POWER_SCALE;
                    hrp->SetProperty(BP::PosVelocity, PropertyValue::Vector3(vel));
                    humanoid->SetProperty(H::Jump, PropertyValue::Bool(false));
                    humanoid->SetProperty(H::State, PropertyValue::Int(H::STATE_JUMPING));
                }
                else
                {
                    // Сбрасываем Jump-триггер даже если не на земле
                    if (wantsJump)
                        humanoid->SetProperty(H::Jump, PropertyValue::Bool(false));

                    // ------------------------------------------------------------------
                    //  Горизонтальное движение: применяем WalkSpeed по X/Z
                    //  Y-скорость оставляем физике (гравитация/прыжок)
                    // ------------------------------------------------------------------
                    float moveLen = std::sqrt(moveDir.X * moveDir.X + moveDir.Z * moveDir.Z);
                    if (moveLen > 1e-4f)
                    {
                        float inv = 1.0f / moveLen;
                        vel.X = moveDir.X * inv * walkSpeed;
                        vel.Z = moveDir.Z * inv * walkSpeed;
                    }
                    else
                    {
                        vel.X = 0.0f;
                        vel.Z = 0.0f;
                    }
                    hrp->SetProperty(BP::PosVelocity, PropertyValue::Vector3(vel));

                    // ------------------------------------------------------------------
                    //  State: Idle / Walking / Jumping / Falling
                    // ------------------------------------------------------------------
                    int32_t newState;
                    if (!grounded && vel.Y > 0.1f)
                        newState = H::STATE_JUMPING;
                    else if (!grounded && vel.Y < -0.1f)
                        newState = H::STATE_FALLING;
                    else if (moveLen > 1e-4f)
                        newState = H::STATE_WALKING;
                    else
                        newState = H::STATE_IDLE;

                    if (newState != state)
                        humanoid->SetProperty(H::State, PropertyValue::Int(newState));
                }
            }
        }

        // Bounce-логика для мячей
        if (engine.Physics.IsInitialized())
        {
            constexpr float BALL_RADIUS  = 1.5f;
            constexpr float BOUNCE_VEL   = 55.0f;
            constexpr float RAY_OFFSET   = 0.05f;
            constexpr float RAY_MAX_DIST = 0.25f;

            for (Instance* ball : ballInstances)
            {
                const PropertyValue* vp = ball->GetProperty(ShapePart::PosVelocity);
                if (!vp || vp->Type != PropertyType::Vector3) continue;
                Sunvoltum::Vector3 vel = vp->Value.AsVector3;
                if (vel.Y > 0.0f) continue;

                const PropertyValue* cp = ball->GetProperty(ShapePart::CFrame);
                if (!cp || cp->Type != PropertyType::CFrame) continue;
                const Sunvoltum::Vector3& pos = cp->Value.AsCFrame.Position;

                Sunvoltum::Vector3 rayOrigin(pos.X, pos.Y - BALL_RADIUS + RAY_OFFSET, pos.Z);
                auto hit = engine.Physics.Raycast(
                    rayOrigin, Sunvoltum::Vector3(0.0f, -1.0f, 0.0f), RAY_MAX_DIST, ball);

                if (hit.Hit)
                {
                    vel.Y = BOUNCE_VEL;
                    ball->SetProperty(ShapePart::PosVelocity, PropertyValue::Vector3(vel));
                }
            }
        }

        // Bounce-логика для стула (ChairSeat — главная динамическая часть)
        if (engine.Physics.IsInitialized() && chairSeat)
        {
            // Расстояние от центра сидения до дна ножек:
            //   legDY = -0.9 (центр ножки), halfLegH = 0.75 → нижняя грань = -1.65
            constexpr float CHAIR_BOTTOM  = 1.65f;
            constexpr float BOUNCE_VEL    = 55.0f;
            constexpr float RAY_OFFSET    = 0.05f;
            constexpr float RAY_MAX_DIST  = 0.25f;

            const PropertyValue* vp = chairSeat->GetProperty(ShapePart::PosVelocity);
            if (vp && vp->Type == PropertyType::Vector3)
            {
                Sunvoltum::Vector3 vel = vp->Value.AsVector3;
                if (vel.Y <= 0.0f)
                {
                    const PropertyValue* cp = chairSeat->GetProperty(ShapePart::CFrame);
                    if (cp && cp->Type == PropertyType::CFrame)
                    {
                        const Sunvoltum::Vector3& pos = cp->Value.AsCFrame.Position;
                        // Raycast от нижней грани ножек вниз
                        Sunvoltum::Vector3 rayOrigin(
                            pos.X,
                            pos.Y - CHAIR_BOTTOM + RAY_OFFSET,
                            pos.Z);
                        auto hit = engine.Physics.Raycast(
                            rayOrigin,
                            Sunvoltum::Vector3(0.0f, -1.0f, 0.0f),
                            RAY_MAX_DIST,
                            chairSeat);
                        if (hit.Hit)
                        {
                            vel.Y = BOUNCE_VEL;
                            chairSeat->SetProperty(ShapePart::PosVelocity,
                                PropertyValue::Vector3(vel));
                        }
                    }
                }
            }
        }

        // Пропеллер: каждый тик прибавляем к DesiredAngle шаг MaxVelocity * dt.
        // PhysicsBridge::SyncJointsDrive видит (DesiredAngle - CurrentAngle) > 0
        // и выставляет DriveVelocity = +MaxVelocity → вал крутится непрерывно.
        if (propMotor && engine.Physics.IsInitialized())
        {
            using M6D = Classes::Motor6D;

            auto* maxVelProp     = propMotor->GetProperty(M6D::MaxVelocity);
            auto* currentAngProp = propMotor->GetProperty(M6D::CurrentAngle);
            auto* desiredAngProp = propMotor->GetProperty(M6D::DesiredAngle);

            if (maxVelProp     && maxVelProp->Type     == PropertyType::Float &&
                currentAngProp && currentAngProp->Type == PropertyType::Float &&
                desiredAngProp && desiredAngProp->Type == PropertyType::Float)
            {
                float maxVel      = maxVelProp->Value.AsFloat;
                float currentAngle = currentAngProp->Value.AsFloat;
                // Держим DesiredAngle на два шага впереди CurrentAngle —
                // мотор никогда не "догоняет" цель и не останавливается.
                float newDesired  = currentAngle + maxVel * fixedDt * 2.0f;
                propMotor->SetProperty(M6D::DesiredAngle,
                    PropertyValue::Float(newDesired));
            }
        }
    };

    runtime.Heartbeat = [&](float dt)
    {
        NetworkManager::Get().Poll();
        NetworkServer::Get().ResendPending();
        ServerReplicator::Get().Tick();
        engine.Tick(dt);

        clockTime += static_cast<double>(dt) * 0.5;
        if (clockTime >= 24.0) clockTime -= 24.0;
        lighting.SetProperty(Lighting::ClockTime, PropertyValue::Number(clockTime));

        static double s_time = 0.0;
        s_time += static_cast<double>(dt);
        ServerScriptBridge::Get().StepScheduler(s_time);
    };

    runtime.Start();

    engine.Shutdown();
    ServerScriptBridge::Get().Shutdown();
    ServerReplicator::Get().Shutdown();
    NetworkServer::Get().Shutdown();
    NetworkManager::Get().Shutdown();

    std::cout << "[SunvoltumServer] Shutdown complete.\n";
    return 0;
}
