#include "ServerReplicator.h"
#include "NetworkSerializer.h"
#include "../DataModel/InstanceRegistry.h"
#include "../DataModel/InstanceClasses/ShapePart.h"
#include "../DataModel/InstanceClasses/Lighting.h"

#include <iostream>

namespace Sunvoltum {
namespace Net {

// ---------------------------------------------------------------------------
ServerReplicator& ServerReplicator::Get()
{
    static ServerReplicator s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
void ServerReplicator::Init(DataModel& dm)
{
    if (m_initialized)
    {
        std::cerr << "[ServerReplicator] Init called twice, ignoring\n";
        return;
    }

    m_dataModel = &dm;

    // Обходим всё дерево DataModel:
    // присваиваем NetId + навешиваем ChildAdded/ChildRemoved/Property подписки.
    RegisterRecursive(dm);

    m_initialized = true;
    std::cout << "[ServerReplicator] Init complete\n";
}

// ---------------------------------------------------------------------------
void ServerReplicator::MarkPeerReady(NetworkId peerId)
{
    m_readyPeers.insert(peerId);
    m_updateSeq[peerId]; // инициализируем пустой map для этого пира
    std::cout << "[ServerReplicator] Peer " << peerId << " marked ready for replication\n";
}

// ---------------------------------------------------------------------------
void ServerReplicator::MarkPeerGone(NetworkId peerId)
{
    m_readyPeers.erase(peerId);
    m_updateSeq.erase(peerId);
}

// ---------------------------------------------------------------------------
void ServerReplicator::Shutdown()
{
    m_childAddedTokens.clear();
    m_childRemovedTokens.clear();
    m_propTokens.clear();
    m_readyPeers.clear();
    m_updateSeq.clear();
    m_pendingUpdates.clear();
    m_dataModel    = nullptr;
    m_initialized  = false;
}

// ---------------------------------------------------------------------------
// RegisterRecursive
// Обходит контейнер, для каждого Instance:
//   1. Присваивает InstanceNetId (если ещё нет)
//   2. Подписывается на его свойства
//   3. Рекурсивно обходит детей
//   4. Подписывается на ChildAdded/ChildRemoved контейнера
// ---------------------------------------------------------------------------
void ServerReplicator::RegisterRecursive(InstanceParent& node)
{
    // Подписка на ChildAdded этого контейнера
    m_childAddedTokens.push_back(
        node.SubscribeChildAdded([this](Instance& child)
        {
            // Новый объект — присваиваем NetId, подписываемся, рассылаем
            InstanceRegistry::Get().Assign(child);
            SubscribeProperties(child);

            // Рекурсивно обрабатываем детей нового объекта (если у него уже есть)
            // и подписываемся на его ChildAdded/ChildRemoved
            RegisterRecursive(child);

            BroadcastInstanceAdded(child);
        }));

    // Подписка на ChildRemoved этого контейнера
    m_childRemovedTokens.push_back(
        node.SubscribeChildRemoved([this](Instance& child)
        {
            InstanceNetId netId = child.GetNetId();
            if (netId == INVALID_INSTANCE_NET_ID) return;

            // Удаляем property-подписки
            m_propTokens.erase(netId);

            // Удаляем из реестра
            InstanceRegistry::Get().Unregister(netId);

            BroadcastInstanceRemoved(netId);
        }));

    // Обходим существующих детей (если это Instance, а не DataModel-корень)
    for (auto& childPtr : node.GetChildren())
    {
        Instance& child = *childPtr;

        // Присваиваем NetId только если ещё не назначен
        if (child.GetNetId() == INVALID_INSTANCE_NET_ID)
            InstanceRegistry::Get().Assign(child);

        // Подписываемся на свойства
        SubscribeProperties(child);

        // Рекурсия вглубь
        RegisterRecursive(child);
    }
}

// ---------------------------------------------------------------------------
// IsFrequentProperty
// ---------------------------------------------------------------------------
bool ServerReplicator::IsFrequentProperty(PropertyId propId)
{
    using SP = Sunvoltum::Classes::ShapePart;
    using LT = Sunvoltum::Classes::Lighting;

    return propId == SP::CFrame       ||
           propId == SP::PosVelocity  ||
           propId == SP::RotVelocity  ||
           propId == LT::ClockTime;
}

// ---------------------------------------------------------------------------
// GetRateMs — минимальный интервал отправки для свойства
// ---------------------------------------------------------------------------
uint32_t ServerReplicator::GetRateMs(PropertyId propId)
{
    using SP = Sunvoltum::Classes::ShapePart;

    // Физические тела обновляются на 240 Hz — ограничиваем до ~30 раз/сек
    if (propId == SP::CFrame      ||
        propId == SP::PosVelocity ||
        propId == SP::RotVelocity)
        return RATE_PHYSICS_MS; // 33 мс

    // ClockTime и другие частые свойства — 20 раз/сек
    return RATE_DEFAULT_MS; // 50 мс
}

// ---------------------------------------------------------------------------
// SubscribeProperties
// Горячие свойства → BroadcastPropertyUpdate (Unreliable+Sequenced)
// Остальные        → BroadcastPropertyChanged (Reliable)
// ---------------------------------------------------------------------------
void ServerReplicator::SubscribeProperties(Instance& inst)
{
    InstanceNetId netId = inst.GetNetId();
    if (netId == INVALID_INSTANCE_NET_ID) return;

    auto& tokens = m_propTokens[netId];

    for (const auto& [propId, entry] : inst.GetProperties())
    {
        if (IsFrequentProperty(propId))
        {
            tokens.push_back(
                PropertyManager::Get().Subscribe(&inst, propId,
                    [this, netId, propId](const PropertyValue& value)
                    {
                        BroadcastPropertyUpdate(netId, propId, value);
                    }));
        }
        else
        {
            tokens.push_back(
                PropertyManager::Get().Subscribe(&inst, propId,
                    [this, netId, propId](const PropertyValue& value)
                    {
                        BroadcastPropertyChanged(netId, propId, value);
                    }));
        }
    }
}

// ---------------------------------------------------------------------------
// BroadcastInstanceAdded
// ---------------------------------------------------------------------------
void ServerReplicator::BroadcastInstanceAdded(Instance& inst)
{
    if (m_readyPeers.empty()) return;

    InstanceNetId netId = inst.GetNetId();
    if (netId == INVALID_INSTANCE_NET_ID) return;

    // Определяем parentNetId
    InstanceNetId parentNetId = INVALID_INSTANCE_NET_ID; // 0 = корень DataModel
    InstanceParent* parent = inst.GetParent();
    if (parent)
    {
        // parent может быть Instance (у него есть GetNetId) или DataModel (нет)
        Instance* parentInst = dynamic_cast<Instance*>(parent);
        if (parentInst)
            parentNetId = parentInst->GetNetId();
    }

    // Функтор Instance* → SerializeId (здесь используем InstanceNetId напрямую)
    auto instToId = [](Instance* ptr) -> uint16_t {
        if (!ptr) return SERIALIZE_ID_NONE;
        InstanceNetId nid = ptr->GetNetId();
        // InstanceNetId uint32_t, SerializeId uint16_t — для InstanceRef
        // в PropertyChanged netId передаётся как uint32_t отдельно.
        // Здесь для совместимости с WritePropertyValue обрезаем до uint16_t.
        // На клиенте ClientReplicator использует InstanceRegistry::Find(netId).
        return (nid <= 0xFFFEu) ? static_cast<uint16_t>(nid) : SERIALIZE_ID_NONE;
    };

    PacketWriter pkt(PacketType::InstanceAdded, /*reliable=*/true);
    pkt.WriteU32(netId);
    pkt.WriteU32(parentNetId);
    pkt.WriteU8(static_cast<uint8_t>(inst.GetClassId()));
    pkt.WriteString(inst.GetName());

    // Свойства
    const auto& props = inst.GetProperties();
    uint8_t propCount = static_cast<uint8_t>(props.size() > 255 ? 255 : props.size());
    pkt.WriteU8(propCount);

    uint8_t written = 0;
    for (const auto& [propId, entry] : props)
    {
        if (written >= propCount) break;
        WritePropertyValue(pkt, propId, entry.Value, instToId);
        ++written;
    }

    for (NetworkId peer : m_readyPeers)
        NetworkServer::Get().SendReliable(peer, pkt);
}

// ---------------------------------------------------------------------------
// BroadcastInstanceRemoved
// ---------------------------------------------------------------------------
void ServerReplicator::BroadcastInstanceRemoved(InstanceNetId netId)
{
    if (m_readyPeers.empty()) return;

    PacketWriter pkt(PacketType::InstanceRemoved, /*reliable=*/true);
    pkt.WriteU32(netId);

    for (NetworkId peer : m_readyPeers)
        NetworkServer::Get().SendReliable(peer, pkt);
}

// ---------------------------------------------------------------------------
// BroadcastPropertyChanged — Reliable
// ---------------------------------------------------------------------------
void ServerReplicator::BroadcastPropertyChanged(InstanceNetId        netId,
                                                 PropertyId           propId,
                                                 const PropertyValue& value)
{
    if (m_readyPeers.empty()) return;

    auto instToId = [](Instance* ptr) -> uint16_t {
        if (!ptr) return SERIALIZE_ID_NONE;
        InstanceNetId nid = ptr->GetNetId();
        return (nid <= 0xFFFEu) ? static_cast<uint16_t>(nid) : SERIALIZE_ID_NONE;
    };

    PacketWriter pkt(PacketType::PropertyChanged, /*reliable=*/true);
    pkt.WriteU32(netId);
    WritePropertyValue(pkt, propId, value, instToId);

    for (NetworkId peer : m_readyPeers)
        NetworkServer::Get().SendReliable(peer, pkt);
}

// ---------------------------------------------------------------------------
// BroadcastPropertyUpdate — Unreliable + Sequenced + Rate-limited
//
// При вызове:
//   - Если с последней отправки прошло >= GetRateMs(propId) → отправить сразу
//   - Иначе → запомнить значение как dirty, отправить в Tick() когда истечёт
//
// Это гарантирует что клиент всегда получит последнее значение,
// но не чаще чем раз в GetRateMs миллисекунд.
// ---------------------------------------------------------------------------
void ServerReplicator::BroadcastPropertyUpdate(InstanceNetId        netId,
                                                PropertyId           propId,
                                                const PropertyValue& value)
{
    if (m_readyPeers.empty()) return;

    PropKey key{ netId, propId };
    auto now = std::chrono::steady_clock::now();
    uint32_t rateMs = GetRateMs(propId);

    auto& pending = m_pendingUpdates[key];

    // Всегда запоминаем последнее значение
    pending.lastValue = value;
    pending.dirty     = true;

    // Проверяем: можно ли отправить прямо сейчас?
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - pending.lastSentTime).count();

    if (static_cast<uint32_t>(elapsed) < rateMs)
        return; // слишком рано — отправим в Tick()

    // Отправляем немедленно
    DoSendPropertyUpdate(key, pending.lastValue, now);
    pending.dirty = false;
}

// ---------------------------------------------------------------------------
// DoSendPropertyUpdate — внутренняя отправка без rate-проверки
// ---------------------------------------------------------------------------
void ServerReplicator::DoSendPropertyUpdate(
    const PropKey& key, const PropertyValue& value,
    std::chrono::steady_clock::time_point sendTime)
{
    auto instToId = [](Instance* ptr) -> uint16_t {
        if (!ptr) return SERIALIZE_ID_NONE;
        InstanceNetId nid = ptr->GetNetId();
        return (nid <= 0xFFFEu) ? static_cast<uint16_t>(nid) : SERIALIZE_ID_NONE;
    };

    for (NetworkId peer : m_readyPeers)
    {
        uint32_t seq = m_updateSeq[peer][key]++;

        PacketWriter pkt(PacketType::PropertyUpdate, /*reliable=*/false);
        pkt.WriteU32(seq);
        pkt.WriteU32(key.netId);
        WritePropertyValue(pkt, key.propId, value, instToId);

        NetworkServer::Get().SendUnreliableTo(peer, pkt);
    }

    m_pendingUpdates[key].lastSentTime = sendTime;
}

// ---------------------------------------------------------------------------
// FlushPendingUpdates — отправка накопленных значений у которых истёк интервал
// ---------------------------------------------------------------------------
void ServerReplicator::FlushPendingUpdates()
{
    if (m_readyPeers.empty()) return;

    auto now = std::chrono::steady_clock::now();

    for (auto& [key, pending] : m_pendingUpdates)
    {
        if (!pending.dirty) continue;

        uint32_t rateMs = GetRateMs(key.propId);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - pending.lastSentTime).count();

        if (static_cast<uint32_t>(elapsed) < rateMs) continue;

        DoSendPropertyUpdate(key, pending.lastValue, now);
        pending.dirty = false;
    }
}

// ---------------------------------------------------------------------------
// Tick — вызывать из Heartbeat сервера каждый кадр
// ---------------------------------------------------------------------------
void ServerReplicator::Tick()
{
    FlushPendingUpdates();
}

} // namespace Net
} // namespace Sunvoltum
