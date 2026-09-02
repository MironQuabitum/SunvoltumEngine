#include "SceneDeserializer.h"
#include "NetworkClient.h"
#include "../DataModel/InstanceRegistry.h"

#include <iostream>
#include <algorithm>
namespace Sunvoltum {
namespace Net {

// ---------------------------------------------------------------------------
// Синглтон
// ---------------------------------------------------------------------------
SceneDeserializer& SceneDeserializer::Get()
{
    static SceneDeserializer s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
void SceneDeserializer::SetOnComplete(std::function<void()> cb)
{
    m_onComplete = std::move(cb);
}

void SceneDeserializer::SetDataModel(DataModel* dm)
{
    m_dataModel = dm;
}

bool SceneDeserializer::IsComplete() const { return m_complete; }

// ---------------------------------------------------------------------------
void SceneDeserializer::Reset()
{
    m_token           = 0;
    m_totalCount      = 0;
    m_serverChecksum  = 0;
    m_endReceived     = false;
    m_complete        = false;
    m_received.clear();
    // m_dataModel намеренно НЕ сбрасывается — устанавливается один раз
    // через SetDataModel() до старта игрового цикла и остаётся валидным.
}

// ---------------------------------------------------------------------------
// OnStartSerialization — S→C
// ---------------------------------------------------------------------------
void SceneDeserializer::OnStartSerialization(PacketReader& r)
{
    uint16_t totalCount = 0;
    uint32_t token      = 0;
    if (!r.ReadU16(totalCount)) return;
    if (!r.ReadU32(token))      return;

    // Защита от повторной доставки Reliable-пакета:
    // если токен совпадает — просто повторяем ReadySerialization и выходим.
    if (m_token != 0 && token == m_token)
    {
        std::cout << "[SceneDeserializer] Duplicate StartSerialization, re-sending Ready\n";
        PacketWriter pkt(PacketType::ReadySerialization, /*reliable=*/true);
        pkt.WriteU32(m_token);
        NetworkClient::Get().SendReliable(pkt);
        return;
    }

    Reset();
    m_totalCount = totalCount;
    m_token      = token;

    std::cout << "[SceneDeserializer] StartSerialization: total=" << totalCount
              << " token=0x" << std::hex << token << std::dec << "\n";

    // Ответ ReadySerialization — Reliable
    PacketWriter pkt(PacketType::ReadySerialization, /*reliable=*/true);
    pkt.WriteU32(m_token);
    NetworkClient::Get().SendReliable(pkt);
}

// ---------------------------------------------------------------------------
// OnNewInstance — S→C
// ---------------------------------------------------------------------------
void SceneDeserializer::OnNewInstance(PacketReader& r)
{
    // Сериализация уже завершена — это Reliable-повтор неподтверждённого пакета.
    // Просто игнорируем: DataModel уже заполнен, ACK дойдёт в следующем Poll().
    if (m_complete) return;

    // --- заголовок объекта ---
    uint32_t token = 0;
    if (!r.ReadU32(token)) return;

    // Проверка токена — отбрасываем «чужие» пакеты
    if (token != m_token)
    {
        std::cerr << "[SceneDeserializer] OnNewInstance: token mismatch, dropping\n";
        return;
    }

    uint32_t netIdRaw    = 0;
    uint16_t serializeId = 0;
    uint16_t parentId    = 0;
    uint8_t  classIdRaw  = 0;
    std::string name;

    if (!r.ReadU32(netIdRaw))    return;
    if (!r.ReadU16(serializeId)) return;
    if (!r.ReadU16(parentId))    return;
    if (!r.ReadU8(classIdRaw))   return;
    if (!r.ReadString(name))     return;

    // Дубликат — сервер мог прислать повторно через AskInstance, берём новую копию
    InstanceDesc desc;
    desc.serializeId = serializeId;
    desc.parentId    = parentId;
    desc.netId       = static_cast<InstanceNetId>(netIdRaw);
    desc.classId     = static_cast<int8_t>(classIdRaw);
    desc.name        = std::move(name);

    // --- свойства ---
    uint8_t propCount = 0;
    if (!r.ReadU8(propCount)) return;

    for (uint8_t i = 0; i < propCount; ++i)
    {
        PropertyId    propId = 0;
        PropertyValue value;
        uint16_t      refId  = SERIALIZE_ID_NONE;

        if (!ReadPropertyValue(r, propId, value, refId))
        {
            std::cerr << "[SceneDeserializer] OnNewInstance: property read error at prop "
                      << static_cast<int>(i) << " of serializeId=" << serializeId << "\n";
            return;
        }

        if (value.Type == PropertyType::InstanceRef && refId != SERIALIZE_ID_NONE)
        {
            // Откладываем связывание
            desc.instanceRefs.emplace_back(propId, refId);
        }
        else if (value.Type != PropertyType::InstanceRef)
        {
            desc.properties.emplace_back(propId, value);
        }
        // InstanceRef с refId == SERIALIZE_ID_NONE (nullptr) — просто пропускаем
    }

    std::cout << "[SceneDeserializer] NewInstance serializeId=" << serializeId
              << " class=" << static_cast<int>(desc.classId)
              << " name=" << desc.name << "\n";

    m_received[serializeId] = std::move(desc);

    // После каждого нового пакета проверяем — вдруг все дыры закрыты
    if (m_endReceived && m_dataModel)
        TryFinalize(m_dataModel);
}

// ---------------------------------------------------------------------------
// OnEndSerialization — S→C
// ---------------------------------------------------------------------------
void SceneDeserializer::OnEndSerialization(PacketReader& r)
{
    // Reliable-повтор после завершения — игнорируем
    if (m_complete) return;

    uint32_t token    = 0;
    uint32_t checksum = 0;
    if (!r.ReadU32(token))    return;
    if (!r.ReadU32(checksum)) return;

    if (token != m_token)
    {
        std::cerr << "[SceneDeserializer] OnEndSerialization: token mismatch, dropping\n";
        return;
    }

    m_serverChecksum = checksum;
    m_endReceived    = true;

    std::cout << "[SceneDeserializer] EndSerialization received. "
              << "Got " << m_received.size() << "/" << m_totalCount << " objects\n";

    TryFinalize(m_dataModel);
}

// ---------------------------------------------------------------------------
// TryFinalize — проверить готовность и завершить
// ---------------------------------------------------------------------------
void SceneDeserializer::TryFinalize(DataModel* dm)
{
    if (m_complete) return;
    if (!m_endReceived) return;

    // Найти все пропущенные SerializeId
    auto missing = GetMissingIds();

    if (!missing.empty())
    {
        std::cout << "[SceneDeserializer] TryFinalize: " << missing.size()
                  << " missing ids, sending AskInstance\n";
        SendAskInstance();
        return;
    }

    // Проверка checksum
    uint32_t localChecksum = 0;
    for (const auto& [sid, _] : m_received)
        localChecksum ^= sid;

    if (localChecksum != m_serverChecksum)
    {
        std::cerr << "[SceneDeserializer] TryFinalize: checksum mismatch!"
                  << " local=0x" << std::hex << localChecksum
                  << " server=0x" << m_serverChecksum << std::dec << "\n";
        // Запрашиваем все объекты заново
        SendAskInstance();
        return;
    }

    // Все объекты получены и checksum совпал
    std::cout << "[SceneDeserializer] All " << m_totalCount
              << " objects received, checksum OK. Applying to DataModel...\n";

    if (dm)
        ApplyTo(*dm);

    SendComplete();
    m_complete = true;

    if (m_onComplete)
        m_onComplete();
}

// ---------------------------------------------------------------------------
// GetMissingIds
// ---------------------------------------------------------------------------
std::vector<uint16_t> SceneDeserializer::GetMissingIds() const
{
    std::vector<uint16_t> missing;
    missing.reserve(m_totalCount);
    for (uint16_t i = 0; i < m_totalCount; ++i)
    {
        if (m_received.find(i) == m_received.end())
            missing.push_back(i);
    }
    return missing;
}

// ---------------------------------------------------------------------------
// SendAskInstance
// ---------------------------------------------------------------------------
void SceneDeserializer::SendAskInstance() const
{
    auto missing = GetMissingIds();
    if (missing.empty()) return;

    // AskInstance ограничен MAX_PAYLOAD_SIZE.
    // Каждый id = 2 байта, заголовок запроса = 4 (token) + 2 (count) = 6 байт.
    // В один пакет влезает (MAX_PAYLOAD_SIZE - 6) / 2 ids.
    constexpr size_t HEADER_BYTES  = 6; // token(4) + count(2)
    constexpr size_t MAX_IDS_PER_PKT =
        (MAX_PAYLOAD_SIZE - HEADER_BYTES) / sizeof(uint16_t);

    size_t offset = 0;
    while (offset < missing.size())
    {
        size_t batchSize = std::min(MAX_IDS_PER_PKT, missing.size() - offset);

        PacketWriter pkt(PacketType::AskInstance, /*reliable=*/true);
        pkt.WriteU32(m_token);
        pkt.WriteU16(static_cast<uint16_t>(batchSize));

        for (size_t i = 0; i < batchSize; ++i)
            pkt.WriteU16(missing[offset + i]);

        NetworkClient::Get().SendReliable(pkt);
        offset += batchSize;
    }

    std::cout << "[SceneDeserializer] AskInstance: requested " << missing.size()
              << " missing ids in " << ((missing.size() + MAX_IDS_PER_PKT - 1) / MAX_IDS_PER_PKT)
              << " packet(s)\n";
}

// ---------------------------------------------------------------------------
// SendComplete
// ---------------------------------------------------------------------------
void SceneDeserializer::SendComplete() const
{
    PacketWriter pkt(PacketType::SerializationComplete, /*reliable=*/true);
    pkt.WriteU32(m_token);
    NetworkClient::Get().SendReliable(pkt);
    std::cout << "[SceneDeserializer] SerializationComplete sent\n";
}

// ---------------------------------------------------------------------------
// ApplyTo — применяем все объекты к DataModel
// ---------------------------------------------------------------------------
void SceneDeserializer::ApplyTo(DataModel& dm)
{
    // Сохраняем ссылку для TryFinalize после AskInstance-повторов
    m_dataModel = &dm;

    if (m_received.empty()) return;

    // Защита от двойного вызова
    if (m_complete) return;

    // Таблица serializeId → созданный Instance*
    std::unordered_map<uint16_t, Instance*> createdInstances;
    createdInstances.reserve(m_received.size());

    // Обходим в порядке возрастания serializeId.
    // BFS на сервере гарантирует: parentId < serializeId всегда,
    // поэтому обход по возрастанию id = топологический порядок.
    std::vector<uint16_t> order;
    order.reserve(m_received.size());
    for (const auto& [sid, _] : m_received)
        order.push_back(sid);
    std::sort(order.begin(), order.end());

    // Первый проход: создать объекты и выставить обычные свойства
    for (uint16_t sid : order)
    {
        const InstanceDesc& desc = m_received.at(sid);

        // Найти родителя
        InstanceParent* parent = nullptr;
        if (desc.parentId == SERIALIZE_ID_NONE)
        {
            // Корень DataModel — проверяем не существует ли уже такой объект
            // (Workspace, Lighting создаются клиентом как заглушки)
            Instance* existing = dm.FindByName(desc.name);
            if (existing && existing->GetClassId() == desc.classId)
            {
                // Переиспользуем заглушку — обновляем свойства и регистрируем в реестре
                createdInstances[sid] = existing;
                if (desc.netId != INVALID_INSTANCE_NET_ID)
                    InstanceRegistry::Get().Register(desc.netId, *existing);
                for (const auto& [propId, value] : desc.properties)
                    existing->SetProperty(propId, value);
                continue;
            }
            parent = &dm;
        }
        else
        {
            auto it = createdInstances.find(desc.parentId);
            if (it == createdInstances.end())
            {
                // Родитель не найден — это ошибка (BFS должен гарантировать порядок)
                std::cerr << "[SceneDeserializer] ApplyTo: parent serializeId="
                          << desc.parentId << " not found for " << desc.name
                          << " (sid=" << sid << "), skipping\n";
                continue;
            }
            parent = it->second;
        }

        // Создаём новый Instance
        Instance& inst = parent->AddInstance(desc.name, desc.classId);
        createdInstances[sid] = &inst;

        // Регистрируем в InstanceRegistry чтобы ClientReplicator мог находить объект по netId
        if (desc.netId != INVALID_INSTANCE_NET_ID)
            InstanceRegistry::Get().Register(desc.netId, inst);

        // Выставляем обычные свойства
        for (const auto& [propId, value] : desc.properties)
            inst.SetProperty(propId, value);
    }

    // Второй проход: разрешаем InstanceRef
    for (uint16_t sid : order)
    {
        const InstanceDesc& desc = m_received.at(sid);
        auto instIt = createdInstances.find(sid);
        if (instIt == createdInstances.end()) continue;

        Instance* inst = instIt->second;

        for (const auto& [propId, refSid] : desc.instanceRefs)
        {
            if (refSid == SERIALIZE_ID_NONE) continue;

            auto refIt = createdInstances.find(refSid);
            if (refIt == createdInstances.end())
            {
                std::cerr << "[SceneDeserializer] ApplyTo: InstanceRef serializeId="
                          << refSid << " not found for prop "
                          << static_cast<int>(propId) << " of " << desc.name << "\n";
                continue;
            }

            inst->SetProperty(propId, PropertyValue::Ref(refIt->second));
        }
    }

    std::cout << "[SceneDeserializer] ApplyTo complete: "
              << createdInstances.size() << " objects applied\n";
}

} // namespace Net
} // namespace Sunvoltum
