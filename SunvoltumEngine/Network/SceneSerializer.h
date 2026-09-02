#pragma once

// ---------------------------------------------------------------------------
// SceneSerializer.h — server-side сериализация сцены при подключении клиента
//
// Алгоритм:
//   1. StartForPeer(peerId, root)
//      - BFS-обход дерева InstanceParent, присвоение SerializeId (0, 1, 2, ...)
//      - Сохранение снапшота каждого объекта (имя, classId, parentId, свойства)
//      - Отправка StartSerialization(totalCount, sessionToken) — Reliable
//
//   2. OnReadySerialization(peerId, reader)
//      - Проверка sessionToken
//      - Отправка всех NewInstance подряд — Reliable
//      - Отправка EndSerialization(checksum) — Reliable
//
//   3. OnAskInstance(peerId, reader)
//      - Клиент запрашивает пропущенные SerializeId
//      - Проверка sessionToken
//      - Повторная отправка запрошенных NewInstance — Reliable
//
//   4. OnSerializationComplete(peerId, reader)
//      - Проверка sessionToken
//      - Очистка состояния пира (освобождение памяти снапшота)
//      - Вызов колбэка m_onComplete(peerId)
//
// Checksum: XOR всех serializeId, подтверждает что EndSerialization
// относится именно к той серии NewInstance, которую клиент получал.
//
// Пропуск объектов Player: сервер не сериализует объекты с ClassId == 14
// (Player) — они создаются индивидуально в SetOnPeerConnected.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "../LibSunvoltum.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceParent.h"
#include "../DataModel/PropertyValue.h"
#include "NetworkServer.h"
#include "NetworkPacket.h"
#include "NetworkSerializer.h"  // SERIALIZE_ID_NONE

namespace Sunvoltum {
namespace Net {

    // -----------------------------------------------------------------------
    // InstanceSnapshot — снапшот одного объекта на момент старта сериализации
    // -----------------------------------------------------------------------
    struct InstanceSnapshot
    {
        uint16_t      serializeId = 0;
        uint16_t      parentId    = SERIALIZE_ID_NONE; // 0xFFFF = корень DataModel
        InstanceNetId netId       = INVALID_INSTANCE_NET_ID; // постоянный id объекта
        int8_t        classId     = 0;
        std::string   name;

        // Свойства: propId → PropertyValue
        // InstanceRef хранятся как есть; при отправке заменяются на SerializeId
        std::vector<std::pair<PropertyId, PropertyValue>> properties;
    };

    // -----------------------------------------------------------------------
    // SceneSerializer — синглтон
    // -----------------------------------------------------------------------
    class LibSunvoltum SceneSerializer
    {
    public:
        static SceneSerializer& Get();

        SceneSerializer(const SceneSerializer&)            = delete;
        SceneSerializer& operator=(const SceneSerializer&) = delete;

        // Колбэк завершения сериализации для конкретного пира.
        // Вызывается когда клиент прислал SerializationComplete с верным токеном.
        void SetOnComplete(std::function<void(NetworkId peerId)> cb);

        // ---------------------------------------------------------------
        // Запустить сериализацию для подключившегося пира.
        // root  — узел с которого начинается BFS (обычно engine.DataModel).
        // skipClassId — ClassId который не сериализуется (по умолчанию 14=Player,
        //               0 = не пропускать ничего дополнительно).
        // ---------------------------------------------------------------
        void StartForPeer(NetworkId peerId, InstanceParent& root,
                          int8_t skipClassId = 14);

        // Обработчики входящих пакетов от клиента — вызывать из SetOnPacketReceived
        void OnReadySerialization   (NetworkId peerId, PacketReader& r);
        void OnAskInstance          (NetworkId peerId, PacketReader& r);
        void OnSerializationComplete(NetworkId peerId, PacketReader& r);

    private:
        SceneSerializer() = default;
        ~SceneSerializer() = default;

        // ------------------------------------------------------------------
        // Состояние одного пира во время сериализации
        // ------------------------------------------------------------------
        struct PeerSerState
        {
            uint32_t  token      = 0;
            uint16_t  totalCount = 0;
            uint32_t  checksum   = 0;

            // Индексированные по serializeId снапшоты
            std::vector<InstanceSnapshot> snapshots;

            // Instance* → serializeId (для разрешения InstanceRef при отправке)
            std::unordered_map<Instance*, uint16_t> instToId;
        };

        std::unordered_map<NetworkId, PeerSerState> m_states;
        std::function<void(NetworkId)>               m_onComplete;

        // ------------------------------------------------------------------
        // BFS-обход дерева, заполнение вектора снапшотов
        // ------------------------------------------------------------------
        void FlattenTree(InstanceParent& root,
                         int8_t skipClassId,
                         std::vector<InstanceSnapshot>& out,
                         std::unordered_map<Instance*, uint16_t>& idMap);

        // Отправить один NewInstance пакет для снапшота snap
        void SendNewInstance(NetworkId peerId,
                             const InstanceSnapshot& snap,
                             uint32_t token,
                             const std::unordered_map<Instance*, uint16_t>& instToId);
    };

} // namespace Net
} // namespace Sunvoltum
