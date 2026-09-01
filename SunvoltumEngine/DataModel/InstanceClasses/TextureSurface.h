#pragma once

#include "../PropertyId.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_TEXTURESURFACE = 7;

    struct TextureSurface
    {
        static constexpr int8_t ClassId = CLASS_TEXTURESURFACE;

        // Грань Part, на которую наносится текстура
        // Значения: 0=Top, 1=Bottom, 2=Left, 3=Right, 4=Front, 5=Back
        static constexpr PropertyId Face          = 0; // Int

        // Путь к файлу текстуры
        static constexpr PropertyId Texture       = 1; // String

        // Размер тайла по горизонтали в стадах (сколько стадов вмещает одна плитка текстуры)
        static constexpr PropertyId StudsPerTileU = 2; // Float

        // Размер тайла по вертикали в стадах
        static constexpr PropertyId StudsPerTileV = 3; // Float
    };

} // namespace Classes
} // namespace Sunvoltum
