#pragma once

namespace Sunover {

    enum class EngineMode
    {
        Standalone, // Одиночный режим (без сети)
        Client,     // Клиент — подключается к серверу
        Server      // Сервер — принимает подключения
    };

} // namespace Sunover
