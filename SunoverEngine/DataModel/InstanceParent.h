#pragma once

#include <string>
#include <vector>
#include <memory>

#include "../LibSunover.h"

namespace Sunover {

    class Instance;

    // InstanceParent — интерфейс узла иерархии.
    // Реализуется как DataModel (корень) так и Instance (любой объект).
    class LibSunover InstanceParent
    {
    public:
        virtual ~InstanceParent() = default;

        // Добавить дочерний Instance. Владение передаётся родителю.
        virtual Instance& AddInstance(const std::string& name, int8_t classId) = 0;

        // Найти первого прямого потомка по имени
        virtual Instance* FindByName(const std::string& name) = 0;

        // Все прямые потомки
        virtual const std::vector<std::unique_ptr<Instance>>& GetChildren() const = 0;
    };

} // namespace Sunover
