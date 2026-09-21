#pragma once

#include "SunvoltumPhysics/Collision/Shapes/Shape.h"
#include "SunvoltumPhysics/Collision/BVH/TriMeshBVH.h"
#include <vector>
#include <limits>
#include <algorithm>

namespace SunvoltumPhysics {

    struct Triangle {
        Vector3 a, b, c;
        Vector3 normal;

        Triangle(const Vector3& p0, const Vector3& p1, const Vector3& p2)
            : a(p0), b(p1), c(p2) {
            Vector3 e1 = b - a;
            Vector3 e2 = c - a;
            normal = e1.Cross(e2).Normalized();
        }

        AABB GetAABB() const {
            Vector3 minPt(
                (std::min)({ a.x, b.x, c.x }),
                (std::min)({ a.y, b.y, c.y }),
                (std::min)({ a.z, b.z, c.z })
            );
            Vector3 maxPt(
                (std::max)({ a.x, b.x, c.x }),
                (std::max)({ a.y, b.y, c.y }),
                (std::max)({ a.z, b.z, c.z })
            );
            return AABB(minPt, maxPt);
        }
    };

    class TriangleMeshShape : public Shape {
    public:
        TriangleMeshShape(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices)
            : Shape(ShapeType::TriangleMesh), m_vertices(vertices), m_indices(indices) {
            BuildTriangles();
        }
        const std::vector<Triangle>& GetTriangles() const { return m_triangles; }
        const std::vector<Vector3>& GetVertices() const { return m_vertices; }
        const std::vector<uint32_t>& GetIndices() const { return m_indices; }
        const TriMeshBVH& GetBVH() const { return m_bvh; }
        AABB ComputeAABB(const Transform& transform) const override {
            if (m_vertices.empty()) return AABB(transform.position, transform.position);
            Vector3 minPt(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
            Vector3 maxPt(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());

            for (const auto& v : m_vertices) {
                Vector3 worldV = transform.TransformPoint(v);
                minPt.x = (std::min)(minPt.x, worldV.x);
                minPt.y = (std::min)(minPt.y, worldV.y);
                minPt.z = (std::min)(minPt.z, worldV.z);
                maxPt.x = (std::max)(maxPt.x, worldV.x);
                maxPt.y = (std::max)(maxPt.y, worldV.y);
                maxPt.z = (std::max)(maxPt.z, worldV.z);
            }
            return AABB(minPt, maxPt);
        }

        Matrix3x3 ComputeInertia(float /*mass*/) const override {
            // ����������� ������������� ���� ������ ������������ ��� Static (������� �������� �������)
            return Matrix3x3();
        }

    private:
        void BuildTriangles() {
            m_triangles.clear();
            if (m_indices.size() >= 3) {
                m_triangles.reserve(m_indices.size() / 3);
                for (size_t i = 0; i + 2 < m_indices.size(); i += 3) {
                    uint32_t i0 = m_indices[i];
                    uint32_t i1 = m_indices[i + 1];
                    uint32_t i2 = m_indices[i + 2];
                    if (i0 < m_vertices.size() && i1 < m_vertices.size() && i2 < m_vertices.size()) {
                        m_triangles.emplace_back(m_vertices[i0], m_vertices[i1], m_vertices[i2]);
                    }
                }
            }
            m_bvh.Build(m_triangles);
        }

    private:
        std::vector<Vector3> m_vertices;
        std::vector<uint32_t> m_indices;
        std::vector<Triangle> m_triangles;
        TriMeshBVH m_bvh;
    };

} // namespace SunvoltumPhysics
