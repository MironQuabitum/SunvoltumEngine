#pragma once

#include "SunvoltumPhysics/Collision/Shapes/Shape.h"
#include <vector>
#include <limits>
#include <algorithm>

namespace SunvoltumPhysics {

    class ConvexMeshShape : public Shape {
    public:
        ConvexMeshShape(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices = {})
            : Shape(ShapeType::ConvexMesh), m_vertices(vertices), m_indices(indices) {
            ComputeLocalBoundsAndCenter();
        }

        const std::vector<Vector3>& GetVertices() const { return m_vertices; }
        const std::vector<uint32_t>& GetIndices() const { return m_indices; }
        const Vector3& GetLocalCenter() const { return m_localCenter; }

        Vector3 GetSupportPoint(const Vector3& localDir) const {
            if (m_vertices.empty()) return Vector3::Zero;
            float maxDot = -std::numeric_limits<float>::infinity();
            Vector3 bestPt = m_vertices[0];
            for (const auto& v : m_vertices) {
                float d = v.Dot(localDir);
                if (d > maxDot) {
                    maxDot = d;
                    bestPt = v;
                }
            }
            return bestPt;
        }

        AABB ComputeAABB(const Transform& transform) const override {
            if (m_vertices.empty()) {
                return AABB(transform.position, transform.position);
            }
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

        Matrix3x3 ComputeInertia(float mass) const override {
            Vector3 size = m_localMax - m_localMin;
            float factor = (1.0f / 12.0f) * mass;
            float dx = (std::max)(size.x, 0.01f);
            float dy = (std::max)(size.y, 0.01f);
            float dz = (std::max)(size.z, 0.01f);

            Matrix3x3 I;
            I.m[0][0] = factor * (dy * dy + dz * dz);
            I.m[1][1] = factor * (dx * dx + dz * dz);
            I.m[2][2] = factor * (dx * dx + dy * dy);
            return I;
        }

    private:
        void ComputeLocalBoundsAndCenter() {
            if (m_vertices.empty()) return;
            m_localMin = Vector3(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
            m_localMax = Vector3(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
            Vector3 sum = Vector3::Zero;

            for (const auto& v : m_vertices) {
                m_localMin.x = (std::min)(m_localMin.x, v.x);
                m_localMin.y = (std::min)(m_localMin.y, v.y);
                m_localMin.z = (std::min)(m_localMin.z, v.z);
                m_localMax.x = (std::max)(m_localMax.x, v.x);
                m_localMax.y = (std::max)(m_localMax.y, v.y);
                m_localMax.z = (std::max)(m_localMax.z, v.z);
                sum += v;
            }
            m_localCenter = sum * (1.0f / static_cast<float>(m_vertices.size()));
        }

    private:
        std::vector<Vector3> m_vertices;
        std::vector<uint32_t> m_indices;
        Vector3 m_localMin{ 0.0f, 0.0f, 0.0f };
        Vector3 m_localMax{ 0.0f, 0.0f, 0.0f };
        Vector3 m_localCenter{ 0.0f, 0.0f, 0.0f };
    };

} // namespace SunvoltumPhysics
