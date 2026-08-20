#pragma once

#include <cmath>
#include "../Types/Mesh.h"

namespace Sunover {
namespace Shapes {

    // Генерирует меш цилиндра с центром в (0,0,0), ось Y.
    // radius  — радиус основания
    // height  — полная высота
    // slices  — количество секций по окружности
    inline Mesh MakeCylinder(float radius = 0.5f, float height = 1.0f, int slices = 16)
    {
        constexpr float PI = 3.14159265358979323846f;

        const float hy = height * 0.5f;

        std::vector<Vertex> verts;
        std::vector<uint32_t> idx;

        // --- Боковая поверхность ---
        for (int i = 0; i <= slices; i++)
        {
            float theta = 2.0f * PI * static_cast<float>(i) / static_cast<float>(slices);
            float x = radius * std::cos(theta);
            float z = radius * std::sin(theta);

            Vector3 normal = { x / radius, 0.0f, z / radius };
            float   u      = static_cast<float>(i) / static_cast<float>(slices);

            verts.push_back({ {x, -hy, z}, normal, {u, 1.0f} }); // нижнее кольцо
            verts.push_back({ {x,  hy, z}, normal, {u, 0.0f} }); // верхнее кольцо
        }

        uint32_t sideVerts = static_cast<uint32_t>(verts.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(slices); i++)
        {
            uint32_t b = i * 2;
            idx.insert(idx.end(), {
                b,     b + 2, b + 1,
                b + 1, b + 2, b + 3
            });
        }

        // --- Нижний диск ---
        uint32_t bottomCenter = static_cast<uint32_t>(verts.size());
        verts.push_back({ {0, -hy, 0}, {0,-1,0}, {0.5f, 0.5f} });

        for (int i = 0; i <= slices; i++)
        {
            float theta = 2.0f * PI * static_cast<float>(i) / static_cast<float>(slices);
            float x = radius * std::cos(theta);
            float z = radius * std::sin(theta);
            verts.push_back({ {x, -hy, z}, {0,-1,0},
                { std::cos(theta) * 0.5f + 0.5f, std::sin(theta) * 0.5f + 0.5f } });
        }

        for (uint32_t i = 0; i < static_cast<uint32_t>(slices); i++)
        {
            idx.insert(idx.end(), {
                bottomCenter,
                bottomCenter + 1 + i + 1,
                bottomCenter + 1 + i
            });
        }

        // --- Верхний диск ---
        uint32_t topCenter = static_cast<uint32_t>(verts.size());
        verts.push_back({ {0, hy, 0}, {0,1,0}, {0.5f, 0.5f} });

        for (int i = 0; i <= slices; i++)
        {
            float theta = 2.0f * PI * static_cast<float>(i) / static_cast<float>(slices);
            float x = radius * std::cos(theta);
            float z = radius * std::sin(theta);
            verts.push_back({ {x, hy, z}, {0,1,0},
                { std::cos(theta) * 0.5f + 0.5f, std::sin(theta) * 0.5f + 0.5f } });
        }

        for (uint32_t i = 0; i < static_cast<uint32_t>(slices); i++)
        {
            idx.insert(idx.end(), {
                topCenter,
                topCenter + 1 + i,
                topCenter + 1 + i + 1
            });
        }

        return Mesh(verts, idx);
    }

} // namespace Shapes
} // namespace Sunover
