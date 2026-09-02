#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// NetworkPacket.h — бинарный протокол поверх UDP
//
// Структура каждого UDP-датаграмма:
//
//   [ PacketHeader (5 байт) ][ payload... ]
//
//   PacketHeader:
//     uint8_t  flags      — бит 0: Reliable (требует ACK)
//     uint16_t seq        — порядковый номер (только у Reliable-пакетов)
//     uint16_t type       — PacketType (тип содержимого)
//
// Reliable-пакеты:
//   Сервер/клиент сохраняет неподтверждённые пакеты и повторяет их
//   через RELIABLE_RESEND_MS миллисекунд до получения ACK.
//   ACK-пакет содержит в payload uint16_t — seq подтверждаемого пакета.
//
// Unreliable-пакеты:
//   Fire-and-forget, seq игнорируется (пишем 0).
//   Используются для частых обновлений (позиции, состояние объектов).
// ---------------------------------------------------------------------------

namespace Sunvoltum {
namespace Net {

    // -----------------------------------------------------------------------
    // Константы
    // -----------------------------------------------------------------------

    static constexpr uint32_t MAX_PACKET_SIZE     = 1200; // байт, безопасно для MTU
    static constexpr uint32_t HEADER_SIZE         = 5;    // flags(1) + seq(2) + type(2)
    static constexpr uint32_t MAX_PAYLOAD_SIZE    = MAX_PACKET_SIZE - HEADER_SIZE;
    static constexpr uint32_t RELIABLE_RESEND_MS  = 100;  // мс до повторной отправки

    static constexpr uint8_t  FLAG_RELIABLE       = 0x01; // пакет требует ACK

    // -----------------------------------------------------------------------
    // PacketType — тип содержимого пакета
    // -----------------------------------------------------------------------
    enum class PacketType : uint16_t
    {
        // --- Соединение ---
        Handshake       = 1,  // C→S: запрос на подключение (имя, версия)
        HandshakeAck    = 2,  // S→C: подтверждение + NetworkId + CharacterName
        Disconnect      = 3,  // любой: намеренное отключение

        // --- Надёжность ---
        Ack             = 4,  // подтверждение Reliable-пакета (payload: uint16_t seq)

        // --- Синхронизация состояния ---
        // PropertyUpdate = 10 был зарезервирован ранее, но не реализован.
        // Теперь используется PropertyUpdate = 33 (Unreliable+Sequenced)
        // и PropertyChanged = 32 (Reliable). Значение 10 свободно.

        // --- Сериализация сцены (начальная синхронизация при подключении) ---
        //
        // Flow:
        //   S → C  StartSerialization    (Reliable) — начало, сколько объектов ждать
        //   C → S  ReadySerialization    (Reliable) — клиент готов принимать
        //   S → C  NewInstance           (Reliable) — один объект с SerializeId
        //   S → C  EndSerialization      (Reliable) — конец потока + checksum
        //   C → S  AskInstance           (Reliable) — запрос пропущенных SerializeId
        //   C → S  SerializationComplete (Reliable) — клиент получил все объекты
        //
        // Безопасность: каждый пакет содержит sessionToken (uint32_t),
        // сгенерированный сервером случайно. Клиент обязан эхировать его
        // в каждом ответе — сервер отбрасывает пакеты с неверным токеном.
        StartSerialization    = 20, // S→C: uint16_t totalCount, uint32_t sessionToken
        ReadySerialization    = 21, // C→S: uint32_t sessionToken
        NewInstance           = 22, // S→C: uint32_t sessionToken, uint32_t netId,
                                    //       uint16_t serializeId,
                                    //       uint16_t parentId (0xFFFF=root), int8_t classId,
                                    //       string name, uint8_t propCount,
                                    //       [uint8_t propId, encoded PropertyValue] × propCount
        EndSerialization      = 23, // S→C: uint32_t sessionToken, uint32_t checksum
        AskInstance           = 24, // C→S: uint32_t sessionToken, uint16_t missingCount,
                                    //       uint16_t missingIds[missingCount]
        SerializationComplete = 25, // C→S: uint32_t sessionToken

        // --- Репликация объектов (live-обновления после сериализации) ---
        //
        // Все пакеты Reliable — потеря любого недопустима.
        //
        // InstanceNetId — постоянный uint32_t идентификатор объекта,
        // назначается сервером через InstanceRegistry::Assign().
        // ParentNetId = 0 (INVALID_INSTANCE_NET_ID) означает корень DataModel.
        //
        InstanceAdded    = 30, // S→C: uint32_t netId, uint32_t parentNetId,
                               //       int8_t classId, string name,
                               //       uint8_t propCount,
                               //       [uint8_t propId, encoded PropertyValue] × propCount
                               //       Reliable — потеря недопустима
        InstanceRemoved  = 31, // S→C: uint32_t netId
                               //       Reliable — потеря недопустима
        PropertyChanged  = 32, // S→C: uint32_t netId, uint8_t propId,
                               //       encoded PropertyValue (type + value)
                               //       Reliable — для структурных свойств (цвет, размер, Anchored)
        PropertyUpdate   = 33, // S→C: uint32_t sequence, uint32_t netId, uint8_t propId,
                               //       encoded PropertyValue (type + value)
                               //       Unreliable+Sequenced — для высокочастотных свойств
                               //       (CFrame, ClockTime, скорости).
                               //       Клиент принимает только если sequence > последнего.

        // --- Зарезервировано ---
        Reserved        = 0xFFFF,
    };

    // -----------------------------------------------------------------------
    // PacketHeader — заголовок, первые HEADER_SIZE байт каждой датаграммы
    // -----------------------------------------------------------------------
#pragma pack(push, 1)
    struct PacketHeader
    {
        uint8_t  flags = 0;
        uint16_t seq   = 0;
        uint16_t type  = 0;

        bool IsReliable() const { return (flags & FLAG_RELIABLE) != 0; }

        void SetReliable(bool r)
        {
            if (r) flags |=  FLAG_RELIABLE;
            else   flags &= ~FLAG_RELIABLE;
        }
    };
#pragma pack(pop)

    static_assert(sizeof(PacketHeader) == HEADER_SIZE, "PacketHeader size mismatch");

    // -----------------------------------------------------------------------
    // PacketWriter — последовательная запись payload в буфер
    // -----------------------------------------------------------------------
    class PacketWriter
    {
    public:
        explicit PacketWriter(PacketType type, bool reliable = false, uint16_t seq = 0)
        {
            m_buf.resize(HEADER_SIZE);
            PacketHeader hdr;
            hdr.SetReliable(reliable);
            hdr.seq  = seq;
            hdr.type = static_cast<uint16_t>(type);
            std::memcpy(m_buf.data(), &hdr, HEADER_SIZE);
        }

        void WriteU8 (uint8_t  v) { Append(&v, 1); }
        void WriteU16(uint16_t v) { Append(&v, 2); }
        void WriteU32(uint32_t v) { Append(&v, 4); }
        void WriteI32(int32_t  v) { Append(&v, 4); }
        void WriteF32(float    v) { Append(&v, 4); }

        void WriteString(const std::string& s)
        {
            uint16_t len = static_cast<uint16_t>(s.size());
            WriteU16(len);
            if (len > 0)
                Append(s.data(), len);
        }

        const uint8_t* Data() const { return m_buf.data(); }
        size_t         Size() const { return m_buf.size(); }

    private:
        void Append(const void* data, size_t n)
        {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
            m_buf.insert(m_buf.end(), p, p + n);
        }

        std::vector<uint8_t> m_buf;
    };

    // -----------------------------------------------------------------------
    // PacketReader — последовательное чтение payload из буфера
    // -----------------------------------------------------------------------
    class PacketReader
    {
    public:
        PacketReader(const uint8_t* data, size_t size)
            : m_data(data), m_size(size), m_pos(0)
        {
            if (m_size >= HEADER_SIZE)
            {
                std::memcpy(&m_header, m_data, HEADER_SIZE);
                m_pos = HEADER_SIZE;
            }
        }

        bool        Ok()     const { return m_pos <= m_size; }
        PacketHeader Header() const { return m_header; }
        PacketType  Type()   const { return static_cast<PacketType>(m_header.type); }

        bool ReadU8 (uint8_t&  v) { return Read(&v, 1); }
        bool ReadU16(uint16_t& v) { return Read(&v, 2); }
        bool ReadU32(uint32_t& v) { return Read(&v, 4); }
        bool ReadI32(int32_t&  v) { return Read(&v, 4); }
        bool ReadF32(float&    v) { return Read(&v, 4); }

        bool ReadString(std::string& s)
        {
            uint16_t len = 0;
            if (!ReadU16(len)) return false;
            if (m_pos + len > m_size) return false;
            s.assign(reinterpret_cast<const char*>(m_data + m_pos), len);
            m_pos += len;
            return true;
        }

    private:
        bool Read(void* dst, size_t n)
        {
            if (m_pos + n > m_size) return false;
            std::memcpy(dst, m_data + m_pos, n);
            m_pos += n;
            return true;
        }

        const uint8_t* m_data   = nullptr;
        size_t         m_size   = 0;
        size_t         m_pos    = 0;
        PacketHeader   m_header = {};
    };

} // namespace Net
} // namespace Sunvoltum
