#include "SunoverEngine.h"

using namespace Sunover;
using namespace Sunover::Classes;

int main()
{
    // --- Engine ---
    Engine engine;
    engine.Init(EngineMode::Standalone);

    auto& dm = engine.DataModel;

    // Сцена
    auto& ws      = dm.AddInstance("Workspace",  Workspace::ClassId);
    auto& lighting = dm.AddInstance("Lighting",  Lighting::ClassId);
    auto& camera   = dm.AddInstance("Camera",    CurrentCamera::ClassId);
    auto& part     = dm.AddInstance("Block1",    ShapePart::ClassId, ws);

    ws.SetProperty(Workspace::Gravity,         PropertyValue::Float(9.8f));
    ws.SetProperty(Workspace::PhysicsEnabled,  PropertyValue::Bool(true), true);

    lighting.SetProperty(Lighting::Brightness,  PropertyValue::Number(1.0));
    lighting.SetProperty(Lighting::ClockTime,   PropertyValue::Number(14.0));

    camera.SetProperty(CurrentCamera::FieldOfView, PropertyValue::Number(70.0));
    camera.SetProperty(CurrentCamera::CameraMode,  PropertyValue::CameraType(CameraType::Follow));

    part.SetProperty(ShapePart::Shape,         PropertyValue::Shape(Shape::Block));
    part.SetProperty(ShapePart::Color,         PropertyValue::Color3({1.0f, 0.2f, 0.2f}));
    part.SetProperty(ShapePart::Transparency,  PropertyValue::Float(0.0f));

    // --- RenderBridge (только Standalone / Client) ---
    RenderBridge renderBridge;
    if (!renderBridge.Init(engine, 1280, 720, "Sunover"))
        return -1;

    // --- Runtime ---
    Runtime runtime;
    runtime.SetRenderBridge(&renderBridge);

    runtime.PreSimulation = [](float fixedDt)
    {
        // TODO: шаг физики
    };

    runtime.PostSimulation = [](float fixedDt)
    {
        // TODO: пост-обработка физики
    };

    runtime.Heartbeat = [&engine](float dt)
    {
        engine.Tick(dt);
        // TODO: сетевой тик, Luau VM
    };

    runtime.Start(); // блокирует до закрытия окна

    renderBridge.Shutdown();
    engine.Shutdown();
    return 0;
}
