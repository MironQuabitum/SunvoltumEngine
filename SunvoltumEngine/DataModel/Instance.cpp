#include "Instance.h"
#include "PropertyManager.h"

namespace Sunvoltum {

    Instance::Instance(const std::string& name, ClassId classId, InstanceParent* parent)
        : m_name(name), m_classId(classId), m_parent(parent) {}

    Instance::~Instance()
    {
        // Сообщаем PropertyManager чтобы он удалил все подписки на этот Instance.
        // Это защищает от dangling-pointer коллбэков после удаления объекта из DataModel.
        PropertyManager::Get().NotifyInstanceDestroyed(this);
    }

    const std::string& Instance::GetName()    const { return m_name;    }
    void               Instance::SetName(const std::string& name) { m_name = name; }
    ClassId            Instance::GetClassId() const { return m_classId; }

    InstanceParent* Instance::GetParent() const     { return m_parent; }
    void Instance::SetParent(InstanceParent* p)     { m_parent = p;    }

    InstanceNetId Instance::GetNetId() const        { return m_netId; }
    void          Instance::SetNetId(InstanceNetId id) { m_netId = id; }

    Instance& Instance::AddInstance(const std::string& name, ClassId classId)
    {
        m_children.push_back(std::make_unique<Instance>(name, classId, this));
        Instance& child = *m_children.back();
        FireChildAdded(child);
        return child;
    }

    Instance& Instance::AddInstance(const std::string& name, ClassId classId,
                                     std::function<void(Instance&)> initFn)
    {
        m_children.push_back(std::make_unique<Instance>(name, classId, this));
        Instance& child = *m_children.back();
        if (initFn) initFn(child);   // инициализация ДО FireChildAdded
        FireChildAdded(child);
        return child;
    }

    Instance* Instance::FindByName(const std::string& name)
    {
        for (auto& child : m_children)
            if (child->GetName() == name) return child.get();
        return nullptr;
    }

    const std::vector<std::unique_ptr<Instance>>& Instance::GetChildren() const
    {
        return m_children;
    }

    bool Instance::RemoveChild(Instance* child)
    {
        for (auto it = m_children.begin(); it != m_children.end(); ++it)
        {
            if (it->get() == child)
            {
                FireChildRemoved(*child);
                m_children.erase(it);
                return true;
            }
        }
        return false;
    }

    void Instance::SetProperty(PropertyId id, const PropertyValue& value,
                               bool readOnly, bool silent)
    {
        m_properties[id] = { value, readOnly };

        if (!silent && PropertyManager::Get().HasAnySubscribers())
            PropertyManager::Get().Notify(this, id, value);
    }

    const PropertyEntry* Instance::GetPropertyEntry(PropertyId id) const
    {
        auto it = m_properties.find(id);
        if (it == m_properties.end()) return nullptr;
        return &it->second;
    }

    const PropertyValue* Instance::GetProperty(PropertyId id) const
    {
        auto* entry = GetPropertyEntry(id);
        return entry ? &entry->Value : nullptr;
    }

    bool Instance::HasProperty(PropertyId id) const { return m_properties.count(id) > 0; }
    bool Instance::IsReadOnly(PropertyId id)  const
    {
        auto* entry = GetPropertyEntry(id);
        return entry ? entry->ReadOnly : false;
    }

    const std::unordered_map<PropertyId, PropertyEntry>& Instance::GetProperties() const
    {
        return m_properties;
    }

} // namespace Sunvoltum
