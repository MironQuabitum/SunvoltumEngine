#include "SunvoltumPhysics/Collision/BVH/TriMeshBVH.h"
#include "SunvoltumPhysics/Collision/Shapes/TriangleMeshShape.h"
#include "SunvoltumPhysics/Common/Math.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace SunvoltumPhysics {

    void TriMeshBVH::Build(const std::vector<Triangle>& triangles) {
        m_nodes.clear();
        if (triangles.empty()) return;

        int n = static_cast<int>(triangles.size());

        std::vector<AABB> triAABBs(n);
        std::vector<Vector3> triCentroids(n);
        for (int i = 0; i < n; ++i) {
            triAABBs[i] = triangles[i].GetAABB();
            triCentroids[i] = (triangles[i].a + triangles[i].b + triangles[i].c) * (1.0f / 3.0f);
        }

        std::vector<int> indices(n);
        for (int i = 0; i < n; ++i) indices[i] = i;

        BuildRecursive(indices, triAABBs, triCentroids, 0, n);
    }

    int TriMeshBVH::BuildRecursive(std::vector<int>& indices,
        const std::vector<AABB>& triAABBs,
        const std::vector<Vector3>& triCentroids,
        int begin, int end) {
        int nodeIdx = static_cast<int>(m_nodes.size());
        m_nodes.emplace_back();
        BVHNode& node = m_nodes[nodeIdx];

        // Вычислить общий AABB для диапазона
        AABB totalAABB = triAABBs[indices[begin]];
        for (int i = begin + 1; i < end; ++i) {
            totalAABB = totalAABB.Merged(triAABBs[indices[i]]);
        }
        node.aabb = totalAABB;

        if (end - begin == 1) {
            // Лист
            node.triangleIdx = indices[begin];
            return nodeIdx;
        }

        // Найти самую длинную ось
        Vector3 extent = totalAABB.max - totalAABB.min;
        int axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (extent.z > ((axis == 0) ? extent.x : extent.y)) axis = 2;

        // Сортировать по центроиду вдоль оси
        int mid = (begin + end) / 2;
        std::nth_element(indices.begin() + begin, indices.begin() + mid, indices.begin() + end,
            [&](int a, int b) {
                float ca = (axis == 0) ? triCentroids[a].x : ((axis == 1) ? triCentroids[a].y : triCentroids[a].z);
                float cb = (axis == 0) ? triCentroids[b].x : ((axis == 1) ? triCentroids[b].y : triCentroids[b].z);
                return ca < cb;
            });

        // Рекурсивно строить дочерние узлы
        // Важно: после рекурсии m_nodes может реаллоцироваться, поэтому обращаемся по индексу
        int leftChild = BuildRecursive(indices, triAABBs, triCentroids, begin, mid);
        int rightChild = BuildRecursive(indices, triAABBs, triCentroids, mid, end);

        m_nodes[nodeIdx].leftChild = leftChild;
        m_nodes[nodeIdx].rightChild = rightChild;

        return nodeIdx;
    }

    void TriMeshBVH::Query(const AABB& queryAABB,
        const std::function<void(int)>& callback) const {
        if (m_nodes.empty()) return;
        QueryRecursive(0, queryAABB, callback);
    }

    void TriMeshBVH::QueryRecursive(int nodeIdx, const AABB& queryAABB,
        const std::function<void(int)>& callback) const {
        const BVHNode& node = m_nodes[nodeIdx];
        if (!node.aabb.Overlaps(queryAABB)) return;

        if (node.triangleIdx >= 0) {
            callback(node.triangleIdx);
            return;
        }

        if (node.leftChild >= 0) QueryRecursive(node.leftChild, queryAABB, callback);
        if (node.rightChild >= 0) QueryRecursive(node.rightChild, queryAABB, callback);
    }

    // Möller–Trumbore ray-triangle intersection
    static bool RayTriangle(const Vector3& orig, const Vector3& dir,
        const Triangle& tri, float maxDist,
        float& outT, Vector3& outNormal) {
        Vector3 e1 = tri.b - tri.a;
        Vector3 e2 = tri.c - tri.a;
        Vector3 h = dir.Cross(e2);
        float   a = e1.Dot(h);

        if (std::abs(a) < EPSILON) return false;

        float   f = 1.0f / a;
        Vector3 s = orig - tri.a;
        float   u = f * s.Dot(h);
        if (u < 0.0f || u > 1.0f) return false;

        Vector3 q = s.Cross(e1);
        float   v = f * dir.Dot(q);
        if (v < 0.0f || u + v > 1.0f) return false;

        float t = f * e2.Dot(q);
        if (t < EPSILON || t > maxDist) return false;

        outT = t;
        outNormal = tri.normal;
        return true;
    }

    bool TriMeshBVH::Raycast(const Vector3& origin, const Vector3& dir, float maxDist,
        const std::vector<Triangle>& triangles,
        float& outT, Vector3& outNormal) const {
        if (m_nodes.empty()) return false;
        outT = maxDist;
        bool hit = RaycastRecursive(0, origin, dir, maxDist, triangles, outT, outNormal);
        return hit;
    }

    bool TriMeshBVH::RaycastRecursive(int nodeIdx,
        const Vector3& origin, const Vector3& dir,
        float maxDist,
        const std::vector<Triangle>& triangles,
        float& outT, Vector3& outNormal) const {
        const BVHNode& node = m_nodes[nodeIdx];

        // Быстрая проверка луча против AABB узла
        // slab method
        Vector3 invDir(
            std::abs(dir.x) > EPSILON ? 1.0f / dir.x : std::numeric_limits<float>::infinity(),
            std::abs(dir.y) > EPSILON ? 1.0f / dir.y : std::numeric_limits<float>::infinity(),
            std::abs(dir.z) > EPSILON ? 1.0f / dir.z : std::numeric_limits<float>::infinity()
        );

        float tx1 = (node.aabb.min.x - origin.x) * invDir.x;
        float tx2 = (node.aabb.max.x - origin.x) * invDir.x;
        float tMin = (std::min)(tx1, tx2);
        float tMax = (std::max)(tx1, tx2);

        float ty1 = (node.aabb.min.y - origin.y) * invDir.y;
        float ty2 = (node.aabb.max.y - origin.y) * invDir.y;
        tMin = (std::max)(tMin, (std::min)(ty1, ty2));
        tMax = (std::min)(tMax, (std::max)(ty1, ty2));

        float tz1 = (node.aabb.min.z - origin.z) * invDir.z;
        float tz2 = (node.aabb.max.z - origin.z) * invDir.z;
        tMin = (std::max)(tMin, (std::min)(tz1, tz2));
        tMax = (std::min)(tMax, (std::max)(tz1, tz2));

        if (tMax < 0.0f || tMin > tMax || tMin > outT) return false;

        if (node.triangleIdx >= 0) {
            float t;
            Vector3 n;
            if (RayTriangle(origin, dir, triangles[node.triangleIdx], outT, t, n)) {
                outT = t;
                outNormal = n;
                return true;
            }
            return false;
        }

        bool hitL = false, hitR = false;
        if (node.leftChild >= 0) hitL = RaycastRecursive(node.leftChild, origin, dir, maxDist, triangles, outT, outNormal);
        if (node.rightChild >= 0) hitR = RaycastRecursive(node.rightChild, origin, dir, maxDist, triangles, outT, outNormal);
        return hitL || hitR;
    }

} // namespace SunvoltumPhysics