#pragma once

// ---------------------------------------------------------------------------
// ServerReplicator.h — server-side live-репликация изменений DataModel
//
// Запускается ПОСЛЕ завершения сериализации (SceneSerializer::SetOnComplete).
// До этого момента подписки не активны — новые объекты попадают в сериализацию
// как часть снапшота, а не как отдельные InstanceAdded.
//
// Что делает:
//   - Рекурсивно обходит всё дерево DataModel и присваивает InstanceNetId
//     каждому объекту через InstanceRegistry::Assign().
//   - Подписывается на ChildAdded/ChildRemoved каждого контейнера
//     (DataModel + все Instance-контейнеры рекурсивно).
//   - Подписывается через PropertyManager на все свойства каждого объекта.
//
// При изменениях:
//   ChildAdded     → InstanceAdded  (Reliable) → всем Connected пирам
//                    у которых сериализация уже завершена
//   ChildRemoved   → InstanceRemoved (Reliable) → тем же пирам
//   SetProperty    → PropertyChanged (Reliable) → тем же пирам
//
// Пиры у которых сериализация ещё не завершена не получают репликацию —
// они получат актуальный снапшот в момент своей сериализации.
// Для этого ServerReplicator хранит множество "готовых" пиров,
// которое пополняется через MarkPeerReady(peerId).
// ---------------------------------------------------------------------------

#include <cstdint>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <functional>

#include "../LibSunvoltum.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceParent.h"
#include "../DataModel/PropertyManager.h"
#include "NetworkPacket.h"
#include "NetworkServer.h"

namespace Sunvoltum {
namespace Net {

    class LibSunvoltum ServerReplicator
    {
    public:
        static ServerReplicator& Get();

        ServerReplicator(const ServerReplicator&)            = delete;
        ServerReplicator& operator=(const ServerReplicator&) = delete;

        // Инициализировать репликатор.
        // Обходит дерево dm, присваивает InstanceNetId, навешивает подписки.
        // Вызывать один раз после построения DataModel на сервере.
        void Init(DataModel& dm);

        // Пометить пира как "готового" — он получил полный снапшот сцены
        // и теперь должен получать live-репликацию.
        // Вызывать из SceneSerializer::SetOnComplete.
        void MarkPeerReady(NetworkId peerId);

        // Убрать пира из списка готовых (при отключении).
        void MarkPeerGone(NetworkId peerId);

        // Освободить все подписки (при завершении сервера).
        void Shutdown();

        // Вызывать каждый кадр из Heartbeat сервера.
        // Отправляет rate-limited обновления у которых истёк интервал.
        void Tick();

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
        ServerReplicator() = default;
        ~ServerReplicator() = default;

        // Рекурсивный обход: присвоить NetId + навесить подписки
        void RegisterRecursive(InstanceParent& node);

        // Навесить PropertyManager-подписки на все свойства объекта
        void SubscribeProperties(Instance& inst);

        // Отправить InstanceAdded всем готовым пирам
        void BroadcastInstanceAdded(Instance& inst);

        // Отправить InstanceRemoved всем готовым пирам
        void BroadcastInstanceRemoved(InstanceNetId netId);

        // Отправить PropertyChanged (Reliable) всем готовым пирам
        void BroadcastPropertyChanged(InstanceNetId netId,
                                      PropertyId    propId,
                                      const PropertyValue& value);

        // Отправить PropertyUpdate (Unreliable+Sequenced) всем готовым пирам
        void BroadcastPropertyUpdate(InstanceNetId netId,
                                     PropertyId    propId,
                                     const PropertyValue& value);

        // Внутренняя отправка без rate-проверки
        void DoSendPropertyUpdate(const PropKey& key,
                                  const PropertyValue& value,
                                  std::chrono::steady_clock::time_point sendTime);

        // Является ли свойство высокочастотным (Unreliable канал)
        static bool IsFrequentProperty(PropertyId propId);

        // Минимальный интервал отправки для свойства (мс)
        static uint32_t GetRateMs(PropertyId propId);

        static constexpr uint32_t RATE_DEFAULT_MS = 50; // 20 раз/сек
        static constexpr uint32_t RATE_PHYSICS_MS = 33; // ~30 раз/сек

        DataModel* m_dataModel = nullptr;

        // Пиры у которых сериализация завершена
        std::unordered_set<NetworkId> m_readyPeers;

        // per-peer, per-(netId,propId) sequence счётчики
        std::unordered_map<NetworkId,
            std::unordered_map<PropKey, uint32_t, PropKeyHash>> m_updateSeq;

        // Rate-limiting: для каждой пары (netId,propId) хранит:
        //   - время последней фактической отправки
        //   - последнее значение (чтобы отправить его когда интервал истечёт)
        //   - флаг что значение «грязное» (изменилось но ещё не отправлено)
        struct PendingUpdate
        {
            PropertyValue             lastValue;
            std::chrono::steady_clock::time_point lastSentTime;
            bool                      dirty = false;
        };
        std::unordered_map<PropKey, PendingUpdate, PropKeyHash> m_pendingUpdates;

        // Flush dirty updates — вызывать каждый кадр из Tick()
        // Отправляет накопленные значения у которых истёк интервал.
        void FlushPendingUpdates();

        // Подписки на ChildAdded/ChildRemoved контейнеров
        std::vector<ChildAddedToken>   m_childAddedTokens;
        std::vector<ChildRemovedToken> m_childRemovedTokens;

        // Подписки на свойства объектов: netId → вектор PropertyToken
        std::unordered_map<InstanceNetId, std::vector<PropertyToken>> m_propTokens;

        bool m_initialized = false;
    };

} // namespace Net
} // namespace Sunvoltum
