#pragma once

#include <cmath>
#include "../Types/Mesh.h"

namespace Sunvoltum {
namespace Shapes {

    // Генерирует меш сферы с центром в (0,0,0).
    // radius    — радиус
    // stacks    — количество горизонтальных колец
    // slices    — количество вертикальных секций
    inline Mesh MakeSphere(float radius = 0.5f, int stacks = 16, int slices = 16)
    {
        constexpr float PI = 3.14159265358979323846f;

        std::vector<Vertex> verts;
        std::vector<uint32_t> idx;

        for (int i = 0; i <= stacks; i++)
        {
            float phi = PI * static_cast<float>(i) / static_cast<float>(stacks);
            float y   = radius * std::cos(phi);
            float r   = radius * std::sin(phi);

            for (int j = 0; j <= slices; j++)
            {
                float theta = 2.0f * PI * static_cast<float>(j) / static_cast<float>(slices);
                float x = r * std::cos(theta);
                float z = r * std::sin(theta);

                Vector3 pos    = { x, y, z };
                Vector3 normal = { x / radius, y / radius, z / radius };
                Vector2 uv     = {
                    static_cast<float>(j) / static_cast<float>(slices),
                    static_cast<float>(i) / static_cast<float>(stacks)
                };

                verts.push_back({ pos, normal, uv });
            }
        }

        for (int i = 0; i < stacks; i++)
        {
            for (int j = 0; j < slices; j++)
            {
                uint32_t row0 = static_cast<uint32_t>(i       * (slices + 1) + j);
                uint32_t row1 = static_cast<uint32_t>((i + 1) * (slices + 1) + j);

                idx.insert(idx.end(), {
                    row0,     row0 + 1, row1,
                    row0 + 1, row1 + 1, row1
                });
            }
        }

        return Mesh(verts, idx);
    }

} // namespace Shapes
} // namespace Sunvoltum
