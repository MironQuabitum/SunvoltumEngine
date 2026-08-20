#pragma once

#include <string>
#include <vector>
#include <memory>

#include "../LibSunover.h"
#include "InstanceParent.h"
#include "Instance.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunover {

    class LibSunover DataModel : public InstanceParent
    {
    public:
        DataModel()  = default;
        ~DataModel() = default;

        DataModel(const DataModel&)            = delete;
        DataModel& operator=(const DataModel&) = delete;

        DataModel(DataModel&&)            = default;
        DataModel& operator=(DataModel&&) = default;

        Instance& AddInstance(const std::string& name, ClassId classId) override;
        Instance& AddInstance(const std::string& name, ClassId classId, InstanceParent& parent);

        Instance* FindByName(const std::string& name) override;

        const std::vector<std::unique_ptr<Instance>>& GetChildren() const override;

    private:
        std::vector<std::unique_ptr<Instance>> m_instances;
    };

} // namespace Sunover

#pragma warning(pop)
