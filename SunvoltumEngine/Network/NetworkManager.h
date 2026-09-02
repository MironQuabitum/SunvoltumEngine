#pragma once

#include <cstdint>
#include "../LibSunvoltum.h"
#include "../Types/EngineMode.h"

// Forward-declare uv_loop_s чтобы не тащить весь uv.h в заголовок
struct uv_loop_s;

namespace Sunvoltum {

    // NetworkManager — центральная точка сетевой подсистемы.
    //
    // Жизненный цикл:
    //   NetworkManager::Get().Init(EngineMode::Server);   // или Client
    //   ...в Heartbeat каждый кадр...
    //   NetworkManager::Get().Poll();
    //   NetworkManager::Get().Shutdown();
    //
    // В режиме Standalone Init() ничего не делает, Poll() — no-op.
    //
    // NetworkId — сквозной идентификатор подключённого клиента.
    // Сервер выдаёт его при установке соединения и записывает в
    // свойство Player::NetworkId. Через этот же Id реализуется
    // SetNetworkOwner(player): Instance помечается NetworkOwnerId = NetworkId
    // игрока, и сервер знает чьи данные принимать для этого объекта.

    using NetworkId = uint32_t;

    constexpr NetworkId INVALID_NETWORK_ID = 0;

    class LibSunvoltum NetworkManager
    {
    public:
        // Singleton
        static NetworkManager& Get();

        NetworkManager(const NetworkManager&)            = delete;
        NetworkManager& operator=(const NetworkManager&) = delete;

        // Инициализация: создаёт uv_loop, запоминает режим.
        // Для Server — вызвать Listen() после Init().
        // Для Client  — вызвать Connect() после Init().
        void Init(EngineMode mode);

        // Неблокирующий тик libuv (UV_RUN_NOWAIT).
        // Вызывать каждый кадр из Heartbeat.
        void Poll();

        // Освобождение ресурсов: закрывает все хендлы, останавливает loop.
        void Shutdown();

        // Текущий режим
        EngineMode GetMode() const;

        // Указатель на uv_loop_t — нужен NetworkServer / NetworkClient
        // для создания хендлов (tcp, timer и т.д.)
        uv_loop_s* GetLoop() const;

        // Выдать следующий уникальный NetworkId (только на сервере)
        NetworkId AllocNetworkId();

    private:
        NetworkManager() = default;
        ~NetworkManager() = default;

        EngineMode  m_mode        = EngineMode::Standalone;
        uv_loop_s*  m_loop        = nullptr;
        bool        m_initialized = false;
        NetworkId   m_nextId      = 1; // 0 зарезервирован как INVALID_NETWORK_ID
    };

} // namespace Sunvoltum
