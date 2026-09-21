#pragma once

#include "SunvoltumPhysics/Common/Math.h"
#include <functional>

namespace SunvoltumPhysics {
namespace GJK {

    using SupportFn = std::function<Vector3(const Vector3&)>;

    // True if the two convex supports overlap. Normal points from A toward B.
    bool TestAndGetContact(
        const SupportFn& supportA,
        const SupportFn& supportB,
        Vector3& outNormal,
        float& outPenetration,
        Vector3& outContactPoint);

} // namespace GJK
} // namespace SunvoltumPhysics
