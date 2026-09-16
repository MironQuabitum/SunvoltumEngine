#pragma once

// ClientBindings.h
// Регистрация клиентских глобалов Luau VM:
//   UserInputService  — ввод (клавиатура, мышь)
//   RunService        — RenderStepped / Heartbeat события

struct lua_State;

namespace Sunvoltum {
    class IInputSource;
}

namespace Sunvoltum {
namespace Scripting {
namespace Client {

    // Зарегистрировать UserInputService, RunService и Enum в Luau VM.
    void RegisterClientBindings(lua_State* L, IInputSource* input);

    // Вызвать все RunService.RenderStepped коннекты с аргументом dt.
    void FireRenderStepped(lua_State* L, double dt);

    // Вызвать все RunService.Heartbeat коннекты с аргументом dt.
    void FireHeartbeat(lua_State* L, double dt);

    // Опросить ввод и сгенерировать InputBegan/InputEnded события.
    // Вызывать один раз за кадр перед FireRenderStepped.
    void FireInputEvents(lua_State* L, IInputSource* input);

} // namespace Client
} // namespace Scripting
} // namespace Sunvoltum
