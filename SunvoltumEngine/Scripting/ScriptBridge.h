#pragma once

// ScriptBridge — единая точка входа в систему скриптинга.
//
// Делегирует LoadScript/RunScript в нужный bridge в зависимости от контекста:
//   - EngineMode::Server → ServerScriptBridge
//   - EngineMode::Client → ClientScriptBridge (пока заглушка)
//
// Клиентский код движка должен работать только через ScriptBridge — не через
// ServerScriptBridge напрямую.

#include "ServerSide/ServerScriptBridge.h"
#include "ClientSide/ClientScriptBridge.h"

namespace Sunvoltum {

    // Удобные алиасы для использования в InstanceClasses/Script.h и вне движка
    using ServerScriptBridge = Scripting::Server::ServerScriptBridge;
    using ClientScriptBridge = Scripting::Client::ClientScriptBridge;
    using ScriptRunResult    = Scripting::Server::ScriptRunResult;

} // namespace Sunvoltum
