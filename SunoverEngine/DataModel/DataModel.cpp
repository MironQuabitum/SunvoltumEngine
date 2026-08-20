#include "DataModel.h"

namespace Sunover {

    Instance& DataModel::AddInstance(const std::string& name, ClassId classId)
    {
        m_instances.push_back(std::make_unique<Instance>(name, classId, this));
        return *m_instances.back();
    }

    Instance& DataModel::AddInstance(const std::string& name, ClassId classId, InstanceParent& parent)
    {
        m_instances.push_back(std::make_unique<Instance>(name, classId, &parent));
        return *m_instances.back();
    }

    Instance* DataModel::FindByName(const std::string& name)
    {
        for (auto& inst : m_instances)
            if (inst->GetName() == name) return inst.get();
        return nullptr;
    }

    const std::vector<std::unique_ptr<Instance>>& DataModel::GetChildren() const
    {
        return m_instances;
    }

} // namespace Sunover
