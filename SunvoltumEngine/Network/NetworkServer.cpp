#include "NetworkServer.h"

#include <uv.h>
#include <cstring>
#include <iostream>
#include <cassert>

namespace Sunvoltum {
namespace Net {

    // -----------------------------------------------------------------------
    // Вспомогательная структура для uv_buf_t (владеет памятью)
    // -----------------------------------------------------------------------
    struct SendRequest
    {
        uv_udp_send_t  req   = {};
        uint8_t        data[MAX_PACKET_SIZE] = {};
        size_t         size  = 0;
    };

    // -----------------------------------------------------------------------
    // Singleton
    // -----------------------------------------------------------------------

    NetworkServer& NetworkServer::Get()
    {
        static NetworkServer instance;
        return instance;
    }

    // -----------------------------------------------------------------------
    // Listen
    // -----------------------------------------------------------------------

    bool NetworkServer::Listen(const std::string& host, uint16_t port)
    {
        uv_loop_t* loop = NetworkManager::Get().GetLoop();
        if (!loop)
        {
            std::cerr << "[NetworkServer] NetworkManager not initialized\n";
            return false;
        }

        m_udp = new uv_udp_t();
        if (uv_udp_init(loop, m_udp) != 0)
        {
            std::cerr << "[NetworkServer] uv_udp_init failed\n";
            delete m_udp;
            m_udp = nullptr;
            return false;
        }

        // Сохраняем указатель на this в data хендла для колбэков
        m_udp->data = this;

        // Разрешаем адрес
        struct sockaddr_in addr4 = {};
        if (uv_ip4_addr(host.c_str(), port, &addr4) != 0)
        {
            std::cerr << "[NetworkServer] Invalid address: " << host << ":" << port << "\n";
            uv_close(reinterpret_cast<uv_handle_t*>(m_udp), [](uv_handle_t* h) {
                delete reinterpret_cast<uv_udp_t*>(h);
            });
            m_udp = nullptr;
            return false;
        }

        if (uv_udp_bind(m_udp, reinterpret_cast<const sockaddr*>(&addr4), 0) != 0)
        {
            std::cerr << "[NetworkServer] uv_udp_bind failed on port " << port << "\n";
            uv_close(reinterpret_cast<uv_handle_t*>(m_udp), [](uv_handle_t* h) {
                delete reinterpret_cast<uv_udp_t*>(h);
            });
            m_udp = nullptr;
            return false;
        }

        // Начинаем принимать датаграммы
        // Колбэк выделения буфера
        auto alloc_cb = [](uv_handle_t* /*handle*/, size_t suggested_size, uv_buf_t* buf)
        {
            buf->base = new char[suggested_size];
            buf->len  = static_cast<ULONG>(suggested_size);
        };

        // Колбэк приёма датаграммы
        auto recv_cb = [](uv_udp_t* handle,
                          ssize_t nread,
                          const uv_buf_t* buf,
                          const struct sockaddr* addr,
                          unsigned /*flags*/)
        {
            auto* srv = reinterpret_cast<NetworkServer*>(handle->data);

            if (nread > 0 && addr != nullptr)
            {
                srv->OnReceive(
                    reinterpret_cast<const uint8_t*>(buf->base),
                    static_cast<size_t>(nread),
                    addr,
                    sizeof(struct sockaddr_in)
                );
            }

            delete[] buf->base;
        };

        if (uv_udp_recv_start(m_udp, alloc_cb, recv_cb) != 0)
        {
            std::cerr << "[NetworkServer] uv_udp_recv_start failed\n";
            uv_close(reinterpret_cast<uv_handle_t*>(m_udp), [](uv_handle_t* h) {
                delete reinterpret_cast<uv_udp_t*>(h);
            });
            m_udp = nullptr;
            return false;
        }

        std::cout << "[NetworkServer] Listening on " << host << ":" << port << " (UDP)\n";
        return true;
    }

    // -----------------------------------------------------------------------
    // Shutdown
    // -----------------------------------------------------------------------

    void NetworkServer::Shutdown()
    {
        if (m_udp)
        {
            uv_udp_recv_stop(m_udp);
            uv_close(reinterpret_cast<uv_handle_t*>(m_udp), [](uv_handle_t* h) {
                delete reinterpret_cast<uv_udp_t*>(h);
            });
            m_udp = nullptr;
        }
        m_peers.clear();
        std::cout << "[NetworkServer] Shutdown.\n";
    }

    // -----------------------------------------------------------------------
    // ResendPending — повторная отправка Reliable-пакетов без ACK
    // -----------------------------------------------------------------------

    void NetworkServer::ResendPending()
    {
        auto now = std::chrono::steady_clock::now();

        for (auto& [id, peer] : m_peers)
        {
            for (auto& [seq, pending] : peer.pendingAck)
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - pending.lastSent).count();

                if (elapsed >= RELIABLE_RESEND_MS)
                {
                    RawSendToAddr(
                        reinterpret_cast<const sockaddr*>(&peer.addr.bytes),
                        peer.addrLen,
                        pending.data.data(),
                        pending.data.size()
                    );
                    pending.lastSent = now;
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // OnReceive — вызывается из libuv-колбэка
    // -----------------------------------------------------------------------

    void NetworkServer::OnReceive(const uint8_t* data, size_t len,
                                  const sockaddr* addr, int addrLen)
    {
        if (len < HEADER_SIZE)
            return;

        PacketReader reader(data, len);
        if (!reader.Ok())
            return;

        PacketType type = reader.Type();

        // Handshake может прийти от неизвестного адреса
        if (type == PacketType::Handshake)
        {
            HandleHandshake(reader, addr, addrLen);
            return;
        }

        // Все остальные пакеты — только от зарегистрированных пиров
        ConnectedPeer* peer = FindPeerByAddr(addr, addrLen);
        if (!peer)
        {
            // Неизвестный отправитель — игнорируем
            return;
        }

        NetworkId id = peer->networkId;

        // Если Reliable — всегда шлём ACK обратно
        if (reader.Header().IsReliable())
        {
            PacketWriter ack(PacketType::Ack);
            ack.WriteU16(reader.Header().seq);
            RawSendToAddr(addr, addrLen, ack.Data(), ack.Size());
        }

        switch (type)
        {
        case PacketType::Ack:
            HandleAck(reader, id);
            break;

        case PacketType::Disconnect:
            HandleDisconnect(id);
            break;

        default:
            // Передаём в пользовательский колбэк
            if (m_onPacket)
                m_onPacket(id, reader);
            break;
        }
    }

    // -----------------------------------------------------------------------
    // HandleHandshake
    // -----------------------------------------------------------------------

    void NetworkServer::HandleHandshake(PacketReader& r,
                                        const sockaddr* addr, int addrLen)
    {
        std::string username;
        if (!r.ReadString(username))
            username = "Unknown";

        // Проверяем — может этот адрес уже подключён (повторный Handshake)
        ConnectedPeer* existing = FindPeerByAddr(addr, addrLen);
        if (existing)
        {
            // Пересылаем HandshakeAck ещё раз (пакет мог потеряться)
            PacketWriter ack(PacketType::HandshakeAck, /*reliable=*/true,
                             existing->outSeq);
            ack.WriteU32(existing->networkId);
            ack.WriteString(existing->username);
            SendReliable(existing->networkId, ack);
            return;
        }

        // Новый клиент
        NetworkId newId = NetworkManager::Get().AllocNetworkId();

        ConnectedPeer peer;
        peer.networkId = newId;
        peer.username  = username;
        peer.addrLen   = addrLen;
        std::memcpy(&peer.addr, addr,
                    static_cast<size_t>(addrLen) <= sizeof(peer.addr.bytes)
                        ? static_cast<size_t>(addrLen)
                        : sizeof(peer.addr.bytes));

        m_peers[newId] = std::move(peer);

        std::cout << "[NetworkServer] Peer connected: id=" << newId
                  << " username=" << username << "\n";

        // Отправляем HandshakeAck (Reliable) с назначенным NetworkId
        PacketWriter ack(PacketType::HandshakeAck, /*reliable=*/true,
                         m_peers[newId].outSeq++);
        ack.WriteU32(newId);
        ack.WriteString(username);
        SendReliable(newId, ack);

        if (m_onConnected)
            m_onConnected(newId, username);
    }

    // -----------------------------------------------------------------------
    // HandleAck
    // -----------------------------------------------------------------------

    void NetworkServer::HandleAck(PacketReader& r, NetworkId id)
    {
        uint16_t seq = 0;
        if (!r.ReadU16(seq))
            return;

        ConnectedPeer* peer = FindPeer(id);
        if (!peer)
            return;

        peer->pendingAck.erase(seq);
    }

    // -----------------------------------------------------------------------
    // HandleDisconnect
    // -----------------------------------------------------------------------

    void NetworkServer::HandleDisconnect(NetworkId id)
    {
        std::cout << "[NetworkServer] Peer disconnected: id=" << id << "\n";

        if (m_onDisconnected)
            m_onDisconnected(id);

        m_peers.erase(id);
    }

    // -----------------------------------------------------------------------
    // Send helpers
    // -----------------------------------------------------------------------

    void NetworkServer::SendTo(NetworkId id, const PacketWriter& pkt)
    {
        ConnectedPeer* peer = FindPeer(id);
        if (!peer) return;
        RawSendToAddr(reinterpret_cast<const sockaddr*>(&peer->addr.bytes),
                      peer->addrLen, pkt.Data(), pkt.Size());
    }

    void NetworkServer::SendToAddr(const sockaddr* addr, int addrLen,
                                   const PacketWriter& pkt)
    {
        RawSendToAddr(addr, addrLen, pkt.Data(), pkt.Size());
    }

    void NetworkServer::SendReliable(NetworkId id, PacketWriter& pkt)
    {
        ConnectedPeer* peer = FindPeer(id);
        if (!peer) return;

        // Сохраняем в pendingAck
        ConnectedPeer::PendingReliable pending;
        pending.data.assign(pkt.Data(), pkt.Data() + pkt.Size());
        pending.lastSent = std::chrono::steady_clock::now();

        uint16_t seq = 0;
        {
            // Вытащим seq из заголовка уже записанного пакета
            PacketReader tmp(pkt.Data(), pkt.Size());
            seq = tmp.Header().seq;
        }

        peer->pendingAck[seq] = std::move(pending);

        RawSendToAddr(reinterpret_cast<const sockaddr*>(&peer->addr.bytes),
                      peer->addrLen, pkt.Data(), pkt.Size());
    }

    void NetworkServer::SendUnreliableTo(NetworkId id, const PacketWriter& pkt)
    {
        // Fire-and-forget: без сохранения в pendingAck, без повторов.
        // Используется для PropertyUpdate (CFrame, ClockTime и т.д.).
        SendTo(id, pkt);
    }

    void NetworkServer::RawSendToAddr(const sockaddr* addr, int addrLen,
                                      const uint8_t* data, size_t size)
    {
        if (!m_udp || size == 0 || size > MAX_PACKET_SIZE)
            return;

        // Выделяем SendRequest на куче — libuv завершит отправку асинхронно
        auto* req = new SendRequest();
        req->size = size;
        std::memcpy(req->data, data, size);

        uv_buf_t buf = uv_buf_init(reinterpret_cast<char*>(req->data),
                                   static_cast<unsigned int>(size));

        uv_udp_send(&req->req, m_udp, &buf, 1, addr,
            [](uv_udp_send_t* r, int status)
            {
                if (status != 0)
                    std::cerr << "[NetworkServer] Send error: "
                              << uv_strerror(status) << "\n";
                delete reinterpret_cast<SendRequest*>(r);
            }
        );
    }

    // -----------------------------------------------------------------------
    // Lookup helpers
    // -----------------------------------------------------------------------

    ConnectedPeer* NetworkServer::FindPeer(NetworkId id)
    {
        auto it = m_peers.find(id);
        return (it != m_peers.end()) ? &it->second : nullptr;
    }

    const std::unordered_map<NetworkId, ConnectedPeer>& NetworkServer::GetPeers() const
    {
        return m_peers;
    }

    ConnectedPeer* NetworkServer::FindPeerByAddr(const sockaddr* addr, int addrLen)
    {
        for (auto& [id, peer] : m_peers)
        {
            if (peer.addrLen == addrLen &&
                std::memcmp(&peer.addr.bytes, addr, static_cast<size_t>(addrLen)) == 0)
            {
                return &peer;
            }
        }
        return nullptr;
    }

    // -----------------------------------------------------------------------
    // Callback setters
    // -----------------------------------------------------------------------

    void NetworkServer::SetOnPeerConnected(
        std::function<void(NetworkId, const std::string&)> cb)
    {
        m_onConnected = std::move(cb);
    }

    void NetworkServer::SetOnPeerDisconnected(
        std::function<void(NetworkId)> cb)
    {
        m_onDisconnected = std::move(cb);
    }

    void NetworkServer::SetOnPacketReceived(
        std::function<void(NetworkId, PacketReader&)> cb)
    {
        m_onPacket = std::move(cb);
    }

} // namespace Net
} // namespace Sunvoltum
