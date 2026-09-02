#include "SceneSerializer.h"
#include "NetworkSerializer.h"
#include "NetworkServer.h"

#include <queue>
#include <random>
#include <iostream>

namespace Sunvoltum {
namespace Net {

// ---------------------------------------------------------------------------
// Синглтон
// ---------------------------------------------------------------------------
SceneSerializer& SceneSerializer::Get()
{
    static SceneSerializer s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
void SceneSerializer::SetOnComplete(std::function<void(NetworkId)> cb)
{
    m_onComplete = std::move(cb);
}

// ---------------------------------------------------------------------------
// FlattenTree — BFS, родители всегда идут раньше детей
// ---------------------------------------------------------------------------
void SceneSerializer::FlattenTree(
    InstanceParent&                           root,
    int8_t                                    skipClassId,
    std::vector<InstanceSnapshot>&            out,
    std::unordered_map<Instance*, uint16_t>&  idMap)
{
    // Очередь: {узел, parentSerializeId}
    // Корень DataModel не является Instance — его дети получают parentId=0xFFFF
    struct QueueEntry
    {
        InstanceParent* node;
        uint16_t        parentId;
    };

    std::queue<QueueEntry> q;

    // Заталкиваем прямых детей корня
    for (auto& childPtr : root.GetChildren())
    {
        q.push({ childPtr.get(), SERIALIZE_ID_NONE });
    }

    while (!q.empty())
    {
        auto [node, parentId] = q.front();
        q.pop();

        Instance* inst = static_cast<Instance*>(node);

        // Пропускаем объекты заданного класса (обычно Player)
        if (skipClassId != 0 && inst->GetClassId() == skipClassId)
            continue;

        uint16_t myId = static_cast<uint16_t>(out.size());
        idMap[inst]   = myId;

        // Собираем снапшот
        InstanceSnapshot snap;
        snap.serializeId = myId;
        snap.parentId    = parentId;
        snap.netId       = inst->GetNetId(); // присвоен ServerReplicator::Init
        snap.classId     = inst->GetClassId();
        snap.name        = inst->GetName();

        // Копируем все свойства кроме ReadOnly (они восстановятся на движке)
        // Исключение: ReadOnly-свойства критичные для отображения (типа Anchored)
        // тоже передаём — клиент сам разберётся что ReadOnly.
        // Мы копируем ВСЕ свойства чтобы клиент получил полную картину.
        for (const auto& [propId, entry] : inst->GetProperties())
        {
            snap.properties.emplace_back(propId, entry.Value);
        }

        out.push_back(std::move(snap));

        // Добавляем детей в очередь
        for (auto& childPtr : inst->GetChildren())
        {
            q.push({ childPtr.get(), myId });
        }
    }
}

// ---------------------------------------------------------------------------
// StartForPeer
// ---------------------------------------------------------------------------
void SceneSerializer::StartForPeer(NetworkId peerId,
                                   InstanceParent& root,
                                   int8_t skipClassId)
{
    // Удаляем старое состояние если есть (переподключение)
    m_states.erase(peerId);

    PeerSerState& state = m_states[peerId];

    // Генерация случайного токена
    {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<uint32_t> dist;
        state.token = dist(rng);
        // Избегаем 0 — зарезервировано как «нет токена»
        if (state.token == 0) state.token = 1;
    }

    // BFS-обход дерева
    FlattenTree(root, skipClassId, state.snapshots, state.instToId);

    state.totalCount = static_cast<uint16_t>(state.snapshots.size());

    // Checksum: XOR всех serializeId
    state.checksum = 0;
    for (const auto& snap : state.snapshots)
        state.checksum ^= snap.serializeId;

    std::cout << "[SceneSerializer] StartForPeer id=" << peerId
              << " objects=" << state.totalCount
              << " token=0x" << std::hex << state.token << std::dec << "\n";

    // Отправка StartSerialization — Reliable
    PacketWriter pkt(PacketType::StartSerialization, /*reliable=*/true);
    pkt.WriteU16(state.totalCount);
    pkt.WriteU32(state.token);
    NetworkServer::Get().SendReliable(peerId, pkt);
}

// ---------------------------------------------------------------------------
// SendNewInstance — формирует и отправляет один NewInstance пакет
// ---------------------------------------------------------------------------
void SceneSerializer::SendNewInstance(
    NetworkId                                        peerId,
    const InstanceSnapshot&                          snap,
    uint32_t                                         token,
    const std::unordered_map<Instance*, uint16_t>&   instToId)
{
    PacketWriter pkt(PacketType::NewInstance, /*reliable=*/true);

    pkt.WriteU32(token);
    pkt.WriteU32(snap.netId);      // InstanceNetId — постоянный id объекта
    pkt.WriteU16(snap.serializeId);
    pkt.WriteU16(snap.parentId);
    pkt.WriteU8(static_cast<uint8_t>(snap.classId));
    pkt.WriteString(snap.name);

    // Количество свойств — запишем заглушку, потом поправим через отдельный счётчик
    uint8_t propCount = static_cast<uint8_t>(
        snap.properties.size() > 255 ? 255 : snap.properties.size());
    pkt.WriteU8(propCount);

    // Функтор для преобразования Instance* → SerializeId
    auto instToIdFn = [&instToId](Instance* ptr) -> uint16_t {
        auto it = instToId.find(ptr);
        if (it != instToId.end()) return it->second;
        return SERIALIZE_ID_NONE;
    };

    uint8_t written = 0;
    for (const auto& [propId, value] : snap.properties)
    {
        if (written >= propCount) break;
        WritePropertyValue(pkt, propId, value, instToIdFn);
        ++written;
    }

    NetworkServer::Get().SendReliable(peerId, pkt);
}

// ---------------------------------------------------------------------------
// OnReadySerialization
// ---------------------------------------------------------------------------
void SceneSerializer::OnReadySerialization(NetworkId peerId, PacketReader& r)
{
    auto it = m_states.find(peerId);
    if (it == m_states.end())
    {
        std::cerr << "[SceneSerializer] OnReadySerialization: unknown peer " << peerId << "\n";
        return;
    }
    PeerSerState& state = it->second;

    uint32_t token = 0;
    if (!r.ReadU32(token)) return;

    if (token != state.token)
    {
        std::cerr << "[SceneSerializer] OnReadySerialization: token mismatch peer=" << peerId << "\n";
        return;
    }

    std::cout << "[SceneSerializer] Peer " << peerId
              << " ready, sending " << state.totalCount << " instances\n";

    // Отправляем все NewInstance подряд
    for (const auto& snap : state.snapshots)
        SendNewInstance(peerId, snap, state.token, state.instToId);

    // EndSerialization — Reliable
    PacketWriter endPkt(PacketType::EndSerialization, /*reliable=*/true);
    endPkt.WriteU32(state.token);
    endPkt.WriteU32(state.checksum);
    NetworkServer::Get().SendReliable(peerId, endPkt);

    std::cout << "[SceneSerializer] EndSerialization sent to peer " << peerId
              << " checksum=0x" << std::hex << state.checksum << std::dec << "\n";
}

// ---------------------------------------------------------------------------
// OnAskInstance — клиент просит повторить пропущенные SerializeId
// ---------------------------------------------------------------------------
void SceneSerializer::OnAskInstance(NetworkId peerId, PacketReader& r)
{
    auto it = m_states.find(peerId);
    if (it == m_states.end())
    {
        std::cerr << "[SceneSerializer] OnAskInstance: unknown peer " << peerId << "\n";
        return;
    }
    PeerSerState& state = it->second;

    uint32_t token = 0;
    if (!r.ReadU32(token)) return;

    if (token != state.token)
    {
        std::cerr << "[SceneSerializer] OnAskInstance: token mismatch peer=" << peerId << "\n";
        return;
    }

    uint16_t missingCount = 0;
    if (!r.ReadU16(missingCount)) return;

    // Защита: не обрабатываем подозрительно большие запросы
    if (missingCount > state.totalCount)
    {
        std::cerr << "[SceneSerializer] OnAskInstance: missingCount=" << missingCount
                  << " > totalCount=" << state.totalCount << ", dropping\n";
        return;
    }

    std::cout << "[SceneSerializer] Peer " << peerId
              << " asks for " << missingCount << " missing instances\n";

    for (uint16_t i = 0; i < missingCount; ++i)
    {
        uint16_t sid = 0;
        if (!r.ReadU16(sid)) break;

        if (sid >= state.snapshots.size())
        {
            std::cerr << "[SceneSerializer] OnAskInstance: serializeId="
                      << sid << " out of range, skipping\n";
            continue;
        }

        SendNewInstance(peerId, state.snapshots[sid], state.token, state.instToId);
    }
}

// ---------------------------------------------------------------------------
// OnSerializationComplete
// ---------------------------------------------------------------------------
void SceneSerializer::OnSerializationComplete(NetworkId peerId, PacketReader& r)
{
    auto it = m_states.find(peerId);
    if (it == m_states.end())
    {
        std::cerr << "[SceneSerializer] OnSerializationComplete: unknown peer " << peerId << "\n";
        return;
    }

    uint32_t token = 0;
    if (!r.ReadU32(token)) return;

    if (token != it->second.token)
    {
        std::cerr << "[SceneSerializer] OnSerializationComplete: token mismatch peer=" << peerId << "\n";
        return;
    }

    std::cout << "[SceneSerializer] Peer " << peerId << " serialization complete\n";

    // Освобождаем память снапшота
    m_states.erase(it);

    if (m_onComplete)
        m_onComplete(peerId);
}

} // namespace Net
} // namespace Sunvoltum
