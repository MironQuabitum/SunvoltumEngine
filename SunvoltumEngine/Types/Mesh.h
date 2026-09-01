#pragma once

#include <vector>
#include <cstdint>
#include "Vector3.h"
#include "Vector2.h"
#include "Color3.h"

namespace Sunvoltum {

    struct Vertex
    {
        Vector3 Position;
        Vector3 Normal;
        Vector2 UV;
        Color3  Color;

        Vertex() = default;
        Vertex(const Vector3& pos, const Vector3& normal,
               const Vector2& uv, const Color3& color = {1,1,1})
            : Position(pos), Normal(normal), UV(uv), Color(color) {}
    };

    struct Mesh
    {
        std::vector<Vertex>   Vertices;
        std::vector<uint32_t> Indices;

        Mesh() = default;
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
            : Vertices(vertices), Indices(indices) {}

        size_t GetVertexCount() const { return Vertices.size(); }
        size_t GetIndexCount()  const { return Indices.size();  }
        bool   IsEmpty()        const { return Vertices.empty(); }
    };

} // namespace Sunvoltum
