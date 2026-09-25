#include "SunvoltumPhysics/Dynamics/World.h"
#include "SunvoltumPhysics/Collision/NarrowPhase/CollisionDispatch.h"
#include "SunvoltumPhysics/Collision/Shapes/BoxShape.h"
#include "SunvoltumPhysics/Collision/Shapes/SphereShape.h"
#include "SunvoltumPhysics/Collision/Shapes/PlaneShape.h"
#include "SunvoltumPhysics/Collision/Shapes/TriangleMeshShape.h"
#include "SunvoltumPhysics/Collision/Shapes/ConvexMeshShape.h"
#include <unordered_map>
#include <cmath>

namespace SunvoltumPhysics {

    void World::Step(float dt) {
        if (dt <= 0.0f) return;

        for (auto& body : m_bodies) {
            body->IntegrateVelocities(dt, m_gravity);
        }

        BroadPhase();
        NarrowPhase();

        m_solver.Solve(m_manifolds, m_joints, dt);

        for (auto& body : m_bodies) {
            body->IntegratePositions(dt);
            body->ClearAccumulators();
        }
    }

    void World::BroadPhase() {
        m_broadPhasePairs.clear();
        size_t n = m_bodies.size();

        for (size_t i = 0; i < n; ++i) {
            RigidBody* bA = m_bodies[i].get();
            for (size_t j = i + 1; j < n; ++j) {
                RigidBody* bB = m_bodies[j].get();

                if (bA->GetType() == BodyType::Static && bB->GetType() == BodyType::Static) {
                    continue;
                }

                // Do not collide bodies connected by an active Joint (matches PhysX joint constraint flag)
                bool connectedByJoint = false;
                for (const auto& joint : m_joints) {
                    if (!joint || !joint->IsEnabled()) continue;
                    if ((joint->GetBodyA() == bA && joint->GetBodyB() == bB) ||
                        (joint->GetBodyA() == bB && joint->GetBodyB() == bA)) {
                        connectedByJoint = true;
                        break;
                    }
                }
                if (connectedByJoint) continue;

                if (bA->GetAABB().Overlaps(bB->GetAABB())) {
                    m_broadPhasePairs.emplace_back(bA, bB);
                }
            }
        }
    }

    void World::NarrowPhase() {
        struct Impulses {
            float normal{ 0.0f };
            float t1{ 0.0f };
            float t2{ 0.0f };
        };
        std::unordered_map<uint64_t, Impulses> oldImpulses;

        for (const auto& oldM : m_manifolds) {
            for (int i = 0; i < oldM.pointCount; ++i) {
                uint64_t key = (reinterpret_cast<uint64_t>(oldM.bodyA) ^ (reinterpret_cast<uint64_t>(oldM.bodyB) << 1)) ^ oldM.points[i].featureId;
                oldImpulses[key] = { oldM.points[i].normalImpulse, oldM.points[i].tangentImpulse1, oldM.points[i].tangentImpulse2 };
            }
        }

        m_manifolds.clear();

        for (const auto& pair : m_broadPhasePairs) {
            ContactManifold manifold;
            if (CollisionDispatch::TestCollision(pair.first, pair.second, manifold)) {
                for (int i = 0; i < manifold.pointCount; ++i) {
                    uint64_t key = (reinterpret_cast<uint64_t>(manifold.bodyA) ^ (reinterpret_cast<uint64_t>(manifold.bodyB) << 1)) ^ manifold.points[i].featureId;
                    auto it = oldImpulses.find(key);
                    if (it != oldImpulses.end()) {
                        manifold.points[i].normalImpulse = it->second.normal;
                        manifold.points[i].tangentImpulse1 = it->second.t1;
                        manifold.points[i].tangentImpulse2 = it->second.t2;
                    }
                }
                m_manifolds.push_back(manifold);
            }
        }
    }

    bool World::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance,
                        RaycastHit& outHit, const RigidBody* ignoreBody) const {
        float closestDist = maxDistance;
        bool found = false;
        Vector3 dirNorm = direction.Normalized();

        for (const auto& bodyPtr : m_bodies) {
            RigidBody* body = bodyPtr.get();
            if (!body || body == ignoreBody) continue;

            auto shape = body->GetShape();
            if (!shape) continue;

            ShapeType sType = shape->GetType();

            if (sType == ShapeType::Plane) {
                auto plane = std::static_pointer_cast<PlaneShape>(shape);
                Vector3 n = plane->GetNormal();
                float denom = n.Dot(dirNorm);
                if (std::abs(denom) > 1e-6f) {
                    float t = (plane->GetDistance() - origin.Dot(n)) / denom;
                    if (t >= 0.0f && t < closestDist) {
                        closestDist = t;
                        found = true;
                        outHit.hit = true;
                        outHit.distance = t;
                        outHit.point = origin + dirNorm * t;
                        outHit.normal = denom < 0.0f ? n : -n;
                        outHit.body = body;
                    }
                }
            }
            else if (sType == ShapeType::Sphere) {
                auto sphere = std::static_pointer_cast<SphereShape>(shape);
                Vector3 center = body->GetPosition();
                float radius = sphere->GetRadius();

                Vector3 m = origin - center;
                float b = m.Dot(dirNorm);
                float c = m.Dot(m) - radius * radius;

                if (c > 0.0f && b > 0.0f) continue;

                float discr = b * b - c;
                if (discr < 0.0f) continue;

                float t = -b - std::sqrt(discr);
                if (t < 0.0f) t = 0.0f;

                if (t < closestDist) {
                    closestDist = t;
                    found = true;
                    outHit.hit = true;
                    outHit.distance = t;
                    outHit.point = origin + dirNorm * t;
                    outHit.normal = (outHit.point - center).Normalized();
                    outHit.body = body;
                }
            }
            else if (sType == ShapeType::Box) {
                auto box = std::static_pointer_cast<BoxShape>(shape);
                Vector3 h = box->GetHalfExtents();
                Transform tr = body->GetTransform();

                Matrix3x3 rot = Matrix3x3::FromQuaternion(tr.rotation);
                Matrix3x3 invRot = rot.Transposed();

                Vector3 localOrig = invRot * (origin - tr.position);
                Vector3 localDir  = invRot * dirNorm;

                float tMin = 0.0f;
                float tMax = closestDist;
                Vector3 hitNorm(0.0f, 1.0f, 0.0f);
                bool hitBox = true;

                for (int i = 0; i < 3; ++i) {
                    float o = (i == 0) ? localOrig.x : ((i == 1) ? localOrig.y : localOrig.z);
                    float d = (i == 0) ? localDir.x  : ((i == 1) ? localDir.y  : localDir.z);
                    float extent = (i == 0) ? h.x : ((i == 1) ? h.y : h.z);

                    if (std::abs(d) < 1e-6f) {
                        if (o < -extent || o > extent) {
                            hitBox = false;
                            break;
                        }
                    } else {
                        float ood = 1.0f / d;
                        float t1 = (-extent - o) * ood;
                        float t2 = (extent - o) * ood;
                        Vector3 faceNorm = (i == 0) ? Vector3(-1, 0, 0) : ((i == 1) ? Vector3(0, -1, 0) : Vector3(0, 0, -1));
                        if (t1 > t2) {
                            std::swap(t1, t2);
                            faceNorm = -faceNorm;
                        }

                        if (t1 > tMin) {
                            tMin = t1;
                            hitNorm = faceNorm;
                        }
                        if (t2 < tMax) tMax = t2;

                        if (tMin > tMax) {
                            hitBox = false;
                            break;
                        }
                    }
                }

                if (hitBox && tMin < closestDist && tMin >= 0.0f) {
                    closestDist = tMin;
                    found = true;
                    outHit.hit = true;
                    outHit.distance = tMin;
                    outHit.point = origin + dirNorm * tMin;
                    outHit.normal = rot * hitNorm;
                    outHit.body = body;
                }
            }
            else if (sType == ShapeType::TriangleMesh) {
                auto mesh = std::static_pointer_cast<TriangleMeshShape>(shape);
                Transform tr = body->GetTransform();
                Matrix3x3 rot = Matrix3x3::FromQuaternion(tr.rotation);
                Matrix3x3 invRot = rot.Transposed();
                Vector3 localOrig = invRot * (origin - tr.position);
                Vector3 localDir = invRot * dirNorm;

                float t = 0.0f;
                Vector3 n(0.0f, 1.0f, 0.0f);
                if (mesh->GetBVH().Raycast(localOrig, localDir, closestDist, mesh->GetTriangles(), t, n)) {
                    if (t >= 0.0f && t < closestDist) {
                        closestDist = t;
                        found = true;
                        outHit.hit = true;
                        outHit.distance = t;
                        outHit.point = origin + dirNorm * t;
                        outHit.normal = (rot * n).Normalized();
                        outHit.body = body;
                    }
                }
            }
            else if (sType == ShapeType::ConvexMesh) {
                auto convex = std::static_pointer_cast<ConvexMeshShape>(shape);
                const auto& verts = convex->GetVertices();
                const auto& indices = convex->GetIndices();
                if (indices.size() < 3 || verts.empty()) continue;

                Transform tr = body->GetTransform();
                Matrix3x3 rot = Matrix3x3::FromQuaternion(tr.rotation);
                Matrix3x3 invRot = rot.Transposed();
                Vector3 localOrig = invRot * (origin - tr.position);
                Vector3 localDir = invRot * dirNorm;

                for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                    uint32_t i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
                    if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) continue;

                    Vector3 v0 = verts[i0];
                    Vector3 v1 = verts[i1];
                    Vector3 v2 = verts[i2];
                    Vector3 e1 = v1 - v0;
                    Vector3 e2 = v2 - v0;
                    Vector3 h = localDir.Cross(e2);
                    float a = e1.Dot(h);
                    if (std::abs(a) < EPSILON) continue;
                    float f = 1.0f / a;
                    Vector3 s = localOrig - v0;
                    float u = f * s.Dot(h);
                    if (u < 0.0f || u > 1.0f) continue;
                    Vector3 q = s.Cross(e1);
                    float v = f * localDir.Dot(q);
                    if (v < 0.0f || u + v > 1.0f) continue;
                    float t = f * e2.Dot(q);
                    if (t < EPSILON || t >= closestDist) continue;

                    Vector3 n = e1.Cross(e2).Normalized();
                    if (n.Dot(localDir) > 0.0f) n = -n;
                    closestDist = t;
                    found = true;
                    outHit.hit = true;
                    outHit.distance = t;
                    outHit.point = origin + dirNorm * t;
                    outHit.normal = (rot * n).Normalized();
                    outHit.body = body;
                }
            }
        }

        return found;
    }

} // namespace SunvoltumPhysics