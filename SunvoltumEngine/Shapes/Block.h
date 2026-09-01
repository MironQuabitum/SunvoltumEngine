#pragma once

#include "../Types/Mesh.h"

namespace Sunvoltum {
namespace Shapes {

    // Генерирует меш параллелепипеда с центром в (0,0,0).
    // sizeX, sizeY, sizeZ — полный размер по каждой оси.
    inline Mesh MakeBlock(float sizeX = 1.0f, float sizeY = 1.0f, float sizeZ = 1.0f)
    {
        const float hx = sizeX * 0.5f;
        const float hy = sizeY * 0.5f;
        const float hz = sizeZ * 0.5f;

        // 6 граней, 4 вершины на грань, нормаль одинакова для всей грани
        std::vector<Vertex> verts;
        std::vector<uint32_t> idx;
        verts.reserve(24);
        idx.reserve(36);

        auto addFace = [&](Vector3 n,
                           Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3)
        {
            uint32_t base = static_cast<uint32_t>(verts.size());
            verts.push_back({ p0, n, {0,0} });
            verts.push_back({ p1, n, {1,0} });
            verts.push_back({ p2, n, {1,1} });
            verts.push_back({ p3, n, {0,1} });
            idx.insert(idx.end(), {
                base,base+1,base+2,
                base,base+2,base+3
            });
        };

        // +Z (front)
        addFace({0,0,1},  {-hx,-hy, hz},{hx,-hy, hz},{hx, hy, hz},{-hx, hy, hz});
        // -Z (back)
        addFace({0,0,-1}, { hx,-hy,-hz},{-hx,-hy,-hz},{-hx, hy,-hz},{ hx, hy,-hz});
        // +Y (top)
        addFace({0,1,0},  {-hx, hy, hz},{hx, hy, hz},{hx, hy,-hz},{-hx, hy,-hz});
        // -Y (bottom)
        addFace({0,-1,0}, {-hx,-hy,-hz},{hx,-hy,-hz},{hx,-hy, hz},{-hx,-hy, hz});
        // +X (right)
        addFace({1,0,0},  { hx,-hy, hz},{hx,-hy,-hz},{hx, hy,-hz},{ hx, hy, hz});
        // -X (left)
        addFace({-1,0,0}, {-hx,-hy,-hz},{-hx,-hy, hz},{-hx, hy, hz},{-hx, hy,-hz});

        return Mesh(verts, idx);
    }

} // namespace Shapes
} // namespace Sunvoltum
