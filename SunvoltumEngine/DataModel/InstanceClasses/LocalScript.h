#pragma once

#include "../PropertyId.h"
#include "../Instance.h"
#include "../PropertyValue.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_LOCALSCRIPT = 10;

    // LocalScript — клиентский скрипт, выполняется только на стороне клиента.
    // В текущей версии является заглушкой: структура аналогична Script,
    // но запуск не реализован.
    struct LocalScript
    {
        static constexpr int8_t ClassId = CLASS_LOCALSCRIPT;

        // Id скрипта, зарегистрированного в ScriptBridge.
        static constexpr PropertyId ScriptId = 0; // Int

        // Включён ли скрипт.
        static constexpr PropertyId Disabled = 1; // Bool

        // Инициализировать свойства LocalScript значениями по умолчанию.
        static void Init(Instance& inst)
        {
            inst.SetProperty(ScriptId, PropertyValue::Int(0));
            inst.SetProperty(Disabled, PropertyValue::Bool(false));
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
    };

} // namespace Classes
} // namespace Sunvoltum
