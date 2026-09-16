#include "DataModel.h"
#include <functional>

namespace Sunvoltum {

    Instance& DataModel::AddInstance(const std::string& name, ClassId classId)
    {
        m_instances.push_back(std::make_unique<Instance>(name, classId, this));
        Instance& child = *m_instances.back();
        FireChildAdded(child);
        return child;
    }

    Instance& DataModel::AddInstance(const std::string& name, ClassId classId,
                                     std::function<void(Instance&)> initFn)
    {
        m_instances.push_back(std::make_unique<Instance>(name, classId, this));
        Instance& child = *m_instances.back();
        if (initFn) initFn(child);
        FireChildAdded(child);
        return child;
    }

    Instance& DataModel::AddInstance(const std::string& name, ClassId classId, InstanceParent& parent)
    {
        // Делегируем в parent — ownership и ChildAdded уведомление обрабатываются там.
        // Объект живёт в parent.m_children, DataModel не хранит дублирующий unique_ptr.
        return parent.AddInstance(name, classId);
    }

    Instance* DataModel::FindByName(const std::string& name)
    {
        for (auto& inst : m_instances)
            if (inst->GetName() == name) return inst.get();
        return nullptr;
    }

    bool DataModel::RemoveChild(Instance* child)
    {
        for (auto it = m_instances.begin(); it != m_instances.end(); ++it)
        {
            if (it->get() == child)
            {
                FireChildRemoved(*child);
                m_instances.erase(it);
                return true;
            }
        }
        return false;
    }

    const std::vector<std::unique_ptr<Instance>>& DataModel::GetChildren() const
    {
        return m_instances;
    }

} // namespace Sunvoltum
