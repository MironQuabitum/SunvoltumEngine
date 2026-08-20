#pragma once

#include <cstdint>

namespace Sunover {

    // PropertyId — числовой идентификатор параметра объекта.
    // Каждый InstanceClass объявляет свои PropertyId как constexpr uint8_t.
    // Используется вместо string чтобы избежать аллокаций в рантайме.
    using PropertyId = uint8_t;

} // namespace Sunover
