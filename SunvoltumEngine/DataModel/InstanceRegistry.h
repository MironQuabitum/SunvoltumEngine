#pragma once

// ---------------------------------------------------------------------------
// InstanceRegistry — таблица InstanceNetId ↔ Instance*
//
// Сервер назначает InstanceNetId каждому новому объекту через Assign().
// Клиент регистрирует объекты через Register() при получении InstanceAdded
// или применении сериализации.
//
// Используется репликатором чтобы по netId найти нужный Instance,
// не обходя дерево DataModel.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <unordered_map>

#include "../LibSunvoltum.h"
#include "Instance.h"

namespace Sunvoltum {

    class LibSunvoltum InstanceRegistry
    {
    public:
        static InstanceRegistry& Get();

        InstanceRegistry(const InstanceRegistry&)            = delete;
        InstanceRegistry& operator=(const InstanceRegistry&) = delete;

        // --- Сервер ---

        // Назначить новый уникальный InstanceNetId объекту и зарегистрировать его.
        // Вызывать сразу после AddInstance на сервере.
        InstanceNetId Assign(Instance& inst);

        // --- Клиент ---

        // Зарегистрировать объект с уже известным netId (пришёл от сервера).
        void Register(InstanceNetId id, Instance& inst);

        // --- Общие ---

        // Найти Instance по netId. Возвращает nullptr если не найден.
        Instance* Find(InstanceNetId id) const;

        // Удалить запись (вызывается при уничтожении объекта).
        void Unregister(InstanceNetId id);

        // Полная очистка — при переподключении / смене сцены.
        void Clear();

    private:
        InstanceRegistry() = default;
        ~InstanceRegistry() = default;

        InstanceNetId m_nextId = 1; // 0 = INVALID_INSTANCE_NET_ID

        std::unordered_map<InstanceNetId, Instance*> m_idToInst;
    };

} // namespace Sunvoltum
