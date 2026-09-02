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

struct uv_udp_s;
struct sockaddr;

namespace Sunvoltum {
namespace Net {

    // Непрозрачный буфер под sockaddr_storage (128 байт, выровнен по 8).
    // Позволяет хранить IPv4/IPv6 адрес без включения winsock2.h в заголовок.
    struct AddrStorage
    {
        alignas(8) uint8_t bytes[128] = {};
    };

} // namespace Net
} // namespace Sunvoltum

namespace Sunvoltum {
namespace Net {

    // -----------------------------------------------------------------------
    // ConnectedPeer — информация об одном подключённом клиенте
    // -----------------------------------------------------------------------
    struct ConnectedPeer
    {
        NetworkId   networkId  = INVALID_NETWORK_ID;
        std::string username;

        // Адрес клиента (IPv4, сохраняем в выровненном буфере AddrStorage)
        AddrStorage addr    = {};
        int         addrLen = 0;

        // Счётчик исходящих Reliable-seq для этого пира
        uint16_t outSeq = 0;

        // Ожидающие ACK Reliable-пакеты: seq → (данные, время последней отправки)
        struct PendingReliable
        {
            std::vector<uint8_t> data;
            std::chrono::steady_clock::time_point lastSent;
        };
        std::unordered_map<uint16_t, PendingReliable> pendingAck;
    };

    // -----------------------------------------------------------------------
    // NetworkServer
    //
    // UDP-сервер на основе libuv.
    //
    // Использование:
    //   NetworkServer& srv = NetworkServer::Get();
    //   srv.SetOnPeerConnected([&](NetworkId id, const std::string& name) { ... });
    //   srv.SetOnPeerDisconnected([&](NetworkId id) { ... });
    //   srv.Listen("0.0.0.0", 7777);
    //   // каждый кадр:
    //   srv.ResendPending();   // повторная отправка неподтверждённых пакетов
    //   // NetworkManager::Get().Poll() запускает libuv — прём пакеты там
    //
    // -----------------------------------------------------------------------
    class LibSunvoltum NetworkServer
    {
    public:
        static NetworkServer& Get();

        NetworkServer(const NetworkServer&)            = delete;
        NetworkServer& operator=(const NetworkServer&) = delete;

        // Открыть UDP-сокет и начать принимать датаграммы.
        // host = "0.0.0.0" для прослушивания на всех интерфейсах.
        // Возвращает true при успехе.
        bool Listen(const std::string& host, uint16_t port);

        // Закрыть сокет и очистить список пиров.
        void Shutdown();

        // Повторная отправка неподтверждённых Reliable-пакетов.
        // Вызывать каждый кадр вместе с NetworkManager::Get().Poll().
        void ResendPending();

        // Найти пира по NetworkId (nullptr если не найден)
        ConnectedPeer* FindPeer(NetworkId id);

        // Все подключённые пиры
        const std::unordered_map<NetworkId, ConnectedPeer>& GetPeers() const;

        // Отправить пакет конкретному пиру
        void SendTo(NetworkId id, const PacketWriter& pkt);

        // Отправить пакет напрямую по адресу (используется внутри колбэка приёма)
        void SendToAddr(const sockaddr* addr, int addrLen, const PacketWriter& pkt);

        // Reliable-отправка: сохраняет пакет до получения ACK
        void SendReliable(NetworkId id, PacketWriter& pkt);

        // Unreliable-отправка: fire-and-forget, без сохранения и повторов
        // Использовать для высокочастотных обновлений (PropertyUpdate)
        void SendUnreliableTo(NetworkId id, const PacketWriter& pkt);

        // Колбэки — устанавливаются до Listen()
        void SetOnPeerConnected   (std::function<void(NetworkId, const std::string& username)> cb);
        void SetOnPeerDisconnected(std::function<void(NetworkId)> cb);
        void SetOnPacketReceived  (std::function<void(NetworkId, PacketReader&)> cb);

        // Внутренний колбэк libuv — не вызывать вручную
        void OnReceive(const uint8_t* data, size_t len,
                       const sockaddr* addr, int addrLen);

    private:
        NetworkServer() = default;
        ~NetworkServer() = default;

        // Обработка конкретных типов пакетов
        void HandleHandshake  (PacketReader& r, const sockaddr* addr, int addrLen);
        void HandleAck        (PacketReader& r, NetworkId id);
        void HandleDisconnect (NetworkId id);
        void HandleUnknownPeer(const sockaddr* addr, int addrLen,
                               const uint8_t* data, size_t len);

        // Найти пира по адресу (nullptr если не найден)
        ConnectedPeer* FindPeerByAddr(const sockaddr* addr, int addrLen);

        // Отправить raw-буфер по адресу
        void RawSendToAddr(const sockaddr* addr, int addrLen,
                           const uint8_t* data, size_t size);

        uv_udp_s* m_udp = nullptr;

        std::unordered_map<NetworkId, ConnectedPeer> m_peers;

        std::function<void(NetworkId, const std::string&)> m_onConnected;
        std::function<void(NetworkId)>                     m_onDisconnected;
        std::function<void(NetworkId, PacketReader&)>      m_onPacket;
    };

} // namespace Net
} // namespace Sunvoltum
