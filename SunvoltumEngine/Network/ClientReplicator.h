#pragma once

// ---------------------------------------------------------------------------
// ClientReplicator.h — client-side обработка live-репликации
//
//   InstanceAdded    (Reliable)            — создать объект
//   InstanceRemoved  (Reliable)            — удалить объект
//   PropertyChanged  (Reliable)            — структурные свойства (цвет, размер...)
//   PropertyUpdate   (Unreliable+Sequenced) — горячие свойства (CFrame, ClockTime...)
//
// Joint-пропагация (Weld и будущие Motor6D):
//   Когда приходит PropertyUpdate для Part0.CFrame, ClientReplicator
//   немедленно вычисляет Part1.CFrame = Part0.CFrame * C0 * Inv(C1)
//   для всех суставов у которых данная часть является Part0.
//   Это устраняет визуальный разрыв при потере пакетов на Part1.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../LibSunvoltum.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/PropertyManager.h"
#include "../Network/NetworkManager.h"
#include "../Types/CFrame.h"
#include "NetworkPacket.h"

namespace Sunvoltum {
    class PhysicsBridge;
namespace Net {

    class LibSunvoltum ClientReplicator
    {
    public:
        // -----------------------------------------------------------------------
        // JointTracker — запись об одном Joint-инстансе (Weld, Motor6D, ...) на клиенте.
        //
        // Объявлен первым в public-секции чтобы PropagateJoint мог ссылаться
        // на него в своей сигнатуре (C++ требует тип до его использования).
        // -----------------------------------------------------------------------
        struct JointTracker
        {
            Instance*  jointInst = nullptr; // сам Joint-инстанс (Weld, Motor6D, ...)
            Instance*  part0     = nullptr;
            Instance*  part1     = nullptr;

            CFrame c0;
            CFrame c1;

            // Подписка на Part0.CFrame — активна пока Joint жив.
            PropertyToken part0CFrameToken;

            // Подписки на Joint.Part0 / Joint.Part1 — только в live-режиме
            // (когда Part0 ещё не известен в момент RegisterJoint).
            PropertyToken jointPart0Token;
            PropertyToken jointPart1Token;

            // Motor6D: подписка на CurrentAngle — пересчитываем Part1 при каждом
            // обновлении угла (Part0=PropBase Anchored → Part0.CFrame не меняется).
            PropertyToken currentAngleToken;
        };

        // -----------------------------------------------------------------------
        // Ключ для таблицы sequence
        // -----------------------------------------------------------------------
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
                size_t h = static_cast<size_t>(k.netId);
                h ^= h >> 16;
                h *= 0x45d9f3bULL;
                h ^= h >> 16;
                h ^= static_cast<size_t>(k.propId) * 0x9e3779b9ULL;
                return h;
            }
        };

        // -----------------------------------------------------------------------
        // Интерфейс
        // -----------------------------------------------------------------------
        static ClientReplicator& Get();

        ClientReplicator(const ClientReplicator&)            = delete;
        ClientReplicator& operator=(const ClientReplicator&) = delete;

        void SetDataModel(DataModel* dm);
        void SetPhysicsBridge(PhysicsBridge* pb) { m_physicsBridge = pb; }

        void OnInstanceAdded    (PacketReader& r);
        void OnInstanceRemoved  (PacketReader& r);
        void OnPropertyChanged  (PacketReader& r);
        void OnPropertyUpdate   (PacketReader& r);

        // Вызывать из SetOnComplete после завершения десериализации.
        // К этому моменту все InstanceRef (Part0/Part1) уже резолвнуты.
        void PostDeserialize(DataModel& dm);

        // Регистрирует один Joint-инстанс и настраивает пропагацию.
        // Public — вызывается из свободной функции RegisterJointsRecursive в .cpp.
        void RegisterJoint(Instance* jointInst);

        // Вычисляет Part1.CFrame = Part0.CFrame * C0 * Inv(C1) и применяет.
        // Public — вызывается из лямбд подписок.
        // Для Motor6D здесь можно будет учитывать DesiredAngle вместо фиксированного C1.
        void PropagateJoint(JointTracker& joint, const CFrame& part0CF);

        // Обновляет все суставы, где part0Inst является Part0 (используется при интерполяции Part0)
        void PropagatePart0Joints(Instance* part0Inst, const CFrame& part0CF);

    private:
        ClientReplicator() = default;
        ~ClientReplicator() = default;

        // Все суставы (Weld, Motor6D и т.д.): ключ — reinterpret_cast<uintptr_t>(Joint Instance*)
        std::unordered_map<uintptr_t, JointTracker>             m_joints;
        std::unordered_map<uintptr_t, std::vector<uintptr_t>>   m_part0ToJoints;
        // Part1* → список jointKey: используется в OnPropertyUpdate чтобы
        // не перезаписывать CFrame Part1 пакетом с сервера —
        // его позиция авторитетно вычислена через PropagateJoint.
        std::unordered_map<uintptr_t, std::vector<uintptr_t>>   m_part1ToJoints;

        DataModel* m_dataModel = nullptr;
        PhysicsBridge* m_physicsBridge = nullptr;
        std::unordered_map<PropKey, uint32_t, PropKeyHash> m_lastUpdateSeq;
    };

} // namespace Net
} // namespace Sunvoltum
