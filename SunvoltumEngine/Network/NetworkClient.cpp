#include "NetworkClient.h"

#include <uv.h>
#include <cstring>
#include <iostream>
#include <cassert>

namespace Sunvoltum {
namespace Net {

    // -----------------------------------------------------------------------
    // Вспомогательная структура для uv_udp_send_t (владеет буфером)
    // -----------------------------------------------------------------------
    struct ClientSendRequest
    {
        uv_udp_send_t req  = {};
        uint8_t       data[MAX_PACKET_SIZE] = {};
        size_t        size = 0;
    };

    // -----------------------------------------------------------------------
    // Singleton
    // -----------------------------------------------------------------------

    NetworkClient& NetworkClient::Get()
    {
        static NetworkClient instance;
        return instance;
    }

    // -----------------------------------------------------------------------
    // Connect
    // -----------------------------------------------------------------------

    bool NetworkClient::Connect(const std::string& host, uint16_t port,
                                const std::string& username)
    {
        if (m_state != ConnectionState::Disconnected)
        {
            std::cerr << "[NetworkClient] Already connecting or connected\n";
            return false;
        }

        uv_loop_t* loop = NetworkManager::Get().GetLoop();
        if (!loop)
        {
            std::cerr << "[NetworkClient] NetworkManager not initialized\n";
            return false;
        }

        m_username = username;

        // Инициализируем UDP-сокет
        m_udp = new uv_udp_t();
        if (uv_udp_init(loop, m_udp) != 0)
        {
            std::cerr << "[NetworkClient] uv_udp_init failed\n";
            delete m_udp;
            m_udp = nullptr;
            return false;
        }
        m_udp->data = this;

        // Привязываем к любому локальному порту (0 = ОС выберет сам)
        struct sockaddr_in localAddr = {};
        uv_ip4_addr("0.0.0.0", 0, &localAddr);
        if (uv_udp_bind(m_udp, reinterpret_cast<const sockaddr*>(&localAddr), 0) != 0)
        {
            std::cerr << "[NetworkClient] uv_udp_bind (local) failed\n";
            uv_close(reinterpret_cast<uv_handle_t*>(m_udp), [](uv_handle_t* h) {
                delete reinterpret_cast<uv_udp_t*>(h);
            });
            m_udp = nullptr;
            return false;
        }

        // Сохраняем адрес сервера
        struct sockaddr_in serverAddr4 = {};
        if (uv_ip4_addr(host.c_str(), port, &serverAddr4) != 0)
        {
            std::cerr << "[NetworkClient] Invalid server address: "
                      << host << ":" << port << "\n";
            uv_close(reinterpret_cast<uv_handle_t*>(m_udp), [](uv_handle_t* h) {
                delete reinterpret_cast<uv_udp_t*>(h);
            });
            m_udp = nullptr;
            return false;
        }
        std::memcpy(&m_serverAddr.bytes, &serverAddr4, sizeof(serverAddr4));
        m_serverAddrLen = sizeof(serverAddr4);

        // Начинаем принимать датаграммы (только от сервера)
        auto alloc_cb = [](uv_handle_t* /*handle*/, size_t suggested_size, uv_buf_t* buf)
        {
            buf->base = new char[suggested_size];
            buf->len  = static_cast<ULONG>(suggested_size);
        };

        auto recv_cb = [](uv_udp_t* handle,
                          ssize_t nread,
                          const uv_buf_t* buf,
                          const struct sockaddr* /*addr*/,
                          unsigned /*flags*/)
        {
            auto* cli = reinterpret_cast<NetworkClient*>(handle->data);

            if (nread > 0)
            {
                cli->OnReceive(
                    reinterpret_cast<const uint8_t*>(buf->base),
                    static_cast<size_t>(nread)
                );
            }

            delete[] buf->base;
        };

        if (uv_udp_recv_start(m_udp, alloc_cb, recv_cb) != 0)
        {
            std::cerr << "[NetworkClient] uv_udp_recv_start failed\n";
            uv_close(reinterpret_cast<uv_handle_t*>(m_udp), [](uv_handle_t* h) {
                delete reinterpret_cast<uv_udp_t*>(h);
            });
            m_udp = nullptr;
            return false;
        }

        // Переходим в состояние Connecting и шлём Handshake
        m_state = ConnectionState::Connecting;
        m_lastHandshakeSent = std::chrono::steady_clock::now();

        PacketWriter handshake(PacketType::Handshake);
        handshake.WriteString(m_username);
        RawSend(handshake.Data(), handshake.Size());

        std::cout << "[NetworkClient] Connecting to "
                  << host << ":" << port
                  << " as \"" << username << "\"...\n";
        return true;
    }

    // -----------------------------------------------------------------------
    // Disconnect
    // -----------------------------------------------------------------------

    void NetworkClient::Disconnect()
    {
        if (m_state == ConnectionState::Disconnected)
            return;

        // Шлём Disconnect серверу (best-effort, без подтверждения)
        PacketWriter pkt(PacketType::Disconnect);
        RawSend(pkt.Data(), pkt.Size());

        m_state     = ConnectionState::Disconnected;
        m_networkId = INVALID_NETWORK_ID;
        m_pendingAck.clear();

        std::cout << "[NetworkClient] Disconnected.\n";

        if (m_onDisconnected)
            m_onDisconnected();
    }

    // -----------------------------------------------------------------------
    // Shutdown
    // -----------------------------------------------------------------------

    void NetworkClient::Shutdown()
    {
        Disconnect();

        if (m_udp)
        {
            uv_udp_recv_stop(m_udp);
            uv_close(reinterpret_cast<uv_handle_t*>(m_udp), [](uv_handle_t* h) {
                delete reinterpret_cast<uv_udp_t*>(h);
            });
            m_udp = nullptr;
        }
    }

    // -----------------------------------------------------------------------
    // ResendPending
    // -----------------------------------------------------------------------

    void NetworkClient::ResendPending()
    {
        if (m_state != ConnectionState::Connected)
            return;

        auto now = std::chrono::steady_clock::now();
        for (auto& [seq, pending] : m_pendingAck)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - pending.lastSent).count();

            if (elapsed >= RELIABLE_RESEND_MS)
            {
                RawSend(pending.data.data(), pending.data.size());
                pending.lastSent = now;
            }
        }
    }

    // -----------------------------------------------------------------------
    // RetryHandshake
    // -----------------------------------------------------------------------

    void NetworkClient::RetryHandshake()
    {
        if (m_state != ConnectionState::Connecting)
            return;

        auto now     = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastHandshakeSent).count();

        if (elapsed >= HANDSHAKE_RETRY_MS)
        {
            std::cout << "[NetworkClient] Retrying handshake...\n";
            PacketWriter handshake(PacketType::Handshake);
            handshake.WriteString(m_username);
            RawSend(handshake.Data(), handshake.Size());
            m_lastHandshakeSent = now;
        }
    }

    // -----------------------------------------------------------------------
    // OnReceive — вызывается из libuv-колбэка
    // -----------------------------------------------------------------------

    void NetworkClient::OnReceive(const uint8_t* data, size_t len)
    {
        if (len < HEADER_SIZE)
            return;

        PacketReader reader(data, len);
        if (!reader.Ok())
            return;

        // Если Reliable — сразу шлём ACK обратно
        if (reader.Header().IsReliable())
        {
            PacketWriter ack(PacketType::Ack);
            ack.WriteU16(reader.Header().seq);
            RawSend(ack.Data(), ack.Size());
        }

        switch (reader.Type())
        {
        case PacketType::HandshakeAck:
            HandleHandshakeAck(reader);
            break;

        case PacketType::Ack:
            HandleAck(reader);
            break;

        case PacketType::Disconnect:
            HandleDisconnect();
            break;

        default:
            // Прочие пакеты — только если уже подключены
            if (m_state == ConnectionState::Connected && m_onPacket)
                m_onPacket(reader);
            break;
        }
    }

    // -----------------------------------------------------------------------
    // HandleHandshakeAck
    // -----------------------------------------------------------------------

    void NetworkClient::HandleHandshakeAck(PacketReader& r)
    {
        // Принимаем даже если уже Connected (повторный ACK из-за потери)
        uint32_t networkId = 0;
        std::string username;
        if (!r.ReadU32(networkId) || !r.ReadString(username))
        {
            std::cerr << "[NetworkClient] Malformed HandshakeAck\n";
            return;
        }

        if (m_state == ConnectionState::Connected)
            return; // уже обработали

        m_networkId = static_cast<NetworkId>(networkId);
        m_state     = ConnectionState::Connected;

        std::cout << "[NetworkClient] Connected! NetworkId=" << m_networkId
                  << " username=" << username << "\n";

        if (m_onConnected)
            m_onConnected(m_networkId);
    }

    // -----------------------------------------------------------------------
    // HandleAck
    // -----------------------------------------------------------------------

    void NetworkClient::HandleAck(PacketReader& r)
    {
        uint16_t seq = 0;
        if (!r.ReadU16(seq))
            return;

        m_pendingAck.erase(seq);
    }

    // -----------------------------------------------------------------------
    // HandleDisconnect
    // -----------------------------------------------------------------------

    void NetworkClient::HandleDisconnect()
    {
        std::cout << "[NetworkClient] Server disconnected us.\n";
        m_state     = ConnectionState::Disconnected;
        m_networkId = INVALID_NETWORK_ID;
        m_pendingAck.clear();

        if (m_onDisconnected)
            m_onDisconnected();
    }

    // -----------------------------------------------------------------------
    // Send helpers
    // -----------------------------------------------------------------------

    void NetworkClient::Send(const PacketWriter& pkt)
    {
        RawSend(pkt.Data(), pkt.Size());
    }

    void NetworkClient::SendReliable(PacketWriter& pkt)
    {
        // Считываем seq из уже записанного заголовка
        uint16_t seq = 0;
        {
            PacketReader tmp(pkt.Data(), pkt.Size());
            seq = tmp.Header().seq;
        }

        PendingReliable pending;
        pending.data.assign(pkt.Data(), pkt.Data() + pkt.Size());
        pending.lastSent = std::chrono::steady_clock::now();
        m_pendingAck[seq] = std::move(pending);

        RawSend(pkt.Data(), pkt.Size());
    }

    void NetworkClient::RawSend(const uint8_t* data, size_t size)
    {
        if (!m_udp || size == 0 || size > MAX_PACKET_SIZE)
            return;

        auto* req = new ClientSendRequest();
        req->size = size;
        std::memcpy(req->data, data, size);

        uv_buf_t buf = uv_buf_init(reinterpret_cast<char*>(req->data),
                                   static_cast<unsigned int>(size));

        uv_udp_send(&req->req, m_udp, &buf, 1,
            reinterpret_cast<const sockaddr*>(&m_serverAddr.bytes),
            [](uv_udp_send_t* r, int status)
            {
                if (status != 0)
                    std::cerr << "[NetworkClient] Send error: "
                              << uv_strerror(status) << "\n";
                delete reinterpret_cast<ClientSendRequest*>(r);
            }
        );
    }

    // -----------------------------------------------------------------------
    // Getters
    // -----------------------------------------------------------------------

    ConnectionState NetworkClient::GetState() const
    {
        return m_state;
    }

    NetworkId NetworkClient::GetNetworkId() const
    {
        return m_networkId;
    }

    // -----------------------------------------------------------------------
    // Callback setters
    // -----------------------------------------------------------------------

    void NetworkClient::SetOnConnected(std::function<void(NetworkId)> cb)
    {
        m_onConnected = std::move(cb);
    }

    void NetworkClient::SetOnDisconnected(std::function<void()> cb)
    {
        m_onDisconnected = std::move(cb);
    }

    void NetworkClient::SetOnPacketReceived(std::function<void(PacketReader&)> cb)
    {
        m_onPacket = std::move(cb);
    }

} // namespace Net
} // namespace Sunvoltum
