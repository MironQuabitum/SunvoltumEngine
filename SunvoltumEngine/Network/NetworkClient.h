#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>

#include "../LibSunvoltum.h"
#include "NetworkManager.h"
#include "NetworkPacket.h"
#include "NetworkServer.h"   // AddrStorage

struct uv_udp_s;
struct uv_timer_s;

namespace Sunvoltum {
namespace Net {

    // -----------------------------------------------------------------------
    // ConnectionState — состояние подключения клиента
    // -----------------------------------------------------------------------
    enum class ConnectionState : uint8_t
    {
        Disconnected,   // нет активного соединения
        Connecting,     // отправлен Handshake, ждём HandshakeAck
        Connected,      // соединение установлено
    };

    // -----------------------------------------------------------------------
    // NetworkClient
    //
    // UDP-клиент на основе libuv.
    //
    // Использование:
    //   NetworkClient& cli = NetworkClient::Get();
    //   cli.SetOnConnected([&](NetworkId myId) { ... });
    //   cli.SetOnDisconnected([&]() { ... });
    //   cli.SetOnPacketReceived([&](PacketReader& r) { ... });
    //   cli.Connect("127.0.0.1", 7777, "PlayerName");
    //   // каждый кадр:
    //   cli.ResendPending();   // повторная отправка неподтверждённых пакетов
    //   cli.RetryHandshake();  // повторяет Handshake если нет ответа
    //   // NetworkManager::Get().Poll() запускает libuv
    //
    // -----------------------------------------------------------------------
    class LibSunvoltum NetworkClient
    {
    public:
        static NetworkClient& Get();

        NetworkClient(const NetworkClient&)            = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;

        // Начать подключение к серверу.
        // Отправляет Handshake-пакет и переходит в состояние Connecting.
        // Результат — через колбэк SetOnConnected.
        bool Connect(const std::string& host, uint16_t port,
                     const std::string& username);

        // Закрыть соединение (шлёт Disconnect серверу, освобождает ресурсы).
        void Disconnect();

        // Освободить все ресурсы (вызывать при завершении приложения).
        void Shutdown();

        // Повторная отправка неподтверждённых Reliable-пакетов.
        // Вызывать каждый кадр.
        void ResendPending();

        // Повторить Handshake если в состоянии Connecting и прошло достаточно времени.
        // Вызывать каждый кадр.
        void RetryHandshake();

        // Отправить пакет серверу
        void Send(const PacketWriter& pkt);

        // Reliable-отправка: сохраняет пакет до получения ACK
        void SendReliable(PacketWriter& pkt);

        // Текущее состояние подключения
        ConnectionState GetState() const;

        // NetworkId назначенный сервером (действителен только в Connected)
        NetworkId GetNetworkId() const;

        // Колбэки — устанавливаются до Connect()
        void SetOnConnected      (std::function<void(NetworkId)> cb);
        void SetOnDisconnected   (std::function<void()> cb);
        void SetOnPacketReceived (std::function<void(PacketReader&)> cb);

        // Внутренний колбэк libuv — не вызывать вручную
        void OnReceive(const uint8_t* data, size_t len);

    private:
        NetworkClient() = default;
        ~NetworkClient() = default;

        void HandleHandshakeAck(PacketReader& r);
        void HandleAck         (PacketReader& r);
        void HandleDisconnect  ();

        void RawSend(const uint8_t* data, size_t size);

        uv_udp_s* m_udp = nullptr;

        // Адрес сервера (IPv4, хранится в выровненном буфере AddrStorage)
        AddrStorage m_serverAddr    = {};
        int         m_serverAddrLen = 0;

        ConnectionState m_state     = ConnectionState::Disconnected;
        NetworkId       m_networkId = INVALID_NETWORK_ID;
        std::string     m_username;

        // Исходящий seq для Reliable-пакетов
        uint16_t m_outSeq = 0;

        // Время последней отправки Handshake (для повторов)
        std::chrono::steady_clock::time_point m_lastHandshakeSent;
        static constexpr int HANDSHAKE_RETRY_MS = 500;

        // Ожидающие ACK Reliable-пакеты: seq → (данные, время последней отправки)
        struct PendingReliable
        {
            std::vector<uint8_t> data;
            std::chrono::steady_clock::time_point lastSent;
        };
        std::unordered_map<uint16_t, PendingReliable> m_pendingAck;

        std::function<void(NetworkId)> m_onConnected;
        std::function<void()>          m_onDisconnected;
        std::function<void(PacketReader&)> m_onPacket;
    };

} // namespace Net
} // namespace Sunvoltum
