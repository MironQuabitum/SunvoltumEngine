#include "SunvoltumEngine.h"
#include "Rendering/RenderBridge.h"
#include "DataModel/InstanceClasses/Script.h"
#include "DataModel/InstanceClasses/LocalScript.h"
#include "DataModel/InstanceClasses/CurrentCamera.h"
#include "DataModel/InstanceClasses/ShapePart.h"
#include "DataModel/InstanceClasses/Workspace.h"
#include "DataModel/InstanceClasses/Lighting.h"
#include "DataModel/InstanceClasses/Decal.h"
#include "DataModel/InstanceClasses/Sound.h"
#include "DataModel/InstanceClasses/TextureSurface.h"
#include "DataModel/InstanceClasses/Model.h"
#include "DataModel/InstanceClasses/Motor6D.h"
#include "Scripting/ServerSide/ServerScriptBridge.h"
#include <iostream>
#include <cmath>
#include <string>

using namespace Sunvoltum;
using namespace Sunvoltum::Classes;
using namespace Sunvoltum::Scripting::Server;

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
    Engine engine;
    engine.Init(EngineMode::Client);

    auto& dm = engine.DataModel;

    auto& ws       = dm.AddInstance("Workspace",  Workspace::ClassId);
    auto& lighting = dm.AddInstance("Lighting",   Lighting::ClassId);
    auto& camera   = dm.AddInstance("Camera",     CurrentCamera::ClassId);

    ServerScriptBridge::Get().Init(&dm);

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
    //  Верхняя грань = 0.0 + 1.5/2 = 0.75
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
    MakePart(ws, "WallNorth", Shape::Block, 0.45f,0.45f,0.5f,  95.0f, wH, wT,         0.0f, wY,  fH+wT*0.5f, true,true);
    MakePart(ws, "WallSouth", Shape::Block, 0.45f,0.45f,0.5f,  95.0f, wH, wT,         0.0f, wY, -fH-wT*0.5f, true,true);
    MakePart(ws, "WallEast",  Shape::Block, 0.45f,0.45f,0.5f,  wT, wH, 95.0f+wT*2.f,  fH+wT*0.5f, wY, 0.0f, true,true);
    MakePart(ws, "WallWest",  Shape::Block, 0.45f,0.45f,0.5f,  wT, wH, 95.0f+wT*2.f, -fH-wT*0.5f, wY, 0.0f, true,true);

    // -----------------------------------------------------------------------
    //  Аватар (Roblox R6)
    //
    //  Пол: Y = 0.75  (верхняя грань)
    //
    //  HumanoidRootPart — ЕДИНСТВЕННАЯ часть с физикой:
    //    Size  = 2 x 4 x 1  (покрывает ноги + торс)
    //    Y     = 0.75 + 4/2 = 2.75  (нижняя грань = 0.75 = пол)
    //    Anchored = false, CanCollide = true, Transparency = 1 (невидим)
    //
    //  Все остальные части: Anchored = true, CanCollide = false,
    //    позиция = позиция HRP + фиксированный оффсет.
    //
    //  Оффсеты (в локальных координатах, Y растёт вверх):
    //    Torso    : dY = 0       (центр HRP)
    //    LeftLeg  : dX = -0.5,  dY = -1.0
    //    RightLeg : dX = +0.5,  dY = -1.0
    //    LeftArm  : dX = -1.5,  dY =  0.0
    //    RightArm : dX = +1.5,  dY =  0.0
    //    Head     : dY = +2.5   (centre = 2.75 + 2 + 0.5 = 5.25)
    // -----------------------------------------------------------------------

    const float FLOOR_TOP = 0.75f;
    const float HRP_H     = 4.0f;               // полная высота HRP
    const float HRP_CY    = FLOOR_TOP + HRP_H * 0.5f;  // 2.75

    const float AX = 0.0f, AZ = 0.0f;

    // Цвета частей тела
    // Кожа (голова, руки):  0.957, 0.800, 0.263
    // Штаны (ноги):         0.647, 0.737, 0.314
    // Рубашка (торс):       0.051, 0.412, 0.671
    const float SK_R = 0.957f, SK_G = 0.800f, SK_B = 0.263f; // skin
    const float LG_R = 0.647f, LG_G = 0.737f, LG_B = 0.314f; // legs
    const float TR_R = 0.051f, TR_G = 0.412f, TR_B = 0.671f; // torso

    // Model "Character" в Workspace
    auto& charModel = ws.AddInstance("Character", Model::ClassId);

    // HumanoidRootPart — физическое тело, невидимо (transparency=1)
    auto& hrp = MakePart(charModel, "HumanoidRootPart", Shape::Block,
                          0.0f, 0.0f, 0.0f,
                          2.0f, HRP_H, 1.0f,
                          AX, HRP_CY, AZ,
                          false, true, 1.0f);

    // Torso — рубашка
    auto& torso = MakePart(charModel, "Torso", Shape::Block,
                            TR_R, TR_G, TR_B,
                            2.0f, 2.0f, 1.0f,
                            AX, HRP_CY + 1.0f, AZ,
                            true, false);

    // Left Leg — штаны
    auto& leftLeg = MakePart(charModel, "Left Leg", Shape::Block,
                              LG_R, LG_G, LG_B,
                              1.0f, 2.0f, 1.0f,
                              AX - 0.5f, HRP_CY - 1.0f, AZ,
                              true, false);

    // Right Leg — штаны
    auto& rightLeg = MakePart(charModel, "Right Leg", Shape::Block,
                               LG_R, LG_G, LG_B,
                               1.0f, 2.0f, 1.0f,
                               AX + 0.5f, HRP_CY - 1.0f, AZ,
                               true, false);

    // Left Arm — кожа
    auto& leftArm = MakePart(charModel, "Left Arm", Shape::Block,
                              SK_R, SK_G, SK_B,
                              1.0f, 2.0f, 1.0f,
                              AX - 1.5f, HRP_CY + 1.0f, AZ,
                              true, false);

    // Right Arm — кожа
    auto& rightArm = MakePart(charModel, "Right Arm", Shape::Block,
                               SK_R, SK_G, SK_B,
                               1.0f, 2.0f, 1.0f,
                               AX + 1.5f, HRP_CY + 1.0f, AZ,
                               true, false);

    // Head — сфера, цвет кожи
    //   Torso центр = HRP_CY + 1.0 = 3.75, Torso верх = 3.75 + 1.0 = 4.75
    //   Head радиус = 0.8 → Head центр = 4.75 + 0.8 = 5.55
    const float HEAD_CY = HRP_CY + 1.0f + 1.0f + 0.8f;  // 5.55
    auto& head = MakePart(charModel, "Head", Shape::Ball,
                           SK_R, SK_G, SK_B,
                           1.6f, 1.6f, 1.6f,
                           AX, HEAD_CY, AZ,
                           true, false);

    // Лицо на Head
    {
        auto& face = head.AddInstance("face", Decal::ClassId);
        face.SetProperty(Decal::Face,         PropertyValue::Int(4)); // Front
        face.SetProperty(Decal::Texture,      PropertyValue::String("PlatformContent/textures/EpicFace.dds"));
        face.SetProperty(Decal::Transparency, PropertyValue::Float(0.0f));
    }

    // PrimaryPart модели = HumanoidRootPart
    charModel.SetProperty(Model::PrimaryPart, PropertyValue::Ref(&hrp));

    // -----------------------------------------------------------------------
    //  Motor6D — соединения конечностей
    //
    //  Каждый мотор задаёт жёсткое соединение:
    //    Part0 = HumanoidRootPart  (опора)
    //    Part1 = конечность        (ведомая часть)
    //    C0    = смещение точки соединения от центра HRP
    //    C1    = смещение точки соединения от центра конечности
    //
    //  PhysicsBridge::SyncOut каждый кадр вычисляет:
    //    Part1.CFrame = Part0.CFrame * C0 * Inv(C1)
    //
    //  C0 можно менять каждый кадр для анимации (поворот конечности).
    //
    //  Оффсеты точки крепления от центра HRP (C0.Position):
    //    Torso    : ( 0.0,  +1.0,  0.0)  — верхняя половина HRP
    //    LeftLeg  : (-0.5,  -1.0,  0.0)  — нижняя-левая
    //    RightLeg : (+0.5,  -1.0,  0.0)  — нижняя-правая
    //    LeftArm  : (-1.5,  +1.0,  0.0)  — сбоку от торса слева
    //    RightArm : (+1.5,  +1.0,  0.0)  — сбоку от торса справа
    //    Head     : ( 0.0,  +2.8,  0.0)  — над торсом
    //
    //  C1 = Identity для всех — часть крепится своим центром.
    // -----------------------------------------------------------------------

    // Вспомогательная лямбда: создать Motor6D и добавить его в charModel
    // c0x/c0y/c0z — точка крепления в пространстве HRP (Part0)
    // c1x/c1y/c1z — точка крепления в пространстве конечности (Part1)
    //               Обычно верхний конец конечности, т.к. вращение идёт вокруг неё.
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

    //  R6 размеры частей тела:
    //    HRP      2x4x1  -> half-width=1.0, half-height=2.0
    //    Torso    2x2x1  -> half-height=1.0
    //    Leg      1x2x1  -> half-height=1.0
    //    Arm      1x2x1  -> half-height=1.0
    //    Head     r=0.8
    //
    //  C0 = точка сустава в локальном пространстве HRP
    //  C1 = та же точка в локальном пространстве конечности
    //
    //  HRP local Y: верх = +2.0, низ = -2.0
    //  HRP local X: лево = -1.0, право = +1.0

    // RootJoint: центр HRP (0,0,0) -> низ торса (0,-1,0)
    auto& motorTorso    = MakeMotor("RootJoint",     torso,
                                    0.0f,  0.0f, 0.0f,   // C0: центр HRP
                                    0.0f, -1.0f, 0.0f);  // C1: низ торса

    // LeftHip/RightHip: нижняя часть HRP -> верхний конец ноги
    auto& motorLeftLeg  = MakeMotor("LeftHip",       leftLeg,
                                   -0.5f,  0.0f, 0.0f,
                                    0.0f, +1.0f, 0.0f);  // C1: верх ноги

    auto& motorRightLeg = MakeMotor("RightHip",      rightLeg,
                                   +0.5f,  0.0f, 0.0f,
                                    0.0f, +1.0f, 0.0f);  // C1: верх ноги

    // LeftShoulder/RightShoulder
    // C0.X = ±2.0 компенсирует C1.X = ∓0.5 так что рука стоит у торса,
    // но вращается вокруг своего внутреннего верхнего угла.
    auto& motorLeftArm  = MakeMotor("LeftShoulder",  leftArm,
                                   -2.0f, +1.5f, 0.0f,
                                   -0.5f, +0.5f, 0.0f);

    auto& motorRightArm = MakeMotor("RightShoulder", rightArm,
                                   +2.0f, +1.5f, 0.0f,
                                   +0.5f, +0.5f, 0.0f);

    // Neck: верх HRP -> низ головы
    auto& motorHead     = MakeMotor("Neck",          head,
                                    0.0f, +2.0f, 0.0f,   // C0: верхняя грань HRP
                                    0.0f, -0.8f, 0.0f);  // C1: низ шара головы

    // -----------------------------------------------------------------------
    //  Камера: Follow, subject = Head
    // -----------------------------------------------------------------------
    camera.SetProperty(CurrentCamera::FieldOfView,     PropertyValue::Number(70.0));
    camera.SetProperty(CurrentCamera::CameraMode,      PropertyValue::CameraType(CameraType::Follow));
    camera.SetProperty(CurrentCamera::CameraSubject,   PropertyValue::Ref(&head));
    camera.SetProperty(CurrentCamera::MinZoomDistance, PropertyValue::Number(0.0));
    camera.SetProperty(CurrentCamera::MaxZoomDistance, PropertyValue::Number(50.0));
    camera.SetProperty(CurrentCamera::CFrame,          PropertyValue::CFrame(
        Sunvoltum::CFrame::FromPosition(AX, HEAD_CY + 3.0f, AZ - 12.0f)));

    // -----------------------------------------------------------------------
    //  Музыка
    // -----------------------------------------------------------------------
    auto& bgm = ws.AddInstance("BGM", Sound::ClassId);
    Sound::Init(bgm);
    bgm.SetProperty(Sound::SoundId, PropertyValue::String("PlatformContent/sounds/betterofalone.ogg"));
    bgm.SetProperty(Sound::Volume,  PropertyValue::Float(0.3f));
    bgm.SetProperty(Sound::Looped,  PropertyValue::Bool(true));
    bgm.SetProperty(Sound::Playing, PropertyValue::Bool(true));

    // -----------------------------------------------------------------------
    //  RenderBridge + Runtime
    // -----------------------------------------------------------------------
    RenderBridge renderBridge;
    if (!renderBridge.Init(engine, 1280, 720, "Sunvoltum — Character"))
        return -1;

    Runtime runtime;
    runtime.SetRenderBridge(&renderBridge);
    runtime.SetEngine(&engine);
    runtime.Input.SetCursorVisible(false);

    float clocktime  = 14.0f;
    float facingYaw  = 0.0f;   // направление взгляда персонажа (фиксируется при движении)
    bool  hrpLocked  = false;  // флаг: LockUpright вызван один раз после инициализации физики

    // PreSimulation — вызывается перед каждым физическим шагом.
    // Используем первый вызов чтобы поставить lock после инициализации PhysX.
    runtime.PreSimulation = [&](float /*fixedDt*/)
    {
        if (!hrpLocked && engine.Physics.IsInitialized())
        {
            engine.Physics.LockUpright(hrp);
            hrpLocked = true;
        }
    };

    runtime.RenderStepped = [&](float dt)
    {
        auto& input = runtime.Input;

        // Время суток
        clocktime += dt * 0.05f;
        if (clocktime > 24.0f) clocktime -= 24.0f;
        lighting.SetProperty(Lighting::ClockTime, PropertyValue::Number(clocktime));

        if (input.IsKeyPressed(KeyCode::Escape))
            runtime.Stop();

        // ----------------------------------------------------------------
        //  Управление через HumanoidRootPart
        //
        //  Горизонтальная скорость задаётся явно каждый кадр.
        //  Вертикальная (Y) берётся из текущей физики и не трогается,
        //  кроме момента прыжка.
        // ----------------------------------------------------------------
        const float WALK_SPEED = 16.0f;
        const float JUMP_VEL   = 48.0f;
        const float BOOST      = input.IsKeyDown(KeyCode::LeftShift) ? 2.0f : 1.0f;

        // Читаем текущую скорость HRP из DataModel (туда пишет PhysicsBridge::SyncOut)
        Sunvoltum::Vector3 vel = {0.0f, 0.0f, 0.0f};
        {
            auto* vp = hrp.GetProperty(ShapePart::PosVelocity);
            if (vp && vp->Type == PropertyType::Vector3)
                vel = vp->Value.AsVector3;
        }

        // Горизонтальные оси из yaw-угла орбиты камеры — только для ввода
        float cameraYaw = runtime.GetCameraYaw();
        Sunvoltum::Vector3 fwd   = {  std::sin(cameraYaw), 0.0f,  std::cos(cameraYaw) };
        Sunvoltum::Vector3 right = {  std::cos(cameraYaw), 0.0f, -std::sin(cameraYaw) };

        Sunvoltum::Vector3 moveDir = {0.0f, 0.0f, 0.0f};
        if (input.IsKeyDown(KeyCode::W)) { moveDir.X += fwd.X;   moveDir.Z += fwd.Z;   }
        if (input.IsKeyDown(KeyCode::S)) { moveDir.X -= fwd.X;   moveDir.Z -= fwd.Z;   }
        if (input.IsKeyDown(KeyCode::D)) { moveDir.X += right.X; moveDir.Z += right.Z; }
        if (input.IsKeyDown(KeyCode::A)) { moveDir.X -= right.X; moveDir.Z -= right.Z; }

        float moveLen = std::sqrt(moveDir.X * moveDir.X + moveDir.Z * moveDir.Z);
        bool  isMoving = moveLen > 0.001f;

        if (isMoving)
        {
            // Скорость — по реальному направлению ввода, сразу и точно
            float speed = WALK_SPEED * BOOST;
            vel.X = (moveDir.X / moveLen) * speed;
            vel.Z = (moveDir.Z / moveLen) * speed;

            // Поворот тела — плавно к целевому углу, только визуально,
            // не влияет на физику и скорость
            float targetYaw = std::atan2(moveDir.X, moveDir.Z);

            float diff = targetYaw - facingYaw;
            while (diff >  3.14159265f) diff -= 2.0f * 3.14159265f;
            while (diff < -3.14159265f) diff += 2.0f * 3.14159265f;

            const float TURN_SPEED = 14.0f; // рад/с — быстро но не мгновенно
            float step = TURN_SPEED * dt;
            if (std::abs(diff) < step)
                facingYaw = targetYaw;
            else
                facingYaw += (diff > 0.0f ? step : -step);
        }
        else
        {
            // Нет ввода — останавливаем горизонталь, взгляд не меняем
            vel.X = 0.0f;
            vel.Z = 0.0f;
        }

        // Raycast вниз от нижней грани HRP.
        // Старт на 0.05 выше нижней грани чтобы луч не начинался внутри shape HRP.
        // maxDist = 0.2 — небольшой зазор для неровной геометрии.
        bool isGrounded = false;
        if (engine.Physics.IsInitialized())
        {
            auto* cfp = hrp.GetProperty(ShapePart::CFrame);
            if (cfp && cfp->Type == PropertyType::CFrame)
            {
                const Sunvoltum::Vector3& hrpPos = cfp->Value.AsCFrame.Position;
                Sunvoltum::Vector3 rayOrigin(hrpPos.X,
                                             hrpPos.Y - HRP_H * 0.5f + 0.05f,
                                             hrpPos.Z);
                auto hit = engine.Physics.Raycast(
                    rayOrigin,
                    Sunvoltum::Vector3(0.0f, -1.0f, 0.0f),
                    0.2f,
                    &hrp);
                isGrounded = hit.Hit;
            }
        }
        if (input.IsKeyPressed(KeyCode::Space) && isGrounded)
            vel.Y = JUMP_VEL;

        // ----------------------------------------------------------------
        //  FP-режим (зум на минимуме): тело невидимо, facingYaw = cameraYaw.
        //  Курсор и захват мыши уже обрабатываются в Runtime::UpdateFollowCamera.
        // ----------------------------------------------------------------
        bool isFP = runtime.IsFirstPerson();

        if (isFP)
        {
            // Тело смотрит туда же куда камера — мгновенно, без интерполяции
            facingYaw = cameraYaw;
        }

        // Прозрачность частей тела: в FP-режиме всё скрыто
        {
            const float fp = isFP ? 1.0f : 0.0f;
            torso.SetProperty(   ShapePart::Transparency, PropertyValue::Float(fp));
            leftLeg.SetProperty( ShapePart::Transparency, PropertyValue::Float(fp));
            rightLeg.SetProperty(ShapePart::Transparency, PropertyValue::Float(fp));
            leftArm.SetProperty( ShapePart::Transparency, PropertyValue::Float(fp));
            rightArm.SetProperty(ShapePart::Transparency, PropertyValue::Float(fp));
            head.SetProperty(    ShapePart::Transparency, PropertyValue::Float(fp));
        }

        // ----------------------------------------------------------------
        //  Принудительно выравниваем ротацию HRP каждый кадр:
        //  - позицию берём из физики (PhysicsBridge::SyncOut уже записал)
        //  - ротацию ставим сами: чисто вокруг Y = facingYaw
        // ----------------------------------------------------------------
        {
            auto* cfp = hrp.GetProperty(ShapePart::CFrame);
            if (cfp && cfp->Type == PropertyType::CFrame)
            {
                Sunvoltum::CFrame upright(
                    cfp->Value.AsCFrame.Position,
                    Matrix3x3::FromEuler(0.0f, facingYaw, 0.0f));
                hrp.SetProperty(ShapePart::CFrame, PropertyValue::CFrame(upright));
            }
        }

        // Пишем скорость обратно — PhysicsBridge::SyncIn подхватит на следующем тике
        hrp.SetProperty(ShapePart::PosVelocity, PropertyValue::Vector3(vel));
        hrp.SetProperty(ShapePart::RotVelocity, PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));

        // ----------------------------------------------------------------
        //  Анимация через Motor6D.C0
        //
        //  isJumping  — персонаж в воздухе (vel.Y > порога)
        //  jumpArmAngle — плавно интерполируется к -75° при прыжке и обратно
        // ----------------------------------------------------------------
        static float walkPhase    = 0.0f;
        static float jumpArmAngle = 0.0f;
        static float headAngle    = 0.0f;

        // Танцевальный режим
        static bool  danceModeOn   = false;
        static float dancePhase    = 0.0f;
        static float danceSpeed    = 4.0f;  // рад/с, изменяется через +/-

        if (input.IsKeyPressed(KeyCode::U))
            danceModeOn = !danceModeOn;

        // + / - меняют скорость танца (только в танцевальном режиме)
        if (danceModeOn)
        {
            if (input.IsKeyPressed(KeyCode::O))
                danceSpeed = std::min(danceSpeed + 5.0f, 2000.0f);
            if (input.IsKeyPressed(KeyCode::P))
                danceSpeed = std::max(danceSpeed - 5.0f, 0.5f);
        }

        const float WALK_ANIM_SPEED  = 8.0f;
        const float WALK_ANIM_ANGLE  = 0.45f;
        const float ARM_ANIM_ANGLE   = 0.30f;
        const float IDLE_DECAY       = 6.0f;

        const float JUMP_ARM_TARGET  = -180.0f * 3.14159265f / 180.0f;
        const float JUMP_ARM_SPEED   = 10.0f;

        bool isJumping = !isGrounded;

        if (danceModeOn)
        {
            // В танцевальном режиме фаза идёт постоянно
            dancePhase += dt * danceSpeed;

            // Ноги ходят как обычно при движении
            if (isMoving) walkPhase += dt * WALK_ANIM_SPEED;
            if (!isMoving && walkPhase != 0.0f)
            {
                float decay = IDLE_DECAY * dt;
                walkPhase = (walkPhase > 0.0f)
                    ? std::max(0.0f, walkPhase - decay)
                    : std::min(0.0f, walkPhase + decay);
            }
            float legAngle = std::sin(walkPhase) * WALK_ANIM_ANGLE * (isMoving ? 1.0f : 0.0f);

            motorLeftLeg.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(-0.5f, 0.0f, 0.0f),
                                  Matrix3x3::FromEuler(-legAngle, 0.0f, 0.0f))));
            motorRightLeg.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(+0.5f, 0.0f, 0.0f),
                                  Matrix3x3::FromEuler(+legAngle, 0.0f, 0.0f))));

            // Руки крутятся в разные стороны по X (pitch)
            // Левая и правая в противофазе
            const float DANCE_ARM_ANGLE = 1.2f;
            float leftArmPitch  = std::sin(dancePhase)                  * DANCE_ARM_ANGLE;
            float rightArmPitch = std::sin(dancePhase + 3.14159265f)    * DANCE_ARM_ANGLE;

            motorLeftArm.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(-2.0f, +1.5f, 0.0f),
                                  Matrix3x3::FromEuler(leftArmPitch, 0.0f, 0.0f))));
            motorRightArm.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(+2.0f, +1.5f, 0.0f),
                                  Matrix3x3::FromEuler(rightArmPitch, 0.0f, 0.0f))));

            // Голова крутится по Y (yaw) — влево-вправо
            const float DANCE_HEAD_ANGLE = 0.6f;  // ~34°
            float headYaw = std::sin(dancePhase * 1.5f) * DANCE_HEAD_ANGLE;

            motorHead.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(0.0f, +2.0f, 0.0f),
                                  Matrix3x3::FromEuler(0.0f, headYaw, 0.0f))));
        }
        else
        {
            // Обычная анимация
            dancePhase = 0.0f;

            if (isMoving) walkPhase += dt * WALK_ANIM_SPEED;
            if (!isMoving && walkPhase != 0.0f)
            {
                float decay = IDLE_DECAY * dt;
                walkPhase = (walkPhase > 0.0f)
                    ? std::max(0.0f, walkPhase - decay)
                    : std::min(0.0f, walkPhase + decay);
            }

            // Угол рук при прыжке
            {
                float target = isJumping ? JUMP_ARM_TARGET : 0.0f;
                float diff   = target - jumpArmAngle;
                float step   = JUMP_ARM_SPEED * dt;
                if (std::abs(diff) <= step) jumpArmAngle = target;
                else jumpArmAngle += (diff > 0.0f ? step : -step);
            }

            float walkFactor = isJumping ? 0.0f : 1.0f;
            float legAngle = std::sin(walkPhase) * WALK_ANIM_ANGLE * (isMoving ? 1.0f : 0.0f);
            float armAngle = std::sin(walkPhase) * ARM_ANIM_ANGLE  * (isMoving ? walkFactor : 0.0f);

            motorLeftLeg.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(-0.5f, 0.0f, 0.0f),
                                  Matrix3x3::FromEuler(-legAngle, 0.0f, 0.0f))));
            motorRightLeg.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(+0.5f, 0.0f, 0.0f),
                                  Matrix3x3::FromEuler(+legAngle, 0.0f, 0.0f))));

            motorLeftArm.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(-2.0f, +1.5f, 0.0f),
                                  Matrix3x3::FromEuler(+armAngle + jumpArmAngle, 0.0f, 0.0f))));
            motorRightArm.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(+2.0f, +1.5f, 0.0f),
                                  Matrix3x3::FromEuler(-armAngle + jumpArmAngle, 0.0f, 0.0f))));

            motorHead.SetProperty(Motor6D::C0, PropertyValue::CFrame(
                Sunvoltum::CFrame(Sunvoltum::Vector3(0.0f, +2.0f, 0.0f),
                                  Matrix3x3::FromEuler(0.0f, 0.0f, 0.0f))));
        }

        // H — телепорт на точку спавна
        if (input.IsKeyPressed(KeyCode::H))
        {
            hrp.SetProperty(ShapePart::CFrame,
                PropertyValue::CFrame(Sunvoltum::CFrame::FromPosition(AX, HRP_CY, AZ)));
            hrp.SetProperty(ShapePart::PosVelocity, PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));
            hrp.SetProperty(ShapePart::RotVelocity, PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));
        }
    };

    runtime.Heartbeat = [&engine](float dt)
    {
        engine.Tick(dt);
        static double s_time = 0.0;
        s_time += static_cast<double>(dt);
        ServerScriptBridge::Get().StepScheduler(s_time);
    };

    runtime.Start();

    renderBridge.Shutdown();
    engine.Shutdown();
    ServerScriptBridge::Get().Shutdown();
    return 0;
}
