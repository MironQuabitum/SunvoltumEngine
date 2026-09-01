#pragma once

#include "../PropertyId.h"
#include "../Instance.h"
#include "../PropertyValue.h"
#include "../../Scripting/ServerSide/ServerScriptBridge.h"

#include <istream>

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_SCRIPT = 9;

    struct Script
    {
        static constexpr int8_t ClassId = CLASS_SCRIPT;

        // Id скрипта, зарегистрированного в ServerScriptBridge.
        // Устанавливается через LoadScript() — вручную не задаётся.
        static constexpr PropertyId ScriptId = 0; // Int

        // Включён ли скрипт. false — скрипт не будет запущен при старте.
        static constexpr PropertyId Disabled = 1; // Bool

        // Инициализировать свойства Script значениями по умолчанию.
        // Вызывается сразу после AddInstance.
        static void Init(Instance& inst)
        {
            inst.SetProperty(ScriptId, PropertyValue::Int(0));
            inst.SetProperty(Disabled, PropertyValue::Bool(false));
        }

        // Загрузить Luau-скрипт из потока, зарегистрировать в ServerScriptBridge
        // и сохранить полученный ScriptId в инстанс.
        // Возвращает присвоенный ScriptId.
        static int LoadScript(Instance& inst, std::istream& stream)
        {
            int id = Scripting::Server::ServerScriptBridge::Get().LoadScript(stream);
            inst.SetProperty(ScriptId, PropertyValue::Int(id));
            return id;
        }

        // Загрузить Luau-скрипт из строки с исходным кодом.
        // Возвращает присвоенный ScriptId.
        static int LoadScriptFromSource(Instance& inst, const std::string& source)
        {
            int id = Scripting::Server::ServerScriptBridge::Get().LoadScriptFromSource(source);
            inst.SetProperty(ScriptId, PropertyValue::Int(id));
            return id;
        }

        // Геттеры

        static int GetScriptId(const Instance& inst)
        {
            const auto* p = inst.GetProperty(ScriptId);
            return (p && p->Type == PropertyType::Int) ? p->Value.AsInt : 0;
        }

        static bool IsDisabled(const Instance& inst)
        {
            const auto* p = inst.GetProperty(Disabled);
            return (p && p->Type == PropertyType::Bool) ? p->Value.AsBool : false;
        }

        // Получить исходный код скрипта из ServerScriptBridge по ScriptId инстанса.
        static const std::string& GetSource(const Instance& inst)
        {
            return Scripting::Server::ServerScriptBridge::Get().GetSource(GetScriptId(inst));
        }
    };

} // namespace Classes
} // namespace Sunvoltum
