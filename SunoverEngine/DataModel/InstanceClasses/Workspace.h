#pragma once

#include "../PropertyId.h"

namespace Sunover {
namespace Classes {

    constexpr int8_t CLASS_WORKSPACE = 1;

    struct Workspace
    {
        static constexpr int8_t ClassId = CLASS_WORKSPACE;

        // PropertyId параметров Workspace
        static constexpr PropertyId Gravity        = 0; // float,  м/с²
        static constexpr PropertyId PhysicsEnabled = 1; // bool,   ReadOnly
    };

} // namespace Classes
} // namespace Sunover
