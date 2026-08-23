#include "SunoverEngine.h"
#include "Rendering/RenderBridge.h"
#include <iostream>
using namespace Sunover;
using namespace Sunover::Classes;

int main()
{
    Engine engine;
    engine.Init(EngineMode::Standalone);

    auto& dm = engine.DataModel;

    // --- Сервисные объекты ---
    auto& ws       = dm.AddInstance("Workspace",  Workspace::ClassId);
    auto& lighting = dm.AddInstance("Lighting",   Lighting::ClassId);
    auto& camera   = dm.AddInstance("Camera",     CurrentCamera::ClassId);

    ws.SetProperty(Workspace::Gravity,        PropertyValue::Float(196.2f)); // 196.2 studs/s² = 9.81 m/s²
    ws.SetProperty(Workspace::PhysicsEnabled, PropertyValue::Bool(true), true);

    lighting.SetProperty(Lighting::Brightness,         PropertyValue::Number(2.0));
    lighting.SetProperty(Lighting::ClockTime,          PropertyValue::Number(14.0));
    lighting.SetProperty(Lighting::GeographicLatitude, PropertyValue::Number(45.0));
    lighting.SetProperty(Lighting::UseDefaultSky,      PropertyValue::Bool(true));

    camera.SetProperty(CurrentCamera::FieldOfView, PropertyValue::Number(60.0));
    camera.SetProperty(CurrentCamera::CameraMode,  PropertyValue::CameraType(CameraType::Scriptable));
    camera.SetProperty(CurrentCamera::CFrame,      PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(5.0f, 3.0f, -20.0f)
    ));

    // -----------------------------------------------------------------------
    // Пол: Size 95 x 1.5 x 95, верхняя грань = 1.5/2 = 0.75
    // -----------------------------------------------------------------------
    auto& floor = dm.AddInstance("Floor", ShapePart::ClassId, ws);
    floor.SetProperty(ShapePart::Shape,        PropertyValue::Shape(Shape::Block));
    floor.SetProperty(ShapePart::Color,        PropertyValue::Color3({0.6f, 0.65f, 0.7f}));
    floor.SetProperty(ShapePart::Transparency, PropertyValue::Float(0.0f));
    floor.SetProperty(ShapePart::Reflectance,  PropertyValue::Float(0.05f));
    floor.SetProperty(ShapePart::Size,         PropertyValue::Vector3({95.0f, 1.5f, 95.0f}));
    floor.SetProperty(ShapePart::CFrame,       PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(0.0f, 0.0f, 0.0f)
    ));
    floor.SetProperty(ShapePart::Anchored,     PropertyValue::Bool(true));
    floor.SetProperty(ShapePart::CanCollide,   PropertyValue::Bool(true));
    floor.SetProperty(ShapePart::PosVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));
    floor.SetProperty(ShapePart::RotVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));

    // Трава на верхней грани пола
    auto& floorGrass = floor.AddInstance("GrassSurface", TextureSurface::ClassId);
    floorGrass.SetProperty(TextureSurface::Face,          PropertyValue::Int(0)); // Top
    floorGrass.SetProperty(TextureSurface::Texture,       PropertyValue::String("PlatformContent/textures/grass/grass.dds"));
    floorGrass.SetProperty(TextureSurface::StudsPerTileU, PropertyValue::Float(2.25f));
    floorGrass.SetProperty(TextureSurface::StudsPerTileV, PropertyValue::Float(2.25f));

    // -----------------------------------------------------------------------
    // Сфера 1: Size 4x4x4, центр Y = 0.75 + 4.0/2 = 2.75
    // -----------------------------------------------------------------------
    auto& sphere1 = dm.AddInstance("Sphere1", ShapePart::ClassId, ws);
    sphere1.SetProperty(ShapePart::Shape,        PropertyValue::Shape(Shape::Ball));
    sphere1.SetProperty(ShapePart::Color,        PropertyValue::Color3({0.6f, 0.65f, 0.7f}));
    sphere1.SetProperty(ShapePart::Transparency, PropertyValue::Float(0.0f));
    sphere1.SetProperty(ShapePart::Size,         PropertyValue::Vector3({4.0f, 4.0f, 4.0f}));
    sphere1.SetProperty(ShapePart::CFrame,       PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(-1.0f, 2.75f, -3.0f)
    ));
    sphere1.SetProperty(ShapePart::Anchored,     PropertyValue::Bool(false));
    sphere1.SetProperty(ShapePart::CanCollide,   PropertyValue::Bool(true));
    sphere1.SetProperty(ShapePart::PosVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));
    sphere1.SetProperty(ShapePart::RotVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));

    // EpicFace на Front грани
    auto& decal1 = sphere1.AddInstance("EpicFace_Front", Decal::ClassId);
    decal1.SetProperty(Decal::Face,         PropertyValue::Int(4)); // Front
    decal1.SetProperty(Decal::Texture,      PropertyValue::String("PlatformContent/textures/EpicFace.dds"));
    decal1.SetProperty(Decal::Transparency, PropertyValue::Float(0.0f));

    // -----------------------------------------------------------------------
    // Сфера 2: Size 4x4x4, смещена по Z
    // -----------------------------------------------------------------------
    auto& sphere2 = dm.AddInstance("Sphere2", ShapePart::ClassId, ws);
    sphere2.SetProperty(ShapePart::Shape,        PropertyValue::Shape(Shape::Ball));
    sphere2.SetProperty(ShapePart::Color,        PropertyValue::Color3({0.6f, 0.65f, 0.7f}));
    sphere2.SetProperty(ShapePart::Transparency, PropertyValue::Float(0.0f));
    sphere2.SetProperty(ShapePart::Size,         PropertyValue::Vector3({4.0f, 4.0f, 4.0f}));
    sphere2.SetProperty(ShapePart::CFrame,       PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(0.0f, 5.75f, -2.0f)
    ));
    sphere2.SetProperty(ShapePart::Anchored,     PropertyValue::Bool(false));
    sphere2.SetProperty(ShapePart::CanCollide,   PropertyValue::Bool(true));
    sphere2.SetProperty(ShapePart::PosVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));
    sphere2.SetProperty(ShapePart::RotVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));

    // EpicFace на Back грани
    auto& decal2 = sphere2.AddInstance("EpicFace_Back", Decal::ClassId);
    decal2.SetProperty(Decal::Face,         PropertyValue::Int(5)); // Back
    decal2.SetProperty(Decal::Texture,      PropertyValue::String("PlatformContent/textures/EpicFace.dds"));
    decal2.SetProperty(Decal::Transparency, PropertyValue::Float(0.0f));

    // -----------------------------------------------------------------------
    // Стена: Size 16x16x4, полупрозрачная
    // -----------------------------------------------------------------------
    auto& wall = dm.AddInstance("Wall", ShapePart::ClassId, ws);
    wall.SetProperty(ShapePart::Shape,        PropertyValue::Shape(Shape::Block));
    wall.SetProperty(ShapePart::Color,        PropertyValue::Color3({0.7f, 0.7f, 0.75f}));
    wall.SetProperty(ShapePart::Transparency, PropertyValue::Float(0.29f));
    wall.SetProperty(ShapePart::Reflectance,  PropertyValue::Float(0.1f));
    wall.SetProperty(ShapePart::Size,         PropertyValue::Vector3({16.0f, 16.0f, 4.0f}));
    wall.SetProperty(ShapePart::CFrame,       PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(0.0f, 8.5f, -3.0f)
    ));
    wall.SetProperty(ShapePart::Anchored,     PropertyValue::Bool(true));
    wall.SetProperty(ShapePart::CanCollide,   PropertyValue::Bool(false));
    wall.SetProperty(ShapePart::PosVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));
    wall.SetProperty(ShapePart::RotVelocity,  PropertyValue::Vector3({0.0f, 0.0f, 0.0f}));

    // -----------------------------------------------------------------------
    // RenderBridge + Runtime
    // -----------------------------------------------------------------------
    RenderBridge renderBridge;
    if (!renderBridge.Init(engine, 1280, 720, "Sunover"))
        return -1;

    Runtime runtime;
    runtime.SetRenderBridge(&renderBridge);
    runtime.SetEngine(&engine);
    runtime.Input.SetCursorVisible(false);

    float camYaw   = 0.0f;
    float camPitch = 0.0f;
    float camX     = 5.0f;
    float camY     = 3.0f;
    float camZ     = -20.0f;
    float rotTime  = 0.0f;

    runtime.RenderStepped = [&](float dt)
    {
        auto& input = runtime.Input;
        rotTime += dt;

        if (input.IsMousePressed(MouseButton::Right))
        {
            input.SetMouseLocked(true);
            input.ResetMouseDelta();
        }
        if (input.IsMouseReleased(MouseButton::Right))
            input.SetMouseLocked(false);

        if (input.IsKeyPressed(KeyCode::Escape))
            runtime.Stop();

        // U — применить линейный и угловой импульс к Sphere2
        if (input.IsKeyPressed(KeyCode::U))
        {
            ShapePart::ApplyImpulse(sphere2,         Vector3(0.0f, 100.0f, 200.0f));
            ShapePart::ApplyRotationImpulse(sphere2, Vector3(0.0f,  20.0f,   0.0f));
        }

        if (input.IsMouseLocked())
        {
            const float SENS  = 0.002618f; // 0.15 deg/px в радианах
            const float LIMIT = 1.5533f;   // ~89 градусов

            camYaw   += static_cast<float>(input.GetMouseDeltaX()) * SENS;
            camPitch += static_cast<float>(input.GetMouseDeltaY()) * SENS;
            if (camPitch >  LIMIT) camPitch =  LIMIT;
            if (camPitch < -LIMIT) camPitch = -LIMIT;
            input.ResetMouseDelta();

            float speed = 10.0f * dt;
            if (input.IsKeyDown(KeyCode::LeftShift)) speed *= 3.0f;

            Matrix3x3 rot     = Matrix3x3::FromEuler(camPitch, camYaw, 0.0f);
            Vector3   forward = rot * Vector3(0.0f, 0.0f, 1.0f);
            Vector3   right   = rot * Vector3(1.0f, 0.0f, 0.0f);

            if (input.IsKeyDown(KeyCode::W)) { camX += forward.X*speed; camY += forward.Y*speed; camZ += forward.Z*speed; }
            if (input.IsKeyDown(KeyCode::S)) { camX -= forward.X*speed; camY -= forward.Y*speed; camZ -= forward.Z*speed; }
            if (input.IsKeyDown(KeyCode::D)) { camX += right.X*speed;   camZ += right.Z*speed; }
            if (input.IsKeyDown(KeyCode::A)) { camX -= right.X*speed;   camZ -= right.Z*speed; }
            if (input.IsKeyDown(KeyCode::Space)) {
                auto* s1vel = sphere1.GetProperty(ShapePart::PosVelocity);
                auto* s1rot = sphere1.GetProperty(ShapePart::RotVelocity);
                auto* s2vel = sphere2.GetProperty(ShapePart::PosVelocity);
                auto* s2rot = sphere2.GetProperty(ShapePart::RotVelocity);
                std::cout << "Velocities:"
                    << " Sphere1 pos=" << (s1vel ? s1vel->Value.AsVector3 : Sunover::Vector3{})
                    << " rot="         << (s1rot ? s1rot->Value.AsVector3 : Sunover::Vector3{})
                    << " | Sphere2 pos=" << (s2vel ? s2vel->Value.AsVector3 : Sunover::Vector3{})
                    << " rot="           << (s2rot ? s2rot->Value.AsVector3 : Sunover::Vector3{})
                    << std::endl;
            }
            if (input.IsKeyDown(KeyCode::E)) camY += speed;
            if (input.IsKeyDown(KeyCode::Q)) camY -= speed;

            camera.SetProperty(Classes::CurrentCamera::CFrame, PropertyValue::CFrame(
                Sunover::CFrame(Vector3(camX, camY, camZ),
                                Matrix3x3::FromEuler(camPitch, camYaw, 0.0f))
            ));
        }

        // Вращение сфер: 45 deg/sec = 0.7854 рад/сек

    };

    runtime.Heartbeat = [&engine](float dt)
    {
        engine.Tick(dt);
    };

    runtime.Start();

    renderBridge.Shutdown();
    engine.Shutdown();
    return 0;
}
