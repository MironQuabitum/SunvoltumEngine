#pragma once

#include "Vector3.h"
#include "../LibSunvoltum.h"

namespace Sunvoltum {

    // RaycastResult -- result of a Physics.Raycast() call.
    // Hit=false means no geometry was found within maxDist.
    struct LibSunvoltum RaycastResult
    {
        bool    Hit      = false;   // true if something was hit
        float   Distance = 0.0f;   // distance from origin to hit point
        Vector3 Position = {};      // world-space hit position
        Vector3 Normal   = {};      // surface normal at hit point (unit vector)
    };

} // namespace Sunvoltum
