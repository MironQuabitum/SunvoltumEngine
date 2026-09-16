#pragma once

#include <string>
#include <vector>
#include <memory>

#include "../LibSunvoltum.h"
#include "InstanceParent.h"
#include "Instance.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunvoltum {

    class LibSunvoltum DataModel : public InstanceParent
    {
    public:
        DataModel()  = default;
        ~DataModel() = default;

        DataModel(const DataModel&)            = delete;
        DataModel& operator=(const DataModel&) = delete;

        DataModel(DataModel&&)            = default;
        DataModel& operator=(DataModel&&) = default;

        Instance& AddInstance(const std::string& name, ClassId classId) override;
        Instance& AddInstance(const std::string& name, ClassId classId,
                              std::function<void(Instance&)> initFn) override;
        Instance& AddInstance(const std::string& name, ClassId classId, InstanceParent& parent);

        Instance* FindByName(const std::string& name) override;

        // Удалить прямого ребёнка по указателю.
        // Возвращает true если объект найден и удалён.
        bool RemoveChild(Instance* child);

        const std::vector<std::unique_ptr<Instance>>& GetChildren() const override;

    private:
        std::vector<std::unique_ptr<Instance>> m_instances;
    };

} // namespace Sunvoltum

#pragma warning(pop)
