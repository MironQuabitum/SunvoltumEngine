#pragma once

#include "SunvoltumPhysics/Common/Math.h"
#include <algorithm>

namespace SunvoltumPhysics {

    struct AABB {
        Vector3 min{ 0.0f, 0.0f, 0.0f };
        Vector3 max{ 0.0f, 0.0f, 0.0f };

        constexpr AABB() = default;
        constexpr AABB(const Vector3& min_, const Vector3& max_) : min(min_), max(max_) {}

        bool Overlaps(const AABB& other) const {
            if (max.x < other.min.x || min.x > other.max.x) return false;
            if (max.y < other.min.y || min.y > other.max.y) return false;
            if (max.z < other.min.z || min.z > other.max.z) return false;
            return true;
        }

        bool Contains(const AABB& other) const {
            return min.x <= other.min.x && max.x >= other.max.x &&
                   min.y <= other.min.y && max.y >= other.max.y &&
                   min.z <= other.min.z && max.z >= other.max.z;
        }

        AABB Merged(const AABB& other) const {
            return {
                { (std::min)(min.x, other.min.x), (std::min)(min.y, other.min.y), (std::min)(min.z, other.min.z) },
                { (std::max)(max.x, other.max.x), (std::max)(max.y, other.max.y), (std::max)(max.z, other.max.z) }
            };
        }

        Vector3 GetCenter() const {
            return (min + max) * 0.5f;
        }

        Vector3 GetExtents() const {
            return (max - min) * 0.5f;
        }
    };

} // namespace SunvoltumPhysics
