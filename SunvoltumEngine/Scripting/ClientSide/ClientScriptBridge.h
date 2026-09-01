#pragma once

#include <string>
#include <istream>

#include "../../LibSunvoltum.h"
#include "../../DataModel/DataModel.h"
#include "../../DataModel/Instance.h"

namespace Sunvoltum {
namespace Scripting {
namespace Client {

    // ClientScriptBridge — заглушка для клиентской стороны.
    //
    // LocalScript-ы выполняются только на клиенте. Реализация будет добавлена
    // отдельно после разделения серверной и клиентской логики Engine.
    // Сейчас все методы — no-op, чтобы клиентский код мог уже вызывать их
    // без ошибок компиляции.
    class LibSunvoltum ClientScriptBridge
    {
    public:
        static ClientScriptBridge& Get();

        // Инициализировать клиентский скриптинг (не реализовано).
        void Init(DataModel* /*dataModel*/) { /* TODO */ }
        void Shutdown()                     { /* TODO */ }
        bool IsInitialized() const { return false; }

        // Загрузка исходников — пока возвращает -1 (неподдерживаемый Id).
        int LoadScript(std::istream& /*stream*/)            { return -1; }
        int LoadScriptFromSource(const std::string& /*src*/) { return -1; }

        // Заглушка запуска LocalScript — ничего не делает.
        void RunScript(int /*scriptId*/, Instance* /*scriptInst*/) { /* TODO */ }

    private:
        ClientScriptBridge() = default;
    };

} // namespace Client
} // namespace Scripting
} // namespace Sunvoltum
