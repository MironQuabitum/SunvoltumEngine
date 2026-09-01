#pragma once

#include "../PropertyId.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_DECAL = 6;

    struct Decal
    {
        static constexpr int8_t ClassId = CLASS_DECAL;

        // Грань Part, на которую наносится декаль
        // Значения: 0=Top, 1=Bottom, 2=Left, 3=Right, 4=Front, 5=Back
        // (соответствует порядку MeturmRender::Enum::DecalFace)
        static constexpr PropertyId Face         = 0; // Int

        // Путь к файлу текстуры
        static constexpr PropertyId Texture      = 1; // String

        // Прозрачность: 0.0 = непрозрачная, 1.0 = невидимая
        static constexpr PropertyId Transparency = 2; // Float
    };

} // namespace Classes
} // namespace Sunvoltum
