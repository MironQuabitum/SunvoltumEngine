#include "ClientReplicator.h"
#include "NetworkSerializer.h"
#include "../DataModel/InstanceRegistry.h"

#include <iostream>

namespace Sunvoltum {
namespace Net {

// ---------------------------------------------------------------------------
ClientReplicator& ClientReplicator::Get()
{
    static ClientReplicator s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
void ClientReplicator::SetDataModel(DataModel* dm)
{
    m_dataModel = dm;
}

// ---------------------------------------------------------------------------
// OnInstanceAdded
//
// Payload: uint32_t netId, uint32_t parentNetId, int8_t classId,
//          string name, uint8_t propCount,
//          [uint8_t propId, encoded value] × propCount
// ---------------------------------------------------------------------------
void ClientReplicator::OnInstanceAdded(PacketReader& r)
{
    if (!m_dataModel)
    {
        std::cerr << "[ClientReplicator] OnInstanceAdded: DataModel not set\n";
        return;
    }

    uint32_t netId      = 0;
    uint32_t parentNetId = 0;
    uint8_t  classIdRaw = 0;
    std::string name;

    if (!r.ReadU32(netId))      return;
    if (!r.ReadU32(parentNetId)) return;
    if (!r.ReadU8(classIdRaw))  return;
    if (!r.ReadString(name))    return;

    // Если объект уже есть в реестре — дубликат (повторная доставка Reliable)
    if (InstanceRegistry::Get().Find(static_cast<InstanceNetId>(netId)))
    {
        std::cerr << "[ClientReplicator] OnInstanceAdded: duplicate netId="
                  << netId << " (" << name << "), skipping\n";
        return;
    }

    // Найти родителя
    InstanceParent* parent = nullptr;
    if (parentNetId == INVALID_INSTANCE_NET_ID)
    {
        parent = m_dataModel;
    }
    else
    {
        Instance* parentInst = InstanceRegistry::Get().Find(
            static_cast<InstanceNetId>(parentNetId));
        if (!parentInst)
        {
            std::cerr << "[ClientReplicator] OnInstanceAdded: parent netId="
                      << parentNetId << " not found for " << name << "\n";
            return;
        }
        parent = parentInst;
    }

    // Создать объект
    Instance& inst = parent->AddInstance(name, static_cast<ClassId>(classIdRaw));
    InstanceRegistry::Get().Register(static_cast<InstanceNetId>(netId), inst);

    std::cout << "[ClientReplicator] InstanceAdded netId=" << netId
              << " class=" << static_cast<int>(classIdRaw)
              << " name=" << name << "\n";

    // Применить свойства
    uint8_t propCount = 0;
    if (!r.ReadU8(propCount)) return;

    for (uint8_t i = 0; i < propCount; ++i)
    {
        PropertyId    propId = 0;
        PropertyValue value;
        uint16_t      refId  = SERIALIZE_ID_NONE;

        if (!ReadPropertyValue(r, propId, value, refId))
        {
            std::cerr << "[ClientReplicator] OnInstanceAdded: prop read error at "
                      << static_cast<int>(i) << " of " << name << "\n";
            return;
        }

        if (value.Type == PropertyType::InstanceRef)
        {
            if (refId != SERIALIZE_ID_NONE)
            {
                // refId здесь — усечённый InstanceNetId (uint16_t)
                Instance* ref = InstanceRegistry::Get().Find(
                    static_cast<InstanceNetId>(refId));
                if (ref)
                    inst.SetProperty(propId, PropertyValue::Ref(ref));
                else
                    std::cerr << "[ClientReplicator] OnInstanceAdded: InstanceRef netId="
                              << refId << " not found for prop "
                              << static_cast<int>(propId) << " of " << name << "\n";
            }
        }
        else
        {
            inst.SetProperty(propId, value);
        }
    }
}

// ---------------------------------------------------------------------------
// OnInstanceRemoved
//
// Payload: uint32_t netId
// ---------------------------------------------------------------------------
void ClientReplicator::OnInstanceRemoved(PacketReader& r)
{
    uint32_t netId = 0;
    if (!r.ReadU32(netId)) return;

    Instance* inst = InstanceRegistry::Get().Find(static_cast<InstanceNetId>(netId));
    if (!inst)
    {
        std::cerr << "[ClientReplicator] OnInstanceRemoved: netId="
                  << netId << " not found\n";
        return;
    }

    std::cout << "[ClientReplicator] InstanceRemoved netId=" << netId
              << " name=" << inst->GetName() << "\n";

    // Снять регистрацию ДО удаления (inst будет уничтожен при RemoveChild)
    InstanceRegistry::Get().Unregister(static_cast<InstanceNetId>(netId));

    // Удалить из родителя — это вызовет FireChildRemoved и уничтожит объект
    InstanceParent* parent = inst->GetParent();
    if (!parent)
    {
        std::cerr << "[ClientReplicator] OnInstanceRemoved: no parent for netId="
                  << netId << "\n";
        return;
    }

    // parent может быть DataModel или Instance — оба наследуют InstanceParent
    // RemoveChild определён только у Instance, для DataModel нужен аналог.
    // Проверяем тип через dynamic_cast.
    Instance* parentInst = dynamic_cast<Instance*>(parent);
    if (parentInst)
    {
        parentInst->RemoveChild(inst);
    }
    else
    {
        // Родитель — DataModel
        DataModel* dm = dynamic_cast<DataModel*>(parent);
        if (dm)
            dm->RemoveChild(inst);
    }
}

// ---------------------------------------------------------------------------
// OnPropertyChanged — Reliable
//
// Payload: uint32_t netId, uint8_t propId, encoded PropertyValue
// ---------------------------------------------------------------------------
void ClientReplicator::OnPropertyChanged(PacketReader& r)
{
    uint32_t netId = 0;
    if (!r.ReadU32(netId)) return;

    Instance* inst = InstanceRegistry::Get().Find(static_cast<InstanceNetId>(netId));
    if (!inst)
    {
        std::cerr << "[ClientReplicator] OnPropertyChanged: netId="
                  << netId << " not found\n";
        return;
    }

    PropertyId    propId = 0;
    PropertyValue value;
    uint16_t      refId  = SERIALIZE_ID_NONE;

    if (!ReadPropertyValue(r, propId, value, refId))
    {
        std::cerr << "[ClientReplicator] OnPropertyChanged: read error for netId="
                  << netId << "\n";
        return;
    }

    if (value.Type == PropertyType::InstanceRef)
    {
        if (refId != SERIALIZE_ID_NONE)
        {
            Instance* ref = InstanceRegistry::Get().Find(
                static_cast<InstanceNetId>(refId));
            if (ref)
                inst->SetProperty(propId, PropertyValue::Ref(ref));
            else
                std::cerr << "[ClientReplicator] OnPropertyChanged: InstanceRef netId="
                          << refId << " not found\n";
        }
        else
        {
            inst->SetProperty(propId, PropertyValue::Ref(nullptr));
        }
    }
    else
    {
        inst->SetProperty(propId, value);
    }
}

// ---------------------------------------------------------------------------
// OnPropertyUpdate — Unreliable + Sequenced
//
// Payload: uint32_t sequence, uint32_t netId, uint8_t propId, encoded value
//
// Принимаем только если sequence > последнего принятого для этого netId.
// Это автоматически отбрасывает старые пакеты пришедшие не по порядку.
// ---------------------------------------------------------------------------
void ClientReplicator::OnPropertyUpdate(PacketReader& r)
{
    uint32_t seq   = 0;
    uint32_t netId = 0;
    if (!r.ReadU32(seq))   return;
    if (!r.ReadU32(netId)) return;

    InstanceNetId instId = static_cast<InstanceNetId>(netId);

    // Читаем propId ДО sequence-проверки — он нужен для ключа (netId, propId)
    PropertyId    propId = 0;
    PropertyValue value;
    uint16_t      refId  = SERIALIZE_ID_NONE;
    if (!ReadPropertyValue(r, propId, value, refId)) return;

    // Sequence-проверка per-(netId, propId).
    // CFrame куба и ClockTime освещения имеют независимые счётчики —
    // старый CFrame не может случайно заблокировать новый ClockTime.
    // diff > 0x80000000 с учётом переполнения uint32_t означает «старше».
    ClientReplicator::PropKey key{ instId, propId };
    auto it = m_lastUpdateSeq.find(key);
    if (it != m_lastUpdateSeq.end())
    {
        uint32_t diff = seq - it->second;
        if (diff == 0 || diff > 0x80000000u)
            return; // устаревший или дубликат — выбрасываем
        it->second = seq;
    }
    else
    {
        m_lastUpdateSeq[key] = seq;
    }

    Instance* inst = InstanceRegistry::Get().Find(instId);
    if (!inst) return;

    if (value.Type == PropertyType::InstanceRef)
    {
        if (refId != SERIALIZE_ID_NONE)
        {
            Instance* ref = InstanceRegistry::Get().Find(
                static_cast<InstanceNetId>(refId));
            if (ref) inst->SetProperty(propId, PropertyValue::Ref(ref));
        }
    }
    else
    {
        inst->SetProperty(propId, value);
    }
}

} // namespace Net
} // namespace Sunvoltum
