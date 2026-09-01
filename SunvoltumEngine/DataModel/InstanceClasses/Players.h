#pragma once

#include "../PropertyId.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_PLAYERS = 5;

    struct Players
    {
        static constexpr int8_t ClassId = CLASS_PLAYERS;

        // Максимальное количество игроков на сервере (Int)
        static constexpr PropertyId MaxPlayers = 0;
    };

} // namespace Classes
} // namespace Sunvoltum
