#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <cmath>
#include <DirectXMath.h>

#include <SunvoltumRender/SunvoltumRender.h>

#include "SunvoltumPhysics/Dynamics/World.h"
#include "SunvoltumPhysics/Collision/Shapes/BoxShape.h"
#include "SunvoltumPhysics/Collision/Shapes/SphereShape.h"
#include "SunvoltumPhysics/Collision/Shapes/PlaneShape.h"
#include "SunvoltumPhysics/Collision/Shapes/TriangleMeshShape.h"
#include "SunvoltumPhysics/Collision/Shapes/ConvexMeshShape.h"

//                        SunvoltumRender
SunvoltumRender::Types::Mesh CreateCubeMesh(const SunvoltumRender::Types::Color& faceColor) {
    using namespace SunvoltumRender::Types;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    auto addFace = [&](Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, Vector3 normal) {
        uint32_t startIndex = static_cast<uint32_t>(vertices.size());
        vertices.emplace_back(p0, normal, Vector2(0.0f, 1.0f), faceColor);
        vertices.emplace_back(p1, normal, Vector2(1.0f, 1.0f), faceColor);
        vertices.emplace_back(p2, normal, Vector2(1.0f, 0.0f), faceColor);
        vertices.emplace_back(p3, normal, Vector2(0.0f, 0.0f), faceColor);

        indices.push_back(startIndex);
        indices.push_back(startIndex + 1);
        indices.push_back(startIndex + 2);
        indices.push_back(startIndex);
        indices.push_back(startIndex + 2);
        indices.push_back(startIndex + 3);
        };

    // Front (+Z)
    addFace({ -0.5f, -0.5f,  0.5f }, { 0.5f, -0.5f,  0.5f }, { 0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f,  0.5f },
        { 0.0f, 0.0f, 1.0f });
    // Back (-Z)
    addFace({ 0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f, -0.5f }, { -0.5f,  0.5f, -0.5f }, { 0.5f,  0.5f, -0.5f },
        { 0.0f, 0.0f, -1.0f });
    // Left (-X)
    addFace({ -0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f,  0.5f }, { -0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f, -0.5f },
        { -1.0f, 0.0f, 0.0f });
    // Right (+X)
    addFace({ 0.5f, -0.5f,  0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f,  0.5f, -0.5f }, { 0.5f,  0.5f,  0.5f },
        { 1.0f, 0.0f, 0.0f });
    // Top (+Y)
    addFace({ -0.5f,  0.5f,  0.5f }, { 0.5f,  0.5f,  0.5f }, { 0.5f,  0.5f, -0.5f }, { -0.5f,  0.5f, -0.5f },
        { 0.0f, 1.0f, 0.0f });
    // Bottom (-Y)
    addFace({ -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f,  0.5f }, { -0.5f, -0.5f,  0.5f },
        { 0.0f, -1.0f, 0.0f });

    return Mesh(vertices, indices);
}

//                             (UV Sphere)
SunvoltumRender::Types::Mesh CreateSphereMesh(const SunvoltumRender::Types::Color& color, float radius = 0.5f, int slices = 16, int stacks = 12) {
    using namespace SunvoltumRender::Types;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (int i = 0; i <= stacks; ++i) {
        float phi = SunvoltumPhysics::PI * static_cast<float>(i) / static_cast<float>(stacks);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * SunvoltumPhysics::PI * static_cast<float>(j) / static_cast<float>(slices);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vector3 normal(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            Vector3 pos(normal.x * radius, normal.y * radius, normal.z * radius);
            Vector2 uv(static_cast<float>(j) / slices, static_cast<float>(i) / stacks);

            vertices.emplace_back(pos, normal, uv, color);
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            uint32_t first = (i * (slices + 1)) + j;
            uint32_t second = first + slices + 1;

            //                        (winding order)            ,
            //                                                             (CCW / CW              D3D)
            indices.push_back(first);
            indices.push_back(first + 1);
            indices.push_back(second);

            indices.push_back(second);
            indices.push_back(first + 1);
            indices.push_back(second + 1);
        }
    }

    return Mesh(vertices, indices);
}
//                    (low-poly)   :          -           .
SunvoltumRender::Types::Mesh CreateFacetedSphereMesh(const SunvoltumRender::Types::Color& color,
    float radius = 0.5f, int slices = 10, int stacks = 8) {
    using namespace SunvoltumRender::Types;
    using SunvoltumPhysics::Vector3;

    //                (         UV-       ,       )
    std::vector<Vector3> raw;
    for (int i = 0; i <= stacks; ++i) {
        float phi = SunvoltumPhysics::PI * static_cast<float>(i) / static_cast<float>(stacks);
        float sinPhi = std::sin(phi), cosPhi = std::cos(phi);
        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * SunvoltumPhysics::PI * static_cast<float>(j) / static_cast<float>(slices);
            float sinTheta = std::sin(theta), cosTheta = std::cos(theta);
            raw.emplace_back(sinPhi * cosTheta * radius, cosPhi * radius, sinPhi * sinTheta * radius);
        }
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    //                     ,                       (flat shading)
    auto addFlatTri = [&](const Vector3& a, const Vector3& b, const Vector3& c) {
        Vector3 n = (b - a).Cross(c - a).Normalized();
        uint32_t start = static_cast<uint32_t>(vertices.size());
        vertices.emplace_back(SunvoltumRender::Types::Vector3(a.x, a.y, a.z),
            SunvoltumRender::Types::Vector3(n.x, n.y, n.z), Vector2(0.0f, 0.0f), color);
        vertices.emplace_back(SunvoltumRender::Types::Vector3(b.x, b.y, b.z),
            SunvoltumRender::Types::Vector3(n.x, n.y, n.z), Vector2(1.0f, 0.0f), color);
        vertices.emplace_back(SunvoltumRender::Types::Vector3(c.x, c.y, c.z),
            SunvoltumRender::Types::Vector3(n.x, n.y, n.z), Vector2(0.0f, 1.0f), color);
        indices.push_back(start);
        indices.push_back(start + 1);
        indices.push_back(start + 2);
        };

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            uint32_t first = i * (slices + 1) + j;
            uint32_t second = first + slices + 1;

            addFlatTri(raw[first], raw[first + 1], raw[second]);
            addFlatTri(raw[second], raw[first + 1], raw[second + 1]);
        }
    }

    return Mesh(vertices, indices);
}

static void BuildTorusGeometry(float majorR, float minorR, int radialSegs, int tubeSegs,
    std::vector<SunvoltumPhysics::Vector3>& verts, std::vector<uint32_t>& indices) {
    verts.clear();
    indices.clear();
    verts.reserve(static_cast<size_t>(radialSegs) * static_cast<size_t>(tubeSegs));
    for (int i = 0; i < radialSegs; ++i) {
        float theta = 2.0f * SunvoltumPhysics::PI * static_cast<float>(i) / static_cast<float>(radialSegs);
        float ct = std::cos(theta), st = std::sin(theta);
        for (int j = 0; j < tubeSegs; ++j) {
            float phi = 2.0f * SunvoltumPhysics::PI * static_cast<float>(j) / static_cast<float>(tubeSegs);
            float cp = std::cos(phi), sp = std::sin(phi);
            verts.emplace_back((majorR + minorR * cp) * ct, minorR * sp, (majorR + minorR * cp) * st);
        }
    }
    for (int i = 0; i < radialSegs; ++i) {
        int i1 = (i + 1) % radialSegs;
        for (int j = 0; j < tubeSegs; ++j) {
            int j1 = (j + 1) % tubeSegs;
            uint32_t a = static_cast<uint32_t>(i * tubeSegs + j);
            uint32_t b = static_cast<uint32_t>(i1 * tubeSegs + j);
            uint32_t c = static_cast<uint32_t>(i1 * tubeSegs + j1);
            uint32_t d = static_cast<uint32_t>(i * tubeSegs + j1);
            indices.push_back(a); indices.push_back(b); indices.push_back(c);
            indices.push_back(a); indices.push_back(c); indices.push_back(d);
        }
    }
}

static void BuildIcosahedronGeometry(float radius,
    std::vector<SunvoltumPhysics::Vector3>& verts, std::vector<uint32_t>& indices) {
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
    verts = {
        {-1.0f,  t, 0.0f}, { 1.0f,  t, 0.0f}, {-1.0f, -t, 0.0f}, { 1.0f, -t, 0.0f},
        { 0.0f, -1.0f,  t}, { 0.0f,  1.0f,  t}, { 0.0f, -1.0f, -t}, { 0.0f,  1.0f, -t},
        { t, 0.0f, -1.0f}, { t, 0.0f,  1.0f}, {-t, 0.0f, -1.0f}, {-t, 0.0f,  1.0f},
    };
    for (auto& v : verts) {
        v = v.Normalized() * radius;
    }
    indices = {
        0,11,5,  0,5,1,  0,1,7,  0,7,10,  0,10,11,
        1,5,9,  5,11,4,  11,10,2, 10,7,6,  7,1,8,
        3,9,4,  3,4,2,  3,2,6,  3,6,8,  3,8,9,
        4,9,5,  2,4,11, 6,2,10, 8,6,7,  9,8,1
    };
}

static SunvoltumRender::Types::Mesh CreateIndexedColorMesh(
    const std::vector<SunvoltumPhysics::Vector3>& verts,
    const std::vector<uint32_t>& indices,
    const SunvoltumRender::Types::Color& color) {
    using namespace SunvoltumRender::Types;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> outIdx;
    vertices.reserve(indices.size());
    outIdx.reserve(indices.size());
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const auto& a = verts[indices[i]];
        const auto& b = verts[indices[i + 1]];
        const auto& c = verts[indices[i + 2]];
        SunvoltumPhysics::Vector3 n = (b - a).Cross(c - a).Normalized();
        uint32_t start = static_cast<uint32_t>(vertices.size());
        vertices.emplace_back(Vector3(a.x, a.y, a.z), Vector3(n.x, n.y, n.z), Vector2(0.0f, 0.0f), color);
        vertices.emplace_back(Vector3(b.x, b.y, b.z), Vector3(n.x, n.y, n.z), Vector2(1.0f, 0.0f), color);
        vertices.emplace_back(Vector3(c.x, c.y, c.z), Vector3(n.x, n.y, n.z), Vector2(0.0f, 1.0f), color);
        outIdx.push_back(start);
        outIdx.push_back(start + 1);
        outIdx.push_back(start + 2);
    }
    return Mesh(vertices, outIdx);
}
// ============================================================
//                Debug-  (wireframe   )
// SunvoltumRender                     -          ,
//                     "  "        (                 )
//                                            ,
//                                   .
// ============================================================
namespace ColliderDebug {

    using namespace SunvoltumPhysics;
    using SunvoltumRender::Types::Vertex;
    using SunvoltumRender::Types::Vector2;

    // -                     wireframe-   ,               
    constexpr float kLineThickness = 0.02f;
    // -                 (RGBA),          -    
    inline SunvoltumRender::Types::Color WireColor() {
        return SunvoltumRender::Types::Color(0.15f, 0.65f, 1.0f, 1.0f);
    }

    //                          "     "   a   b   
    //                                                      .
    inline void AppendLineSegment(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
        const Vector3& a, const Vector3& b,
        const SunvoltumRender::Types::Color& color,
        float thickness = kLineThickness) {
        Vector3 dir = b - a;
        float len = dir.Length();
        if (len < 1e-6f) return;
        dir = dir * (1.0f / len);

        //                  ,       dir
        Vector3 up = (std::abs(dir.y) > 0.99f) ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 1.0f, 0.0f);
        Vector3 side = dir.Cross(up).Normalized();
        Vector3 ortho = dir.Cross(side).Normalized();

        float ht = thickness * 0.5f;
        Vector3 s = side * ht;
        Vector3 o = ortho * ht;

        //   8            "     " (          a b)
        Vector3 p[8] = {
            a - s - o, a + s - o, a + s + o, a - s + o, // near cap (around a)
            b - s - o, b + s - o, b + s + o, b - s + o  // far cap (around b)
        };

        uint32_t base = static_cast<uint32_t>(vertices.size());
        for (int i = 0; i < 8; ++i) {
            Vector3 n = (p[i] - (i < 4 ? a : b)).Normalized();
            vertices.emplace_back(
                SunvoltumRender::Types::Vector3(p[i].x, p[i].y, p[i].z),
                SunvoltumRender::Types::Vector3(n.x, n.y, n.z),
                Vector2(0.0f, 0.0f),
                color
            );
        }

        auto quad = [&](int i0, int i1, int i2, int i3) {
            indices.push_back(base + i0); indices.push_back(base + i1); indices.push_back(base + i2);
            indices.push_back(base + i0); indices.push_back(base + i2); indices.push_back(base + i3);
            };
        // 4         +      +      (          ,       "  "        )
        quad(0, 1, 5, 4);
        quad(1, 2, 6, 5);
        quad(2, 3, 7, 6);
        quad(3, 0, 4, 7);
        quad(3, 2, 1, 0); // near cap
        quad(4, 5, 6, 7); // far cap
    }

    //             (          Y     )
    inline void AppendCircle(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
        const Vector3& center, const Vector3& axisA, const Vector3& axisB,
        float radius, const SunvoltumRender::Types::Color& color, int segments = 24) {
        Vector3 prev = center + axisA * radius;
        for (int i = 1; i <= segments; ++i) {
            float t = (2.0f * PI * static_cast<float>(i)) / static_cast<float>(segments);
            Vector3 cur = center + (axisA * std::cos(t) + axisB * std::sin(t)) * radius;
            AppendLineSegment(vertices, indices, prev, cur, color);
            prev = cur;
        }
    }

    //    BoxShape:               (halfExtents      )
    inline SunvoltumRender::Types::Mesh BuildBoxWire(const Vector3& halfExtents) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        Vector3 h = halfExtents;
        Vector3 c[8] = {
            {-h.x,-h.y,-h.z}, { h.x,-h.y,-h.z}, { h.x, h.y,-h.z}, {-h.x, h.y,-h.z},
            {-h.x,-h.y, h.z}, { h.x,-h.y, h.z}, { h.x, h.y, h.z}, {-h.x, h.y, h.z},
        };
        static const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
        };
        auto col = WireColor();
        for (auto& e : edges) AppendLineSegment(vertices, indices, c[e[0]], c[e[1]], col);
        return SunvoltumRender::Types::Mesh(vertices, indices);
    }

    //    SphereShape:   3      (            )
    inline SunvoltumRender::Types::Mesh BuildSphereWire(float radius, int segments = 32) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        auto col = WireColor();
        AppendCircle(vertices, indices, Vector3::Zero, Vector3(1, 0, 0), Vector3(0, 1, 0), radius, col, segments);
        AppendCircle(vertices, indices, Vector3::Zero, Vector3(1, 0, 0), Vector3(0, 0, 1), radius, col, segments);
        AppendCircle(vertices, indices, Vector3::Zero, Vector3(0, 1, 0), Vector3(0, 0, 1), radius, col, segments);
        return SunvoltumRender::Types::Mesh(vertices, indices);
    }

    //    PlaneShape:             (         normal/distance     )
    inline SunvoltumRender::Types::Mesh BuildPlaneWire(const Vector3& normal, float distance,
        float halfSize = 25.0f, int divisions = 10) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        auto col = WireColor();

        Vector3 n = normal.Normalized();
        Vector3 up = (std::abs(n.y) > 0.99f) ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
        Vector3 tangentU = n.Cross(up).Normalized();
        Vector3 tangentV = n.Cross(tangentU).Normalized();
        Vector3 origin = n * distance;

        for (int i = 0; i <= divisions; ++i) {
            float t = -halfSize + (2.0f * halfSize) * (static_cast<float>(i) / divisions);
            Vector3 a = origin + tangentU * t + tangentV * (-halfSize);
            Vector3 b = origin + tangentU * t + tangentV * (halfSize);
            AppendLineSegment(vertices, indices, a, b, col);

            Vector3 c = origin + tangentV * t + tangentU * (-halfSize);
            Vector3 d = origin + tangentV * t + tangentU * (halfSize);
            AppendLineSegment(vertices, indices, c, d, col);
        }
        return SunvoltumRender::Types::Mesh(vertices, indices);
    }

    //    ConvexMeshShape / TriangleMeshShape:                 (indices)
    inline SunvoltumRender::Types::Mesh BuildTriListWire(const std::vector<Vector3>& verts,
        const std::vector<uint32_t>& indices) {
        std::vector<Vertex> outVerts;
        std::vector<uint32_t> outIndices;
        auto col = WireColor();
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const Vector3& a = verts[indices[i]];
            const Vector3& b = verts[indices[i + 1]];
            const Vector3& c = verts[indices[i + 2]];
            AppendLineSegment(outVerts, outIndices, a, b, col);
            AppendLineSegment(outVerts, outIndices, b, c, col);
            AppendLineSegment(outVerts, outIndices, c, a, col);
        }
        return SunvoltumRender::Types::Mesh(outVerts, outIndices);
    }

    // Fallback:                    AABB      (   ConvexMesh              /  )
    inline SunvoltumRender::Types::Mesh BuildAABBWire(const Vector3& halfExtents) {
        return BuildBoxWire(halfExtents);
    }

    //                    RigidBody           MeshObject.
    //                              (   scale/rotate     ),
    //                        SetPosition/SetRotation     .
    inline SunvoltumRender::Objects::MeshObject BuildWireMeshForBody(const std::shared_ptr<RigidBody>& body) {
        auto shape = body->GetShape();
        switch (shape->GetType()) {
        case ShapeType::Box: {
            auto box = std::static_pointer_cast<BoxShape>(shape);
            return SunvoltumRender::Objects::MeshObject(BuildBoxWire(box->GetHalfExtents()));
        }
        case ShapeType::Sphere: {
            auto sphere = std::static_pointer_cast<SphereShape>(shape);
            return SunvoltumRender::Objects::MeshObject(BuildSphereWire(sphere->GetRadius()));
        }
        case ShapeType::Plane: {
            auto plane = std::static_pointer_cast<PlaneShape>(shape);
            return SunvoltumRender::Objects::MeshObject(BuildPlaneWire(plane->GetNormal(), plane->GetDistance()));
        }
        case ShapeType::ConvexMesh: {
            auto convex = std::static_pointer_cast<ConvexMeshShape>(shape);
            if (!convex->GetIndices().empty()) {
                return SunvoltumRender::Objects::MeshObject(BuildTriListWire(convex->GetVertices(), convex->GetIndices()));
            }
            // -                 -      AABB
            return SunvoltumRender::Objects::MeshObject(BuildAABBWire(Vector3(0.5f, 0.5f, 0.5f)));
        }
        case ShapeType::TriangleMesh: {
            auto tri = std::static_pointer_cast<TriangleMeshShape>(shape);
            return SunvoltumRender::Objects::MeshObject(BuildTriListWire(tri->GetVertices(), tri->GetIndices()));
        }
        default: {
            //                                
            std::vector<Vertex> v; std::vector<uint32_t> i;
            return SunvoltumRender::Objects::MeshObject(SunvoltumRender::Types::Mesh(v, i));
        }
        }
    }

} // namespace ColliderDebug

// Convert physics orientation through the renderer's matrix→Euler path
// (matches XMMatrixRotationRollPitchYaw). Direct quaternion→Euler used the wrong roll term.
SunvoltumRender::Types::Matrix3x3 PhysicsRotationToRenderMatrix(const SunvoltumPhysics::Quaternion& q) {
    SunvoltumPhysics::Matrix3x3 m = SunvoltumPhysics::Matrix3x3::FromQuaternion(q.Normalized());
    return SunvoltumRender::Types::Matrix3x3(
        m.m[0][0], m.m[0][1], m.m[0][2],
        m.m[1][0], m.m[1][1], m.m[1][2],
        m.m[2][0], m.m[2][1], m.m[2][2]
    );
}

struct RenderablePhysicsBody {
    std::shared_ptr<SunvoltumPhysics::RigidBody> body;
    SunvoltumRender::Objects::MeshObject meshObject;
};

//                       :                        RigidBody.
struct ColliderDebugObject {
    std::shared_ptr<SunvoltumPhysics::RigidBody> body;
    SunvoltumRender::Objects::MeshObject meshObject;
};

int main() {
    std::cout << "=== Starting Sunvoltum Physics Interactive Real-time Demo ===" << std::endl;

    SunvoltumRender::RenderEngine engine;
    SunvoltumManager::WindowConfig config;
    config.title = "SunvoltumPDemo - 3D Real-Time Physics & Ball Ramp Test";
    config.width = 1280;
    config.height = 720;
    config.fullscreen = false;

    if (!engine.Init(SunvoltumRender::RenderType::Direct3D9, config)) {
        std::cerr << "[SunvoltumPDemo] Engine initialization failed!" << std::endl;
        return -1;
    }

    auto* window = engine.GetWindow();
    auto* renderer = engine.GetRenderer();

    //                    
    renderer->SetAmbientLight(SunvoltumRender::Types::Color(0.25f, 0.28f, 0.35f, 1.0f), 0.8f);
    SunvoltumRender::Objects::DirectionalLight sunLight(
        { -0.6f, -0.8f, 0.4f },
        { 1.0f, 0.98f, 0.92f, 1.0f },
        1.2f
    );
    renderer->SetDirectionalLight(sunLight);

    renderer->SetClearColor(0.12f, 0.15f, 0.22f, 1.0f);

    //     
    auto floorMesh = CreateCubeMesh({ 0.60f, 0.65f, 0.70f, 1.0f }); //              ServerScene.lua
    auto wallMesh = CreateCubeMesh({ 0.45f, 0.45f, 0.50f, 1.0f }); //              ServerScene.lua
    auto shotBallMesh = CreateSphereMesh({ 1.0f, 0.3f, 0.1f, 1.0f }, 0.75f); //             

    //               
    using namespace SunvoltumPhysics;
    //                        ws.Gravity = 196.2,                                   -25.0f..-35.0f
    World physicsWorld(Vector3(0.0f, -30.0f, 0.0f));
    // === Ramp (TriangleMeshShape) ===
// Наклонная плоскость 20x20, подъём 8 единиц
    auto CreateRampMesh = [](float w, float d, float h) {
        std::vector<Vector3> verts = {
            { -w * 0.5f, 0.0f, -d * 0.5f },   // 0: передний левый (низ)
            {  w * 0.5f, 0.0f, -d * 0.5f },   // 1: передний правый (низ)
            {  w * 0.5f, h,     d * 0.5f },   // 2: задний правый (верх)
            { -w * 0.5f, h,     d * 0.5f },   // 3: задний левый (верх)
        };
        std::vector<uint32_t> idxs = {
            0, 1, 2,   // треугольник 1
            0, 2, 3    // треугольник 2
        };
        return std::make_shared<TriangleMeshShape>(verts, idxs);
        };

    // Физическая рампа — вершины уже содержат наклон, rotation = identity
    auto rampShape = CreateRampMesh(20.0f, 20.0f, 8.0f);
    auto rampBody = std::make_shared<RigidBody>(
        BodyType::Static, rampShape,
        Transform(Vector3(0.0f, 0.75f, 15.0f), Quaternion(0, 0, 0, 1))
    );
    physicsWorld.AddBody(rampBody);

    // Визуал: куб наклоняем так же как наклонена поверхность рампы
    // Рампа: низ при Z=+10, верх при Z=-10, высота 8
    // Центр поверхности: X=0, Y=0.75+4=4.75, Z=15+0=15
    // Угол наклона вдоль Z: arctan(8/20) = 21.8 градуса
    auto rampVisMesh = CreateCubeMesh({ 0.55f, 0.75f, 0.40f, 1.0f });
    SunvoltumRender::Objects::MeshObject rampVisObj(rampVisMesh);
    rampVisObj.SetPosition({ 0.0f, 0.75f + 4.0f, 15.0f });
    rampVisObj.SetScale({ 20.0f, 0.5f, 21.5f });
    rampVisObj.SetRotation({ -21.8f, 0.0f, 0.0f });
    std::vector<RenderablePhysicsBody> renderBodies;

    //                              PhysicsWorld.
    //                                              Body*   ,
    //                                              GetBodies().
    std::vector<ColliderDebugObject> colliderDebugObjects;
    bool showColliders = true; // toggle-        C

    auto RebuildColliderDebugObjects = [&]() {
        colliderDebugObjects.clear();
        colliderDebugObjects.reserve(physicsWorld.GetBodies().size());
        for (const auto& b : physicsWorld.GetBodies()) {
            if (!b || !b->GetShape()) continue;
            ColliderDebugObject dbg;
            dbg.body = b;
            dbg.meshObject = ColliderDebug::BuildWireMeshForBody(b);
            colliderDebugObjects.push_back(std::move(dbg));
        }
        };
    auto AddColliderDebugForBody = [&](const std::shared_ptr<RigidBody>& b) {
        if (!b || !b->GetShape()) return;
        ColliderDebugObject dbg;
        dbg.body = b;
        dbg.meshObject = ColliderDebug::BuildWireMeshForBody(b);
        colliderDebugObjects.push_back(std::move(dbg));
        };
    auto SpawnBox = [&](const Vector3& pos, const Vector3& halfExtents, const Quaternion& rot, const SunvoltumRender::Types::Mesh& mesh, float mass, BodyType type = BodyType::Dynamic) {
        auto boxShape = std::make_shared<BoxShape>(halfExtents);
        auto body = std::make_shared<RigidBody>(type, boxShape, Transform(pos, rot));
        body->SetMass(mass);
        body->SetFriction(0.6f);
        body->SetRestitution(0.4f);
        physicsWorld.AddBody(body);

        SunvoltumRender::Objects::MeshObject obj(mesh);
        obj.SetScale({ halfExtents.x * 2.0f, halfExtents.y * 2.0f, halfExtents.z * 2.0f });
        renderBodies.push_back({ body, obj });
        return body;
        };
    auto SpawnCube = [&](const Vector3& pos, float halfSize,
        const SunvoltumRender::Types::Color& color,
        float mass, const Vector3& initVel = Vector3::Zero) {
            auto boxShape = std::make_shared<BoxShape>(Vector3(halfSize, halfSize, halfSize));
            auto body = std::make_shared<RigidBody>(
                BodyType::Dynamic, boxShape,
                Transform(pos, Quaternion(0, 0, 0, 1))
            );
            body->SetMass(mass);
            body->SetFriction(0.6f);
            body->SetRestitution(0.3f);
            body->SetLinearVelocity(initVel);
            physicsWorld.AddBody(body);

            auto cubeMesh = CreateCubeMesh(color);
            SunvoltumRender::Objects::MeshObject obj(cubeMesh);
            obj.SetScale({ halfSize * 2.0f, halfSize * 2.0f, halfSize * 2.0f });
            renderBodies.push_back({ body, obj });
            return body;
        };
    auto SpawnSphere = [&](const Vector3& pos, float radius, const SunvoltumRender::Types::Mesh& mesh,
        float mass, const Vector3& initVel = Vector3::Zero) {
            auto sphereShape = std::make_shared<SphereShape>(radius);
            auto body = std::make_shared<RigidBody>(BodyType::Dynamic, sphereShape, Transform(pos, Quaternion(0, 0, 0, 1)));
            body->SetMass(mass);
            body->SetFriction(0.5f);
            body->SetRestitution(0.75f);
            body->SetAngularDamping(0.10f);
            body->SetLinearVelocity(initVel);
            physicsWorld.AddBody(body);

            SunvoltumRender::Objects::MeshObject obj(mesh); // без пересоздания геометрии
            obj.SetScale({ radius * 2.0f, radius * 2.0f, radius * 2.0f });
            renderBodies.push_back({ body, obj });
            return body;
        };

    // 1.     95 x 1.5 x 95 (      ServerScene.lua)
    //        : (0, 0, 0),                       Y = 0.75
    SpawnBox(Vector3(0.0f, 0.0f, 0.0f), Vector3(47.5f, 0.75f, 47.5f), Quaternion(0, 0, 0, 1), floorMesh, 0.0f, BodyType::Static);

    // 2.                    (      ServerScene.lua)
    constexpr float fH = 47.5f;
    constexpr float wH = 12.0f;
    constexpr float wT = 2.0f;
    constexpr float wY = 0.75f + wH * 0.5f; // 6.75f

    // WallNorth: 95 x 12 x 2    (0, wY, fH + wT * 0.5)
    SpawnBox(Vector3(0.0f, wY, fH + wT * 0.5f), Vector3(47.5f, wH * 0.5f, wT * 0.5f), Quaternion(0, 0, 0, 1), wallMesh, 0.0f, BodyType::Static);
    // WallSouth: 95 x 12 x 2    (0, wY, -fH - wT * 0.5)
    SpawnBox(Vector3(0.0f, wY, -fH - wT * 0.5f), Vector3(47.5f, wH * 0.5f, wT * 0.5f), Quaternion(0, 0, 0, 1), wallMesh, 0.0f, BodyType::Static);
    // WallEast: 2 x 12 x (95 + wT * 2)    (fH + wT * 0.5, wY, 0)
    SpawnBox(Vector3(fH + wT * 0.5f, wY, 0.0f), Vector3(wT * 0.5f, wH * 0.5f, 47.5f + wT), Quaternion(0, 0, 0, 1), wallMesh, 0.0f, BodyType::Static);
    // WallWest: 2 x 12 x (95 + wT * 2)    (-fH - wT * 0.5, wY, 0)
    SpawnBox(Vector3(-fH - wT * 0.5f, wY, 0.0f), Vector3(wT * 0.5f, wH * 0.5f, 47.5f + wT), Quaternion(0, 0, 0, 1), wallMesh, 0.0f, BodyType::Static);


    const SunvoltumRender::Types::Color BALL_COLORS[20] = {
        {0.95f, 0.20f, 0.20f, 1.0f}, {0.95f, 0.55f, 0.10f, 1.0f}, {0.95f, 0.90f, 0.10f, 1.0f}, {0.40f, 0.85f, 0.20f, 1.0f},
        {0.10f, 0.75f, 0.75f, 1.0f}, {0.20f, 0.40f, 0.90f, 1.0f}, {0.65f, 0.20f, 0.90f, 1.0f}, {0.90f, 0.20f, 0.65f, 1.0f},
        {0.90f, 0.90f, 0.90f, 1.0f}, {0.30f, 0.30f, 0.30f, 1.0f}, {0.85f, 0.50f, 0.30f, 1.0f}, {0.20f, 0.85f, 0.50f, 1.0f},
        {0.50f, 0.20f, 0.10f, 1.0f}, {0.10f, 0.50f, 0.85f, 1.0f}, {0.80f, 0.80f, 0.10f, 1.0f}, {0.10f, 0.80f, 0.30f, 1.0f},
        {0.80f, 0.10f, 0.10f, 1.0f}, {0.60f, 0.60f, 0.95f, 1.0f}, {0.95f, 0.60f, 0.80f, 1.0f}, {0.30f, 0.80f, 0.80f, 1.0f},
    };

    constexpr int COLS = 5;
    constexpr int ROWS = 3;
    constexpr float SPACING = 9.0f;
    constexpr float BALL_RADIUS = 1.5f; // BALL_D = 3 -> Radius = 1.5
    constexpr float START_X = -(COLS - 1) * SPACING * 0.5f;
    constexpr float START_Z = -(ROWS - 1) * SPACING * 0.5f;


    std::vector<SunvoltumRender::Types::Mesh> sphereMeshCache;
    std::vector<SunvoltumRender::Types::Mesh> cubeMeshCache;
    sphereMeshCache.reserve(20);
    cubeMeshCache.reserve(20);
    for (int i = 0; i < 20; ++i) {
        sphereMeshCache.push_back(CreateFacetedSphereMesh(BALL_COLORS[i], 0.5f, 10, 8));
        cubeMeshCache.push_back(CreateCubeMesh(BALL_COLORS[i]));
    }

    static SunvoltumRender::Types::Mesh s_sphereWireMesh = ColliderDebug::BuildSphereWire(BALL_RADIUS);
    static SunvoltumRender::Types::Mesh s_boxWireMesh = ColliderDebug::BuildBoxWire(Vector3(1.0f, 1.0f, 1.0f));

    static SunvoltumRender::Types::Mesh s_blueBallMesh = CreateFacetedSphereMesh(
        SunvoltumRender::Types::Color(0.10f, 0.35f, 0.95f, 1.0f), 0.5f, 10, 8);
    static SunvoltumRender::Types::Mesh s_cannonBallMesh = CreateFacetedSphereMesh(
        SunvoltumRender::Types::Color(1.0f, 0.3f, 0.1f, 1.0f), 0.5f, 10, 8);


    int idx = 0;
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            float bx = START_X + static_cast<float>(col) * SPACING;
            float bz = START_Z + static_cast<float>(row) * SPACING;
            float by = 5.0f + static_cast<float>(idx) * 1.2f;
            float initVY = 15.0f + static_cast<float>((idx % 5)) * 4.0f;

            SpawnSphere(Vector3(bx, by, bz), BALL_RADIUS, sphereMeshCache[idx], 3.0f, Vector3(0.0f, initVY, 0.0f));
            SpawnCube(Vector3(bx + 28.0f, by + 2.0f, bz), 1.0f, BALL_COLORS[idx], 2.0f, Vector3(0.0f, initVY * 0.4f, 0.0f));
            idx++;
        }
    }

    // Сложный вогнутый TriangleMesh — тор (дыры нет у ConvexMesh)
    std::vector<Vector3> torusVerts;
    std::vector<uint32_t> torusIdx;
    BuildTorusGeometry(6.0f, 1.6f, 20, 12, torusVerts, torusIdx);
    auto torusShape = std::make_shared<TriangleMeshShape>(torusVerts, torusIdx);
    auto torusBody = std::make_shared<RigidBody>(
        BodyType::Static, torusShape,
        Transform(Vector3(-22.0f, 3.0f, 8.0f), Quaternion(0, 0, 0, 1))
    );
    physicsWorld.AddBody(torusBody);
    SunvoltumRender::Objects::MeshObject torusObj(
        CreateIndexedColorMesh(torusVerts, torusIdx, { 0.85f, 0.45f, 0.15f, 1.0f }));
    renderBodies.push_back({ torusBody, torusObj });

    // Динамический ConvexMesh — икосаэдр
    std::vector<Vector3> icoVerts;
    std::vector<uint32_t> icoIdx;
    BuildIcosahedronGeometry(2.4f, icoVerts, icoIdx);
    auto icoShape = std::make_shared<ConvexMeshShape>(icoVerts, icoIdx);
    auto icoBody = std::make_shared<RigidBody>(
        BodyType::Dynamic, icoShape,
        Transform(Vector3(6.0f, 18.0f, 12.0f), Quaternion(0, 0, 0, 1))
    );
    icoBody->SetMass(4.0f);
    icoBody->SetFriction(0.55f);
    icoBody->SetRestitution(0.25f);
    physicsWorld.AddBody(icoBody);
    SunvoltumRender::Objects::MeshObject icoObj(
        CreateIndexedColorMesh(icoVerts, icoIdx, { 0.20f, 0.85f, 0.95f, 1.0f }));
    renderBodies.push_back({ icoBody, icoObj });

    //                                                        PhysicsWorld
    //         rampBody,        SpawnBox/SpawnSphere.
    RebuildColliderDebugObjects();

    //                                 
    SunvoltumRender::Objects::Camera camera;
    camera.SetPerspective(60.0f, static_cast<float>(window->GetWidth()) / static_cast<float>(window->GetHeight()), 0.1f, 1000.0f);
    camera.SetPosition({ 0.0f, 32.0f, -55.0f });
    camera.SetRotation({ 26.0f, 0.0f, 0.0f });

    static SunvoltumRender::Renderer* s_renderer = renderer;
    static SunvoltumRender::Objects::Camera* s_camera = &camera;
    window->SetResizeCallback([](int newWidth, int newHeight) {
        if (newWidth > 0 && newHeight > 0) {
            if (s_renderer) s_renderer->Resize(newWidth, newHeight);
            if (s_camera) s_camera->SetPerspective(60.0f, static_cast<float>(newWidth) / static_cast<float>(newHeight), 0.1f, 1000.0f);
        }
        });

    auto& input = window->GetInput();
    input.SetSystemCursorVisible(true);

    std::cout << "[SunvoltumPDemo] Running! Controls:" << std::endl;
    std::cout << "  - W/A/S/D / Space / Ctrl: Fly Camera" << std::endl;
    std::cout << "  - Right Mouse Button (Hold): Look Around" << std::endl;
    std::cout << "  - F: Shoot Ball from Camera forward" << std::endl;
    std::cout << "  - R: Spawn Box above stack" << std::endl;
    std::cout << "  - C: Toggle collider debug draw" << std::endl;
    std::cout << "  - Escape: Exit" << std::endl;

    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    float accumulator = 0.0f;
    constexpr float fixedDt = 1.0f / 60.0f;
    constexpr int subSteps = 4;
    constexpr float subDt = fixedDt / static_cast<float>(subSteps);

    float statTimer = 0.0f;
    constexpr float STAT_INTERVAL = 1.0f;
    size_t sphereCount = 15, boxCount = 15;

    while (window && !window->ShouldClose()) {
        window->PollEvents();

        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        if (input.IsKeyPressed(SunvoltumManager::KeyCode::Escape)) { window->Close(); break; }

        if (input.IsKeyPressed(SunvoltumManager::KeyCode::C)) {
            showColliders = !showColliders;
            std::cout << "[SunvoltumPDemo] Collider debug draw: " << (showColliders ? "ON" : "OFF") << std::endl;
        }

        // --- камера ---
        auto camPos = camera.GetPosition();
        auto camRot = camera.GetRotation();

        static bool s_isRmbHeld = false;
        static int s_lockedMouseX = 0, s_lockedMouseY = 0;

        if (input.IsMousePressed(SunvoltumManager::MouseButton::Right)) {
            s_isRmbHeld = true;
            s_lockedMouseX = input.GetMouseX();
            s_lockedMouseY = input.GetMouseY();
            input.ResetMouseDelta();
        }
        if (input.IsMouseReleased(SunvoltumManager::MouseButton::Right)) s_isRmbHeld = false;

        if (s_isRmbHeld) {
            float sensitivity = 0.18f;
            int dx = input.GetRawMouseDeltaX();
            int dy = input.GetRawMouseDeltaY();
            if (dx == 0 && dy == 0) { dx = input.GetMouseDeltaX(); dy = input.GetMouseDeltaY(); }
            camRot.y += dx * sensitivity;
            camRot.x += dy * sensitivity;
            camRot.x = std::max(-89.0f, std::min(89.0f, camRot.x));
            input.SetMousePosition(s_lockedMouseX, s_lockedMouseY);
        }

        float yawRad = DirectX::XMConvertToRadians(camRot.y);
        float pitchRad = DirectX::XMConvertToRadians(camRot.x);
        float forwardX = std::sin(yawRad) * std::cos(pitchRad);
        float forwardY = -std::sin(pitchRad);
        float forwardZ = std::cos(yawRad) * std::cos(pitchRad);
        float rightX = std::cos(yawRad), rightY = 0.0f, rightZ = -std::sin(yawRad);

        float speed = 10.0f * deltaTime;
        if (input.IsKeyDown(SunvoltumManager::KeyCode::LeftShift)) speed *= 2.5f;

        if (input.IsKeyDown(SunvoltumManager::KeyCode::W)) { camPos.x += forwardX * speed; camPos.y += forwardY * speed; camPos.z += forwardZ * speed; }
        if (input.IsKeyDown(SunvoltumManager::KeyCode::S)) { camPos.x -= forwardX * speed; camPos.y -= forwardY * speed; camPos.z -= forwardZ * speed; }
        if (input.IsKeyDown(SunvoltumManager::KeyCode::D)) { camPos.x += rightX * speed;   camPos.y += rightY * speed;   camPos.z += rightZ * speed; }
        if (input.IsKeyDown(SunvoltumManager::KeyCode::A)) { camPos.x -= rightX * speed;   camPos.y -= rightY * speed;   camPos.z -= rightZ * speed; }
        if (input.IsKeyDown(SunvoltumManager::KeyCode::Space))       camPos.y += speed;
        if (input.IsKeyDown(SunvoltumManager::KeyCode::LeftControl)) camPos.y -= speed;

        camera.SetPosition(camPos);
        camera.SetRotation(camRot);

        // --- ручной спавн ---
        if (input.IsKeyPressed(SunvoltumManager::KeyCode::R)) {
            static int s_extraCount = 0;
            int meshIdx = s_extraCount++ % 20;
            auto b = SpawnSphere(Vector3(0.0f, 15.0f, 0.0f), BALL_RADIUS, sphereMeshCache[meshIdx], 3.0f, Vector3(0.0f, 5.0f, 0.0f));
            AddColliderDebugForBody(b);
            ++sphereCount;
        }
        if (input.IsKeyPressed(SunvoltumManager::KeyCode::T)) {
            auto b = SpawnCube(Vector3(0.0f, 16.0f, 18.0f), 1.5f,
                SunvoltumRender::Types::Color(0.2f, 0.9f, 0.3f, 1.0f), 2.0f);
            AddColliderDebugForBody(b);
            ++boxCount;
        }
        if (input.IsKeyPressed(SunvoltumManager::KeyCode::F)) {
            Vector3 camForward(forwardX, forwardY, forwardZ);
            Vector3 spawnPos(camPos.x + forwardX * 1.5f, camPos.y + forwardY * 1.5f, camPos.z + forwardZ * 1.5f);
            auto b = SpawnSphere(spawnPos, 0.75f, s_cannonBallMesh, 4.0f, camForward * 30.0f);
            AddColliderDebugForBody(b);
            ++sphereCount;
        }

        // --- статистика раз в секунду ---
        statTimer += deltaTime;
        if (statTimer >= STAT_INTERVAL) {
            statTimer -= STAT_INTERVAL;
            std::cout << "[SunvoltumPDemo] Spheres: " << sphereCount
                << " | Boxes: " << boxCount
                << " | Total bodies: " << physicsWorld.GetBodies().size()
                << " | FPS: " << (deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f)
                << std::endl;
        }

        // --- физика ---
        accumulator += deltaTime;
        while (accumulator >= fixedDt) {
            for (int s = 0; s < subSteps; ++s) physicsWorld.Step(subDt);
            accumulator -= fixedDt;
        }

        // --- синхронизация трансформов ---
        for (auto& rb : renderBodies) {
            const auto& pPos = rb.body->GetPosition();
            const auto& pRot = rb.body->GetRotation();
            rb.meshObject.SetPosition({ pPos.x, pPos.y, pPos.z });
            rb.meshObject.SetRotation(PhysicsRotationToRenderMatrix(pRot));
        }
        if (showColliders) {
            for (auto& dbg : colliderDebugObjects) {
                const auto& dPos = dbg.body->GetPosition();
                const auto& dRot = dbg.body->GetRotation();
                dbg.meshObject.SetPosition({ dPos.x, dPos.y, dPos.z });
                dbg.meshObject.SetRotation(PhysicsRotationToRenderMatrix(dRot));
            }
        }

        // --- рендер ---
        renderer->BeginFrame();
        renderer->RenderObject(rampVisObj, camera);
        for (const auto& rb : renderBodies) renderer->RenderObject(rb.meshObject, camera);
        if (showColliders) {
            for (const auto& dbg : colliderDebugObjects) renderer->RenderObject(dbg.meshObject, camera);
        }
        renderer->EndFrame();
    }

    std::cout << "[SunvoltumPDemo] Exiting demo..." << std::endl;
    engine.Shutdown();
    return 0;
}