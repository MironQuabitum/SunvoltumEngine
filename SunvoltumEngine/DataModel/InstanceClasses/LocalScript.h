#pragma once

#include "../PropertyId.h"
#include "../Instance.h"
#include "../PropertyValue.h"
#include "../../Scripting/ClientSide/ClientScriptBridge.h"

#include <istream>
#include <string>

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_LOCALSCRIPT = 10;

    // LocalScript — клиентский скрипт, выполняется только на стороне клиента.
    // Запускается автоматически когда инстанс попадает в иерархию Workspace
    // (через ClientScriptBridge::WatchWorkspace).
    struct LocalScript
    {
        static constexpr int8_t ClassId = CLASS_LOCALSCRIPT;

        static constexpr PropertyId ScriptId = 0; // Int
        static constexpr PropertyId Disabled = 1; // Bool

        static void Init(Instance& inst)
        {
            inst.SetProperty(ScriptId, PropertyValue::Int(0));
            inst.SetProperty(Disabled, PropertyValue::Bool(false));
        }

        // Загрузить исходник в ClientScriptBridge и записать ScriptId в инстанс.
        static int LoadScriptFromSource(Instance& inst, const std::string& source)
        {
            int id = Scripting::Client::ClientScriptBridge::Get().LoadScriptFromSource(source);
            inst.SetProperty(ScriptId, PropertyValue::Int(id));
            return id;
        }

        static int LoadScript(Instance& inst, std::istream& stream)
        {
            int id = Scripting::Client::ClientScriptBridge::Get().LoadScript(stream);
            inst.SetProperty(ScriptId, PropertyValue::Int(id));
            return id;
        }

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
    };

} // namespace Classes
} // namespace Sunvoltum
