#include "SunoverEngine.h"
#include "Rendering/RenderBridge.h"

using namespace Sunover;
using namespace Sunover::Classes;

int main()
{
    Engine engine;
    engine.Init(EngineMode::Standalone);

    auto& dm = engine.DataModel;

    // --- Сервисы ---
    auto& ws      = dm.AddInstance("Workspace",     Workspace::ClassId);
    auto& lighting = dm.AddInstance("Lighting",     Lighting::ClassId);
    auto& camera   = dm.AddInstance("Camera",       CurrentCamera::ClassId);

    // --- Объекты сцены ---
    auto& floor    = dm.AddInstance("Floor",        ShapePart::ClassId);
    auto& sphere1  = dm.AddInstance("Sphere1",      ShapePart::ClassId);
    auto& sphere2  = dm.AddInstance("Sphere2",      ShapePart::ClassId);
    auto& wall     = dm.AddInstance("Wall",         ShapePart::ClassId);

    // -----------------------------------------------------------------------
    // Workspace
    // -----------------------------------------------------------------------
    ws.SetProperty(Workspace::Gravity,        PropertyValue::Float(9.8f));
    ws.SetProperty(Workspace::PhysicsEnabled, PropertyValue::Bool(true), true);

    // -----------------------------------------------------------------------
    // Lighting
    //   ClockTime 14.0 → угол солнца ~50° — соответствует MeturmDemo
    // -----------------------------------------------------------------------
    lighting.SetProperty(Lighting::Brightness,         PropertyValue::Number(2.0));
    lighting.SetProperty(Lighting::ClockTime,          PropertyValue::Number(14.0));
    lighting.SetProperty(Lighting::GeographicLatitude, PropertyValue::Number(45.0));
    lighting.SetProperty(Lighting::UseDefaultSky,      PropertyValue::Bool(true));

    // -----------------------------------------------------------------------
    // Camera
    //   Позиция (5, 3, -20), смотрим чуть влево — как в MeturmDemo
    // -----------------------------------------------------------------------
    camera.SetProperty(CurrentCamera::FieldOfView, PropertyValue::Number(60.0));
    camera.SetProperty(CurrentCamera::CameraMode,  PropertyValue::CameraType(CameraType::Follow));
    camera.SetProperty(CurrentCamera::CFrame,      PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(5.0f, 3.0f, -20.0f)
    ));

    // -----------------------------------------------------------------------
    // Floor  — центр (0, 0, 0), Size 25 × 1.5 × 25
    //   Верхняя грань: 0 + 1.5/2 = 0.75
    // -----------------------------------------------------------------------
    floor.SetProperty(ShapePart::Shape,        PropertyValue::Shape(Shape::Block));
    floor.SetProperty(ShapePart::Color,        PropertyValue::Color3({0.6f, 0.65f, 0.7f}));
    floor.SetProperty(ShapePart::Transparency, PropertyValue::Float(0.0f));
    floor.SetProperty(ShapePart::Reflectance,  PropertyValue::Float(0.05f));
    floor.SetProperty(ShapePart::Size,         PropertyValue::Vector3({25.0f, 1.5f, 25.0f}));
    floor.SetProperty(ShapePart::CFrame,       PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(0.0f, 0.0f, 0.0f)
    ));

    // Трава на верхней грани пола
    auto& floorGrass = floor.AddInstance("GrassSurface", TextureSurface::ClassId);
    floorGrass.SetProperty(TextureSurface::Texture,       PropertyValue::String("PlatformContent/textures/grass/grass.dds"));
    floorGrass.SetProperty(TextureSurface::Face,          PropertyValue::Int(0));   // Top
    floorGrass.SetProperty(TextureSurface::StudsPerTileU, PropertyValue::Float(2.25f));
    floorGrass.SetProperty(TextureSurface::StudsPerTileV, PropertyValue::Float(2.25f));

    // -----------------------------------------------------------------------
    // Sphere1  — стоит на полу, центр Y = 0.75 + 4.0/2 = 2.75
    //   Декаль EpicFace на Front-грани
    // -----------------------------------------------------------------------
    sphere1.SetProperty(ShapePart::Shape,        PropertyValue::Shape(Shape::Ball));
    sphere1.SetProperty(ShapePart::Color,        PropertyValue::Color3({0.6f, 0.65f, 0.7f}));
    sphere1.SetProperty(ShapePart::Transparency, PropertyValue::Float(0.0f));
    sphere1.SetProperty(ShapePart::Size,         PropertyValue::Vector3({4.0f, 4.0f, 4.0f}));
    sphere1.SetProperty(ShapePart::CFrame,       PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(0.0f, 2.75f, 0.0f)
    ));

    auto& decal1 = sphere1.AddInstance("EpicFace_Front", Decal::ClassId);
    decal1.SetProperty(Decal::Texture,      PropertyValue::String("PlatformContent/textures/EpicFace.dds"));
    decal1.SetProperty(Decal::Face,         PropertyValue::Int(4)); // Front
    decal1.SetProperty(Decal::Transparency, PropertyValue::Float(0.0f));

    // -----------------------------------------------------------------------
    // Sphere2  — смещена по Z, центр Y = 2.75
    //   Декаль EpicFace на Back-грани
    // -----------------------------------------------------------------------
    sphere2.SetProperty(ShapePart::Shape,        PropertyValue::Shape(Shape::Ball));
    sphere2.SetProperty(ShapePart::Color,        PropertyValue::Color3({0.6f, 0.65f, 0.7f}));
    sphere2.SetProperty(ShapePart::Transparency, PropertyValue::Float(0.0f));
    sphere2.SetProperty(ShapePart::Size,         PropertyValue::Vector3({4.0f, 4.0f, 4.0f}));
    sphere2.SetProperty(ShapePart::CFrame,       PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(0.0f, 2.75f, -6.0f)
    ));

    auto& decal2 = sphere2.AddInstance("EpicFace_Back", Decal::ClassId);
    decal2.SetProperty(Decal::Texture,      PropertyValue::String("PlatformContent/textures/EpicFace.dds"));
    decal2.SetProperty(Decal::Face,         PropertyValue::Int(5)); // Back
    decal2.SetProperty(Decal::Transparency, PropertyValue::Float(0.0f));

    // -----------------------------------------------------------------------
    // Wall  — прозрачная стена позади шаров
    //   Центр (0, 8.5, -3), Scale 16 × 16 × 4, Transparency 0.29
    // -----------------------------------------------------------------------
    wall.SetProperty(ShapePart::Shape,        PropertyValue::Shape(Shape::Block));
    wall.SetProperty(ShapePart::Color,        PropertyValue::Color3({0.6f, 0.65f, 0.7f}));
    wall.SetProperty(ShapePart::Transparency, PropertyValue::Float(0.29f));
    wall.SetProperty(ShapePart::Reflectance,  PropertyValue::Float(0.1f));
    wall.SetProperty(ShapePart::Size,         PropertyValue::Vector3({16.0f, 16.0f, 4.0f}));
    wall.SetProperty(ShapePart::CFrame,       PropertyValue::CFrame(
        Sunover::CFrame::FromPosition(0.0f, 8.5f, -3.0f)
    ));

    // -----------------------------------------------------------------------
    // RenderBridge + Runtime
    // -----------------------------------------------------------------------
    RenderBridge renderBridge;
    if (!renderBridge.Init(engine, 1280, 720, "Sunover"))
        return -1;

    Runtime runtime;
    runtime.SetRenderBridge(&renderBridge);

    runtime.PreSimulation = [](float fixedDt)
    {
        // TODO: физика
    };

    runtime.Heartbeat = [&engine, &sphere1](float dt)
    {
        auto* cfProp = sphere1.GetProperty(ShapePart::CFrame);
        if (cfProp && cfProp->Type == PropertyType::CFrame)
        {
            auto cframe = cfProp->Value.AsCFrame;
            auto pos    = cframe.Position;
            pos.Y += 0.5f * dt;

            // Инкрементальный поворот вокруг X — умножаем матрицы,
            // не конвертируем через ToEuler (избегаем gimbal lock)
            auto rot = cframe.Rotation * Matrix3x3::RotateY(1.0f * dt);

            sphere1.SetProperty(ShapePart::CFrame, PropertyValue::CFrame(
                Sunover::CFrame(pos, rot)
            ));
        }
        engine.Tick(dt);
    };

    runtime.Start();

    renderBridge.Shutdown();
    engine.Shutdown();
    return 0;
}
