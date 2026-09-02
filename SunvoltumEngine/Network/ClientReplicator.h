#pragma once

// ---------------------------------------------------------------------------
// ClientReplicator.h — client-side обработка live-репликации
//
//   InstanceAdded    (Reliable)            — создать объект
//   InstanceRemoved  (Reliable)            — удалить объект
//   PropertyChanged  (Reliable)            — структурные свойства (цвет, размер...)
//   PropertyUpdate   (Unreliable+Sequenced) — горячие свойства (CFrame, ClockTime...)
//
// PropertyUpdate: каждый пакет содержит uint32_t sequence.
// Клиент отбрасывает пакет если его sequence <= последнего принятого
// для данного netId — защита от out-of-order доставки UDP.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <unordered_map>

#include "../LibSunvoltum.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "NetworkPacket.h"

namespace Sunvoltum {
namespace Net {

    class LibSunvoltum ClientReplicator
    {
    public:
        static ClientReplicator& Get();

        ClientReplicator(const ClientReplicator&)            = delete;
        ClientReplicator& operator=(const ClientReplicator&) = delete;

        // Передать DataModel в который применяются изменения.
        void SetDataModel(DataModel* dm);

        // Обработчики пакетов — вызывать из SetOnPacketReceived клиента
        void OnInstanceAdded   (PacketReader& r);
        void OnInstanceRemoved (PacketReader& r);
        void OnPropertyChanged (PacketReader& r);  // Reliable
        void OnPropertyUpdate  (PacketReader& r);  // Unreliable+Sequenced

        // Ключ для таблицы sequence — публичный чтобы .cpp мог его использовать
        struct PropKey
        {
            InstanceNetId netId;
            PropertyId    propId;
            bool operator==(const PropKey& o) const
            { return netId == o.netId && propId == o.propId; }
        };
        struct PropKeyHash
        {
            size_t operator()(const PropKey& k) const
            {
                return std::hash<uint64_t>()(
                    (static_cast<uint64_t>(k.netId) << 8) | k.propId);
            }
        };

    private:
        ClientReplicator() = default;
        ~ClientReplicator() = default;

        DataModel* m_dataModel = nullptr;

        std::unordered_map<PropKey, uint32_t, PropKeyHash> m_lastUpdateSeq;
    };

} // namespace Net
} // namespace Sunvoltum
