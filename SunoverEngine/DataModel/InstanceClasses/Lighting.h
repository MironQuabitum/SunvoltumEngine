#pragma once

#include "../PropertyId.h"

namespace Sunover {
namespace Classes {

    constexpr int8_t CLASS_LIGHTING = 4;

    struct Lighting
    {
        static constexpr int8_t ClassId = CLASS_LIGHTING;

        // Использовать стандартное небо (bool)
        static constexpr PropertyId UseDefaultSky         = 0;

        // Текущее время суток: 0.0 = полночь, 12.0 = полдень (Number)
        static constexpr PropertyId ClockTime             = 1;

        // Географическая широта для расчёта положения солнца (Number, градусы)
        static constexpr PropertyId GeographicLatitude    = 2;

        // Яркость освещения (Number)
        static constexpr PropertyId Brightness            = 3;
    };

} // namespace Classes
} // namespace Sunover
