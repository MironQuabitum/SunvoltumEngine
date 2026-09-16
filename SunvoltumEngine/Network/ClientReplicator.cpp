#include "ClientReplicator.h"
#include "NetworkSerializer.h"
#include "../DataModel/InstanceRegistry.h"
#include "../DataModel/InstanceClasses/JointInstance.h"
#include "../DataModel/InstanceClasses/Motor6D.h"
#include "../DataModel/InstanceClasses/BasePart.h"
#include "../Types/CFrameUtils.h"

#include <algorithm>
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
    // Сбрасываем sequence-таблицу при смене DataModel (реконнект, перезагрузка сцены).
    m_lastUpdateSeq.clear();
    // Сбрасываем все Joint-записи — токены подписок уничтожаются вместе с JointTracker.
    m_joints.clear();
    m_part0ToJoints.clear();
    m_part1ToJoints.clear();
}

// ---------------------------------------------------------------------------
// OnInstanceAdded — обрабатывает live-добавление объекта (ПОСЛЕ десериализации).
//
// Важно: для Joint-инстансов Part0/Part1 здесь ещё nullptr — они придут позже
// как отдельные PropertyChanged пакеты. RegisterJoint подпишется на Joint.Part0
// чтобы поставить подписку на CFrame как только Part0 придёт.
// ---------------------------------------------------------------------------
void ClientReplicator::OnInstanceAdded(PacketReader& r)
{
    if (!m_dataModel)
    {
        std::cerr << "[ClientReplicator] OnInstanceAdded: DataModel not set\n";
        return;
    }

    uint32_t netId       = 0;
    uint32_t parentNetId = 0;
    uint8_t  classIdRaw  = 0;
    std::string name;

    if (!r.ReadU32(netId))       return;
    if (!r.ReadU32(parentNetId)) return;
    if (!r.ReadU8(classIdRaw))   return;
    if (!r.ReadString(name))     return;

    if (InstanceRegistry::Get().Find(static_cast<InstanceNetId>(netId)))
    {
        std::cerr << "[ClientReplicator] OnInstanceAdded: duplicate netId="
                  << netId << " (" << name << "), skipping\n";
        return;
    }

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

    Instance& inst = parent->AddInstance(name, static_cast<ClassId>(classIdRaw));
    InstanceRegistry::Get().Register(static_cast<InstanceNetId>(netId), inst);

    std::cout << "[ClientReplicator] InstanceAdded netId=" << netId
              << " class=" << static_cast<int>(classIdRaw)
              << " name=" << name << "\n";

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

    // Joint добавленный в live-режиме: Part0/Part1 ещё nullptr, RegisterJoint
    // подпишется на Joint.Part0 и поставит подписку на CFrame когда придёт.
    if (Classes::IsJoint(static_cast<int8_t>(classIdRaw)))
        RegisterJoint(&inst);
}

// ---------------------------------------------------------------------------
// OnInstanceRemoved
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

    InstanceRegistry::Get().Unregister(static_cast<InstanceNetId>(netId));

    // Чистим sequence-записи
    {
        InstanceNetId removedId = static_cast<InstanceNetId>(netId);
        auto it = m_lastUpdateSeq.begin();
        while (it != m_lastUpdateSeq.end())
        {
            if (it->first.netId == removedId)
                it = m_lastUpdateSeq.erase(it);
            else
                ++it;
        }
    }

    // Чистим Joint-записи
    {
        uintptr_t removedPtr = reinterpret_cast<uintptr_t>(inst);

        // A) Сам инстанс — Joint
        auto jointIt = m_joints.find(removedPtr);
        if (jointIt != m_joints.end())
        {
            if (jointIt->second.part0)
            {
                uintptr_t p0Key = reinterpret_cast<uintptr_t>(jointIt->second.part0);
                auto idxIt = m_part0ToJoints.find(p0Key);
                if (idxIt != m_part0ToJoints.end())
                {
                    auto& vec = idxIt->second;
                    vec.erase(std::remove(vec.begin(), vec.end(), removedPtr), vec.end());
                    if (vec.empty())
                        m_part0ToJoints.erase(idxIt);
                }
            }
            if (jointIt->second.part1)
            {
                uintptr_t p1Key = reinterpret_cast<uintptr_t>(jointIt->second.part1);
                auto idxIt = m_part1ToJoints.find(p1Key);
                if (idxIt != m_part1ToJoints.end())
                {
                    auto& vec = idxIt->second;
                    vec.erase(std::remove(vec.begin(), vec.end(), removedPtr), vec.end());
                    if (vec.empty())
                        m_part1ToJoints.erase(idxIt);
                }
            }
            m_joints.erase(jointIt);
        }

        // B) Инстанс был Part0 каких-то Joint
        auto idxIt = m_part0ToJoints.find(removedPtr);
        if (idxIt != m_part0ToJoints.end())
        {
            for (uintptr_t jKey : idxIt->second)
            {
                auto it2 = m_joints.find(jKey);
                if (it2 != m_joints.end())
                {
                    it2->second.part0CFrameToken.Disconnect();
                    it2->second.part0 = nullptr;
                }
            }
            m_part0ToJoints.erase(idxIt);
        }

        // C) Инстанс был Part1 каких-то Joint — обнуляем ссылку
        auto idxIt1 = m_part1ToJoints.find(removedPtr);
        if (idxIt1 != m_part1ToJoints.end())
        {
            for (uintptr_t jKey : idxIt1->second)
            {
                auto it2 = m_joints.find(jKey);
                if (it2 != m_joints.end())
                    it2->second.part1 = nullptr;
            }
            m_part1ToJoints.erase(idxIt1);
        }
    }

    InstanceParent* parent = inst->GetParent();
    if (!parent)
    {
        std::cerr << "[ClientReplicator] OnInstanceRemoved: no parent for netId="
                  << netId << "\n";
        return;
    }

    Instance* parentInst = dynamic_cast<Instance*>(parent);
    if (parentInst)
        parentInst->RemoveChild(inst);
    else
    {
        DataModel* dm = dynamic_cast<DataModel*>(parent);
        if (dm) dm->RemoveChild(inst);
    }
}

// ---------------------------------------------------------------------------
// OnPropertyChanged — Reliable
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
// ---------------------------------------------------------------------------
void ClientReplicator::OnPropertyUpdate(PacketReader& r)
{
    uint32_t seq   = 0;
    uint32_t netId = 0;
    if (!r.ReadU32(seq))   return;
    if (!r.ReadU32(netId)) return;

    InstanceNetId instId = static_cast<InstanceNetId>(netId);

    PropertyId    propId = 0;
    PropertyValue value;
    uint16_t      refId  = SERIALIZE_ID_NONE;
    if (!ReadPropertyValue(r, propId, value, refId)) return;

    PropKey key{ instId, propId };
    auto it = m_lastUpdateSeq.find(key);
    if (it != m_lastUpdateSeq.end())
    {
        uint32_t diff = seq - it->second;
        if (diff == 0 || diff > 0x80000000u)
            return;
        it->second = seq;
    }
    else
    {
        m_lastUpdateSeq[key] = seq;
    }

    Instance* inst = InstanceRegistry::Get().Find(instId);
    if (!inst) return;

    // Если это CFrame-обновление для Part1 активного Joint — игнорируем.
    // Позиция Part1 авторитетно вычисляется локально через PropagateJoint
    // (Part1.CFrame = Part0.CFrame * C0 * Inv(C1)) — серверный пакет
    // пришёл бы с задержкой и затёр бы правильно вычисленное значение.
    if (propId == Classes::BasePart::CFrame)
    {
        uintptr_t instPtr = reinterpret_cast<uintptr_t>(inst);
        if (m_part1ToJoints.count(instPtr))
            return;
    }

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

// ---------------------------------------------------------------------------
// PropagateJoint — применяет transform Joint к Part1.
//
// Вызывается синхронно при каждом обновлении Part0.CFrame.
//
// Weld:
//   Part1.CFrame = Part0.CFrame * C0 * Inv(C1)
//   Классическая формула — C1 фиксирует attachment в пространстве Part1.
//
// Motor6D:
//   Part1.CFrame = Part0.CFrame * C0 * CFrame.Angles(CurrentAngle, 0, 0) * Inv(C1)
//   CurrentAngle — угол вращения вокруг X-оси joint frame, который сервер
//   реплицирует как обычное свойство через reliable channel каждый тик.
//   Клиент применяет его локально без ожидания следующего пакета Part1.CFrame.
// ---------------------------------------------------------------------------
void ClientReplicator::PropagateJoint(JointTracker& joint, const CFrame& part0CF)
{
    if (!joint.part1) return;

    if (joint.jointInst)
    {
        auto* c0prop = joint.jointInst->GetProperty(Classes::JointInstance::C0);
        if (c0prop && c0prop->Type == PropertyType::CFrame)
            joint.c0 = c0prop->Value.AsCFrame;

        auto* c1prop = joint.jointInst->GetProperty(Classes::JointInstance::C1);
        if (c1prop && c1prop->Type == PropertyType::CFrame)
            joint.c1 = c1prop->Value.AsCFrame;

        auto* enProp = joint.jointInst->GetProperty(Classes::JointInstance::Enabled);
        if (enProp && enProp->Type == PropertyType::Bool && !enProp->Value.AsBool)
            return;
    }

    CFrame part1CF;

    if (joint.jointInst &&
        joint.jointInst->GetClassId() == Classes::CLASS_MOTOR6D)
    {
        // Motor6D: учитываем CurrentAngle — вращение вокруг X-оси joint frame
        float currentAngle = 0.0f;
        auto* angProp = joint.jointInst->GetProperty(Classes::Motor6D::CurrentAngle);
        if (angProp && angProp->Type == PropertyType::Float)
            currentAngle = angProp->Value.AsFloat;

        // CFrame.Angles(currentAngle, 0, 0) — поворот вокруг X (eTWIST)
        CFrame rotation = CFrame::Angles(currentAngle, 0.0f, 0.0f);
        part1CF = part0CF * joint.c0 * rotation * CFrameInverse(joint.c1);
    }
    else
    {
        // Weld (и любой будущий joint без привода): фиксированная формула
        part1CF = part0CF * joint.c0 * CFrameInverse(joint.c1);
    }

    joint.part1->SetProperty(Classes::BasePart::CFrame, PropertyValue::CFrame(part1CF));
}

// ---------------------------------------------------------------------------
// RegisterJoint — регистрирует любой Joint-инстанс и настраивает пропагацию.
//
// Два сценария:
//   A) PostDeserialize: Part0/Part1 уже резолвнуты — подписка на CFrame сразу.
//   B) Live InstanceAdded: Part0=nullptr — подписываемся на Joint.Part0, и когда
//      Part0 придёт через PropertyChanged — ставим подписку на Part0.CFrame.
//
// Добавление Motor6D: никаких изменений здесь не нужно — IsJoint() подхватит
// новый classId, а PropagateJoint будет вызван как обычно.
// ---------------------------------------------------------------------------
void ClientReplicator::RegisterJoint(Instance* jointInst)
{
    if (!jointInst) return;

    uintptr_t jointKey = reinterpret_cast<uintptr_t>(jointInst);
    if (m_joints.count(jointKey)) return;

    JointTracker tracker;
    tracker.jointInst = jointInst;

    auto* p0prop = jointInst->GetProperty(Classes::JointInstance::Part0);
    if (p0prop && p0prop->Type == PropertyType::InstanceRef)
        tracker.part0 = p0prop->Value.AsInstanceRef;

    auto* p1prop = jointInst->GetProperty(Classes::JointInstance::Part1);
    if (p1prop && p1prop->Type == PropertyType::InstanceRef)
        tracker.part1 = p1prop->Value.AsInstanceRef;

    tracker.c0 = CFrame::FromPosition(0.0f, 0.0f, 0.0f);
    auto* c0prop = jointInst->GetProperty(Classes::JointInstance::C0);
    if (c0prop && c0prop->Type == PropertyType::CFrame)
        tracker.c0 = c0prop->Value.AsCFrame;

    tracker.c1 = CFrame::FromPosition(0.0f, 0.0f, 0.0f);
    auto* c1prop = jointInst->GetProperty(Classes::JointInstance::C1);
    if (c1prop && c1prop->Type == PropertyType::CFrame)
        tracker.c1 = c1prop->Value.AsCFrame;

    std::cout << "[ClientReplicator] RegisterJoint (" << jointInst->GetName()
              << "): Part0=" << (tracker.part0 ? tracker.part0->GetName() : "null")
              << " Part1=" << (tracker.part1 ? tracker.part1->GetName() : "null")
              << "\n";

    // Вставляем ДО подписок — лямбды захватывают ссылку на m_joints
    m_joints.emplace(jointKey, std::move(tracker));
    JointTracker& inserted = m_joints.at(jointKey);

    // Регистрируем Part1 в обратном индексе — чтобы OnPropertyUpdate
    // не перезаписывал его CFrame серверным пакетом.
    if (inserted.part1)
    {
        uintptr_t p1Key = reinterpret_cast<uintptr_t>(inserted.part1);
        m_part1ToJoints[p1Key].push_back(jointKey);
    }

    if (inserted.part0)
    {
        // Сценарий A: Part0 известен — подписываемся на CFrame сразу
        Instance*  part0Ptr   = inserted.part0;
        uintptr_t  jointKeyCap = jointKey;

        inserted.part0CFrameToken = PropertyManager::Get().Subscribe(
            part0Ptr, Classes::BasePart::CFrame,
            [this, jointKeyCap](const PropertyValue& val)
            {
                if (val.Type != PropertyType::CFrame) return;
                auto it = m_joints.find(jointKeyCap);
                if (it == m_joints.end()) return;

                // Deferred resolve Part1
                if (!it->second.part1 && it->second.jointInst)
                {
                    auto* p1 = it->second.jointInst->GetProperty(
                        Classes::JointInstance::Part1);
                    if (p1 && p1->Type == PropertyType::InstanceRef)
                        it->second.part1 = p1->Value.AsInstanceRef;
                }

                PropagateJoint(it->second, val.Value.AsCFrame);
            });

        uintptr_t p0Key = reinterpret_cast<uintptr_t>(part0Ptr);
        m_part0ToJoints[p0Key].push_back(jointKey);
    }
    else
    {
        // Сценарий B: Part0 ещё не пришёл — ждём PropertyChanged на Joint.Part0
        uintptr_t jointKeyCap = jointKey;

        inserted.jointPart0Token = PropertyManager::Get().Subscribe(
            jointInst, Classes::JointInstance::Part0,
            [this, jointKeyCap](const PropertyValue& val)
            {
                if (val.Type != PropertyType::InstanceRef) return;
                Instance* newPart0 = val.Value.AsInstanceRef;
                if (!newPart0) return;

                auto it = m_joints.find(jointKeyCap);
                if (it == m_joints.end()) return;

                // Снимаем старую подписку если была
                it->second.part0CFrameToken.Disconnect();
                if (it->second.part0)
                {
                    uintptr_t oldP0Key = reinterpret_cast<uintptr_t>(it->second.part0);
                    auto idxIt = m_part0ToJoints.find(oldP0Key);
                    if (idxIt != m_part0ToJoints.end())
                    {
                        auto& vec = idxIt->second;
                        vec.erase(std::remove(vec.begin(), vec.end(), jointKeyCap),
                                  vec.end());
                        if (vec.empty()) m_part0ToJoints.erase(idxIt);
                    }
                }

                it->second.part0 = newPart0;

                // Ставим подписку на CFrame нового Part0
                it->second.part0CFrameToken = PropertyManager::Get().Subscribe(
                    newPart0, Classes::BasePart::CFrame,
                    [this, jointKeyCap](const PropertyValue& v)
                    {
                        if (v.Type != PropertyType::CFrame) return;
                        auto it2 = m_joints.find(jointKeyCap);
                        if (it2 == m_joints.end()) return;

                        if (!it2->second.part1 && it2->second.jointInst)
                        {
                            auto* p1 = it2->second.jointInst->GetProperty(
                                Classes::JointInstance::Part1);
                            if (p1 && p1->Type == PropertyType::InstanceRef)
                                it2->second.part1 = p1->Value.AsInstanceRef;
                        }

                        PropagateJoint(it2->second, v.Value.AsCFrame);
                    });

                uintptr_t p0Key = reinterpret_cast<uintptr_t>(newPart0);
                m_part0ToJoints[p0Key].push_back(jointKeyCap);
            });

        // Подписка на Part1 чтобы кэшировать указатель
        inserted.jointPart1Token = PropertyManager::Get().Subscribe(
            jointInst, Classes::JointInstance::Part1,
            [this, jointKeyCap](const PropertyValue& val)
            {
                if (val.Type != PropertyType::InstanceRef) return;
                auto it = m_joints.find(jointKeyCap);
                if (it == m_joints.end()) return;

                // Убираем старый Part1 из индекса
                if (it->second.part1)
                {
                    uintptr_t oldP1Key = reinterpret_cast<uintptr_t>(it->second.part1);
                    auto idxIt = m_part1ToJoints.find(oldP1Key);
                    if (idxIt != m_part1ToJoints.end())
                    {
                        auto& vec = idxIt->second;
                        vec.erase(std::remove(vec.begin(), vec.end(), jointKeyCap), vec.end());
                        if (vec.empty()) m_part1ToJoints.erase(idxIt);
                    }
                }

                it->second.part1 = val.Value.AsInstanceRef;

                // Добавляем новый Part1 в индекс
                if (it->second.part1)
                {
                    uintptr_t p1Key = reinterpret_cast<uintptr_t>(it->second.part1);
                    m_part1ToJoints[p1Key].push_back(jointKeyCap);
                }
            });
    }

    // -----------------------------------------------------------------------
    // Motor6D: дополнительная подписка на CurrentAngle.
    //
    // Для Weld часть Part0 двигается сама → подписка на Part0.CFrame достаточна.
    // Для Motor6D Part0 = PropBase (Anchored) → его CFrame никогда не меняется,
    // подписка молчит. Вместо этого подписываемся на CurrentAngle самого мотора —
    // он обновляется сервером каждый тик через unreliable канал.
    // При каждом обновлении CurrentAngle берём кешированный Part0.CFrame
    // и вызываем PropagateJoint чтобы пересчитать Part1.CFrame.
    // -----------------------------------------------------------------------
    if (jointInst->GetClassId() == Classes::CLASS_MOTOR6D)
    {
        uintptr_t jointKeyCap = jointKey;

        inserted.currentAngleToken = PropertyManager::Get().Subscribe(
            jointInst, Classes::Motor6D::CurrentAngle,
            [this, jointKeyCap](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Float) return;

                auto it = m_joints.find(jointKeyCap);
                if (it == m_joints.end()) return;

                JointTracker& jt = it->second;
                if (!jt.part0 || !jt.part1) return;

                // Берём текущий CFrame Part0 из DataModel
                const PropertyValue* cf0prop =
                    jt.part0->GetProperty(Classes::BasePart::CFrame);
                if (!cf0prop || cf0prop->Type != PropertyType::CFrame) return;

                PropagateJoint(jt, cf0prop->Value.AsCFrame);
            });
    }
}

// ---------------------------------------------------------------------------
// PostDeserialize — вызывать из SetOnComplete после завершения десериализации.
//
// К этому моменту ApplyTo выполнил оба прохода и все InstanceRef свойства
// (Part0/Part1 суставов) уже резолвнуты. Обходим DataModel рекурсивно
// и регистрируем все Joint-инстансы — это сценарий A в RegisterJoint.
// ---------------------------------------------------------------------------
static void RegisterJointsRecursive(ClientReplicator& rep, Instance* inst)
{
    if (!inst) return;

    if (Classes::IsJoint(inst->GetClassId()))
    {
        rep.RegisterJoint(inst);
        return;
    }

    for (auto& child : inst->GetChildren())
        RegisterJointsRecursive(rep, child.get());
}

void ClientReplicator::PostDeserialize(DataModel& dm)
{
    std::cout << "[ClientReplicator] PostDeserialize: scanning for joints...\n";
    for (auto& child : dm.GetChildren())
        RegisterJointsRecursive(*this, child.get());
    std::cout << "[ClientReplicator] PostDeserialize: "
              << m_joints.size() << " joint(s) registered\n";
}

} // namespace Net
} // namespace Sunvoltum
