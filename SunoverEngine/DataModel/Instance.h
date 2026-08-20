#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "../LibSunover.h"
#include "InstanceParent.h"
#include "PropertyId.h"
#include "PropertyValue.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunover {

    using ClassId = int8_t;

    struct PropertyEntry
    {
        PropertyValue Value;
        bool          ReadOnly = false;
    };

    class LibSunover Instance : public InstanceParent
    {
    public:
        Instance(const std::string& name, ClassId classId, InstanceParent* parent = nullptr);
        ~Instance() = default;

        Instance(const Instance&)            = delete;
        Instance& operator=(const Instance&) = delete;

        Instance(Instance&&)            = default;
        Instance& operator=(Instance&&) = default;

        const std::string& GetName()    const;
        ClassId            GetClassId() const;

        InstanceParent* GetParent() const;
        void            SetParent(InstanceParent* parent);

        Instance& AddInstance(const std::string& name, ClassId classId) override;
        Instance* FindByName(const std::string& name) override;
        const std::vector<std::unique_ptr<Instance>>& GetChildren() const override;

        void SetProperty(PropertyId id, const PropertyValue& value, bool readOnly = false);
        const PropertyEntry*  GetPropertyEntry(PropertyId id) const;
        const PropertyValue*  GetProperty(PropertyId id) const;
        bool HasProperty(PropertyId id) const;
        bool IsReadOnly(PropertyId id)  const;

    private:
        std::string                                   m_name;
        ClassId                                       m_classId = 0;
        InstanceParent*                               m_parent  = nullptr;
        std::vector<std::unique_ptr<Instance>>        m_children;
        std::unordered_map<PropertyId, PropertyEntry> m_properties;
    };

} // namespace Sunover

#pragma warning(pop)
