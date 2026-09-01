#pragma once

#include "../PropertyId.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_FOLDER = 11;

    // Folder — контейнер для организации объектов в иерархии.
    // Не имеет собственных свойств, просто группирует дочерние Instance.
    struct Folder
    {
        static constexpr int8_t ClassId = CLASS_FOLDER;
    };

} // namespace Classes
} // namespace Sunvoltum
