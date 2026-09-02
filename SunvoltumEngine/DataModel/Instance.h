#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "../LibSunvoltum.h"
#include "InstanceParent.h"
#include "PropertyId.h"
#include "PropertyValue.h"
#include "PropertyManager.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunvoltum {

    using ClassId       = int8_t;
    using InstanceNetId = uint32_t;

    static constexpr InstanceNetId INVALID_INSTANCE_NET_ID = 0;

    struct PropertyEntry
    {
        PropertyValue Value;
        bool          ReadOnly = false;
    };

    class LibSunvoltum Instance : public InstanceParent
    {
    public:
        Instance(const std::string& name, ClassId classId, InstanceParent* parent = nullptr);
        ~Instance();

        Instance(const Instance&)            = delete;
        Instance& operator=(const Instance&) = delete;

        Instance(Instance&&)            = default;
        Instance& operator=(Instance&&) = default;

        const std::string& GetName()    const;
        ClassId            GetClassId() const;

        InstanceParent* GetParent() const;
        void            SetParent(InstanceParent* parent);

        // Постоянный сетевой идентификатор объекта.
        // Назначается сервером через InstanceRegistry::Assign() при создании.
        // На клиенте устанавливается при получении InstanceAdded / сериализации.
        // INVALID_INSTANCE_NET_ID (0) — объект не сетевой (локальный).
        InstanceNetId GetNetId() const;
        void          SetNetId(InstanceNetId id);

        Instance& AddInstance(const std::string& name, ClassId classId) override;
        Instance* FindByName(const std::string& name) override;
        const std::vector<std::unique_ptr<Instance>>& GetChildren() const override;

        // Удалить прямого ребёнка по указателю.
        // Возвращает true если ребёнок найден и удалён, false если не найден.
        // Вызывает FireChildRemoved перед фактическим удалением.
        bool RemoveChild(Instance* child);

        // Записать свойство.
        // readOnly = true  — помечает запись как только для чтения.
        // silent   = true  — значение записывается, но PropertyManager НЕ уведомляется.
        //                    Используется в PhysicsBridge::SyncOut чтобы не вызывать
        //                    рекурсивный цикл уведомлений при обратной записи из физики.
        void SetProperty(PropertyId id, const PropertyValue& value,
                         bool readOnly = false, bool silent = false);
        const PropertyEntry*  GetPropertyEntry(PropertyId id) const;
        const PropertyValue*  GetProperty(PropertyId id) const;
        bool HasProperty(PropertyId id) const;
        bool IsReadOnly(PropertyId id)  const;

        // Доступ ко всем свойствам — используется SceneSerializer для снапшота
        const std::unordered_map<PropertyId, PropertyEntry>& GetProperties() const;

    private:
        std::string                                   m_name;
        ClassId                                       m_classId = 0;
        InstanceNetId                                 m_netId   = INVALID_INSTANCE_NET_ID;
        InstanceParent*                               m_parent  = nullptr;
        std::vector<std::unique_ptr<Instance>>        m_children;
        std::unordered_map<PropertyId, PropertyEntry> m_properties;
    };

} // namespace Sunvoltum

#pragma warning(pop)
