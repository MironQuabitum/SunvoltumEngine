#pragma once

#include "SunvoltumPhysics/Collision/AABB.h"
#include "SunvoltumPhysics/Common/Math.h"
#include <vector>
#include <functional>

namespace SunvoltumPhysics {
    struct Triangle;
    struct BVHNode {
        AABB aabb;
        int leftChild = -1;
        int rightChild = -1;
        int triangleIdx = -1; // только для листа
    };

    class TriMeshBVH {
    public:
        void Build(const std::vector<Triangle>& triangles);

        void Query(const AABB& queryAABB,
            const std::function<void(int)>& callback) const;

        bool Raycast(const Vector3& origin, const Vector3& dir, float maxDist,
            const std::vector<Triangle>& triangles,
            float& outT, Vector3& outNormal) const;

    private:
        int BuildRecursive(std::vector<int>& indices,
            const std::vector<AABB>& triAABBs,
            const std::vector<Vector3>& triCentroids,
            int begin, int end);

        void QueryRecursive(int nodeIdx, const AABB& queryAABB,
            const std::function<void(int)>& callback) const;

        bool RaycastRecursive(int nodeIdx,
            const Vector3& origin, const Vector3& dir,
            float maxDist,
            const std::vector<Triangle>& triangles,
            float& outT, Vector3& outNormal) const;

        std::vector<BVHNode> m_nodes;
    };

} // namespace SunvoltumPhysics