#include "SunvoltumPhysics/Collision/NarrowPhase/CollisionDispatch.h"
#include "SunvoltumPhysics/Collision/NarrowPhase/GJK_EPA.h"
#include "SunvoltumPhysics/Collision/Shapes/SphereShape.h"
#include "SunvoltumPhysics/Collision/Shapes/BoxShape.h"
#include "SunvoltumPhysics/Collision/Shapes/PlaneShape.h"
#include "SunvoltumPhysics/Collision/Shapes/TriangleMeshShape.h"
#include "SunvoltumPhysics/Collision/Shapes/ConvexMeshShape.h"
#include "SunvoltumPhysics/Collision/OBB.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace SunvoltumPhysics {

    bool CollisionDispatch::TestCollision(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold) {
        if (!bodyA || !bodyB) return false;
        if (bodyA->GetType() == BodyType::Static && bodyB->GetType() == BodyType::Static) return false;

        ShapeType typeA = bodyA->GetShape()->GetType();
        ShapeType typeB = bodyB->GetShape()->GetType();

        outManifold.bodyA = bodyA;
        outManifold.bodyB = bodyB;
        auto matA = bodyA->GetMaterial();
        auto matB = bodyB->GetMaterial();

        if (matA && matB) {
            outManifold.friction = CombineParameter(
                matA->GetDynamicFriction(), matB->GetDynamicFriction(),
                matA->GetFrictionCombineMode(), matB->GetFrictionCombineMode()
            );
            outManifold.restitution = CombineParameter(
                matA->GetRestitution(), matB->GetRestitution(),
                matA->GetRestitutionCombineMode(), matB->GetRestitutionCombineMode()
            );
        } else {
            outManifold.friction = std::sqrt(bodyA->GetFriction() * bodyB->GetFriction());
            outManifold.restitution = (std::min)(bodyA->GetRestitution(), bodyB->GetRestitution());
        }

        if (typeA == ShapeType::Sphere && typeB == ShapeType::Sphere) {
            return SphereVsSphere(bodyA, bodyB, outManifold);
        }
        if (typeA == ShapeType::Sphere && typeB == ShapeType::Plane) {
            bool hit = SphereVsPlane(bodyA, bodyB, outManifold);
            return hit;
        }
        if (typeA == ShapeType::Plane && typeB == ShapeType::Sphere) {
            bool hit = SphereVsPlane(bodyB, bodyA, outManifold);
            if (hit) {
                outManifold.bodyA = bodyA;
                outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal;
            }
            return hit;
        }
        if (typeA == ShapeType::Sphere && typeB == ShapeType::Box) {
            return SphereVsBox(bodyA, bodyB, outManifold);
        }
        if (typeA == ShapeType::Box && typeB == ShapeType::Sphere) {
            bool hit = SphereVsBox(bodyB, bodyA, outManifold);
            if (hit) {
                outManifold.bodyA = bodyA;
                outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal;
            }
            return hit;
        }
        if (typeA == ShapeType::Box && typeB == ShapeType::Plane) {
            return BoxVsPlane(bodyA, bodyB, outManifold);
        }
        if (typeA == ShapeType::Plane && typeB == ShapeType::Box) {
            bool hit = BoxVsPlane(bodyB, bodyA, outManifold);
            if (hit) {
                outManifold.bodyA = bodyA;
                outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal;
            }
            return hit;
        }
        if (typeA == ShapeType::Box && typeB == ShapeType::Box) {
            return BoxVsBox(bodyA, bodyB, outManifold);
        }

        if (typeA == ShapeType::Sphere && typeB == ShapeType::TriangleMesh) {
            return SphereVsTriangleMesh(bodyA, bodyB, outManifold);
        }
        if (typeA == ShapeType::TriangleMesh && typeB == ShapeType::Sphere) {
            bool hit = SphereVsTriangleMesh(bodyB, bodyA, outManifold);
            if (hit) {
                outManifold.bodyA = bodyA;
                outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal;
            }
            return hit;
        }
        if (typeA == ShapeType::Box && typeB == ShapeType::TriangleMesh) {
            return BoxVsTriangleMesh(bodyA, bodyB, outManifold);
        }
        if (typeA == ShapeType::TriangleMesh && typeB == ShapeType::Box) {
            bool hit = BoxVsTriangleMesh(bodyB, bodyA, outManifold);
            if (hit) {
                outManifold.bodyA = bodyA;
                outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal;
            }
            return hit;
        }

        if (typeA == ShapeType::Sphere && typeB == ShapeType::ConvexMesh) {
            return SphereVsConvexMesh(bodyA, bodyB, outManifold);
        }
        if (typeA == ShapeType::ConvexMesh && typeB == ShapeType::Sphere) {
            bool hit = SphereVsConvexMesh(bodyB, bodyA, outManifold);
            if (hit) {
                outManifold.bodyA = bodyA;
                outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal;
            }
            return hit;
        }
        if (typeA == ShapeType::Box && typeB == ShapeType::ConvexMesh) {
            return BoxVsConvexMesh(bodyA, bodyB, outManifold);
        }
        if (typeA == ShapeType::ConvexMesh && typeB == ShapeType::Box) {
            bool hit = BoxVsConvexMesh(bodyB, bodyA, outManifold);
            if (hit) {
                outManifold.bodyA = bodyA;
                outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal;
            }
            return hit;
        }
        if (typeA == ShapeType::ConvexMesh && typeB == ShapeType::ConvexMesh) {
            return ConvexMeshVsConvexMesh(bodyA, bodyB, outManifold);
        }
        if (typeA == ShapeType::ConvexMesh && typeB == ShapeType::TriangleMesh) {
            return ConvexMeshVsTriangleMesh(bodyA, bodyB, outManifold);
        }
        if (typeA == ShapeType::TriangleMesh && typeB == ShapeType::ConvexMesh) {
            bool hit = ConvexMeshVsTriangleMesh(bodyB, bodyA, outManifold);
            if (hit) {
                outManifold.bodyA = bodyA;
                outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal;
            }
            return hit;
        }

        return false;
    }

    bool CollisionDispatch::SphereVsSphere(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold) {
        auto sA = std::static_pointer_cast<SphereShape>(bodyA->GetShape());
        auto sB = std::static_pointer_cast<SphereShape>(bodyB->GetShape());

        Vector3 delta = bodyB->GetPosition() - bodyA->GetPosition();
        float distSq = delta.LengthSquared();
        float radiusSum = sA->GetRadius() + sB->GetRadius();

        if (distSq > radiusSum * radiusSum) return false;

        float dist = std::sqrt(distSq);
        Vector3 normal(0.0f, 1.0f, 0.0f);
        if (dist > EPSILON) {
            normal = delta * (1.0f / dist);
        }

        ContactPoint cp;
        cp.penetration = radiusSum - dist;
        cp.position = bodyA->GetPosition() + normal * sA->GetRadius();
        cp.featureId = 0;

        outManifold.normal = normal; // Points from A to B
        outManifold.AddPoint(cp);
        return true;
    }

    bool CollisionDispatch::SphereVsPlane(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold) {
        // bodyA is Sphere, bodyB is Plane
        auto sphere = std::static_pointer_cast<SphereShape>(bodyA->GetShape());
        auto plane = std::static_pointer_cast<PlaneShape>(bodyB->GetShape());

        Vector3 planeNorm = plane->GetNormal();
        float dist = bodyA->GetPosition().Dot(planeNorm) - plane->GetDistance();

        if (dist > sphere->GetRadius()) return false;

        ContactPoint cp;
        cp.penetration = sphere->GetRadius() - dist;
        cp.position = bodyA->GetPosition() - planeNorm * sphere->GetRadius();
        cp.featureId = 0;

        // Normal points from A (Sphere) to B (Plane). Since plane normal points outward towards sphere, normal from Sphere to Plane is -planeNorm.
        outManifold.normal = -planeNorm;
        outManifold.AddPoint(cp);
        return true;
    }

    bool CollisionDispatch::SphereVsBox(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold) {
        // bodyA is Sphere, bodyB is Box
        auto sphere = std::static_pointer_cast<SphereShape>(bodyA->GetShape());
        auto box = std::static_pointer_cast<BoxShape>(bodyB->GetShape());

        Transform boxTrans = bodyB->GetTransform();
        Quaternion boxRot = boxTrans.rotation.Normalized();
        Quaternion invBoxRot(-boxRot.x, -boxRot.y, -boxRot.z, boxRot.w);
        Vector3 h = box->GetHalfExtents();
        float radius = sphere->GetRadius();

        Vector3 localSpherePos = invBoxRot.Rotate(bodyA->GetPosition() - boxTrans.position);

        Vector3 closestPoint(
            (std::max)(-h.x, (std::min)(localSpherePos.x, h.x)),
            (std::max)(-h.y, (std::min)(localSpherePos.y, h.y)),
            (std::max)(-h.z, (std::min)(localSpherePos.z, h.z))
        );

        Vector3 diff = localSpherePos - closestPoint;
        float distSq = diff.LengthSquared();

        Vector3 localNorm;
        float penetration = 0.0f;

        if (distSq > EPSILON * EPSILON) {
            if (distSq > radius * radius) return false;

            float dist = std::sqrt(distSq);
            localNorm = diff * (1.0f / dist);
            penetration = radius - dist;
        } else {
            // Sphere center is inside the box — push out through the nearest face
            float distX = h.x - std::abs(localSpherePos.x);
            float distY = h.y - std::abs(localSpherePos.y);
            float distZ = h.z - std::abs(localSpherePos.z);

            if (distX <= distY && distX <= distZ) {
                float sign = localSpherePos.x >= 0.0f ? 1.0f : -1.0f;
                localNorm = Vector3(sign, 0.0f, 0.0f);
                penetration = distX + radius;
                closestPoint.x = sign * h.x;
            } else if (distY <= distX && distY <= distZ) {
                float sign = localSpherePos.y >= 0.0f ? 1.0f : -1.0f;
                localNorm = Vector3(0.0f, sign, 0.0f);
                penetration = distY + radius;
                closestPoint.y = sign * h.y;
            } else {
                float sign = localSpherePos.z >= 0.0f ? 1.0f : -1.0f;
                localNorm = Vector3(0.0f, 0.0f, sign);
                penetration = distZ + radius;
                closestPoint.z = sign * h.z;
            }
        }

        Vector3 worldNorm = boxRot.Rotate(localNorm);
        float worldNormLen = worldNorm.Length();
        if (worldNormLen > EPSILON) {
            worldNorm *= (1.0f / worldNormLen);
        } else {
            worldNorm = Vector3(0.0f, 1.0f, 0.0f);
        }

        Vector3 worldClosest = boxTrans.position + boxRot.Rotate(closestPoint);

        ContactPoint cp;
        cp.penetration = penetration;
        cp.position = worldClosest;
        cp.featureId = 0;

        outManifold.normal = -worldNorm; // Points from Sphere (A) to Box (B)
        outManifold.AddPoint(cp);
        return true;
    }

    bool CollisionDispatch::BoxVsPlane(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold) {
        // bodyA is Box, bodyB is Plane
        auto box = std::static_pointer_cast<BoxShape>(bodyA->GetShape());
        auto plane = std::static_pointer_cast<PlaneShape>(bodyB->GetShape());

        Vector3 planeNorm = plane->GetNormal();
        float planeDist = plane->GetDistance();

        Transform t = bodyA->GetTransform();
        Matrix3x3 rot = Matrix3x3::FromQuaternion(t.rotation);
        Vector3 h = box->GetHalfExtents();

        Vector3 localVertices[8] = {
            {-h.x, -h.y, -h.z}, { h.x, -h.y, -h.z}, { h.x,  h.y, -h.z}, {-h.x,  h.y, -h.z},
            {-h.x, -h.y,  h.z}, { h.x, -h.y,  h.z}, { h.x,  h.y,  h.z}, {-h.x,  h.y,  h.z}
        };

        // Normal points from A (Box) to B (Plane): -planeNorm
        outManifold.normal = -planeNorm;

        for (uint32_t i = 0; i < 8; ++i) {
            Vector3 worldPos = t.position + rot * localVertices[i];
            float dist = worldPos.Dot(planeNorm) - planeDist;
            if (dist < 0.0f) {
                ContactPoint cp;
                cp.penetration = -dist;
                cp.position = worldPos;
                cp.featureId = i;
                outManifold.AddPoint(cp);
                if (outManifold.pointCount >= 4) break;
            }
        }

        return outManifold.pointCount > 0;
    }

    static float ProjectRadius(const OBB& obb, const Vector3& axis) {
        return obb.extents.x * std::abs(axis.Dot(obb.GetAxis(0))) +
               obb.extents.y * std::abs(axis.Dot(obb.GetAxis(1))) +
               obb.extents.z * std::abs(axis.Dot(obb.GetAxis(2)));
    }

    static float ExtentComponent(const Vector3& e, int axis) {
        return axis == 0 ? e.x : (axis == 1 ? e.y : e.z);
    }

    static int ClipPolygonAgainstPlane(const Vector3* inPts, int inCount,
                                       Vector3* outPts,
                                       const Vector3& planeN, float planeOffset) {
        int outCount = 0;
        if (inCount == 0) return 0;

        Vector3 prev = inPts[inCount - 1];
        float prevDist = prev.Dot(planeN) - planeOffset;
        for (int i = 0; i < inCount; ++i) {
            Vector3 curr = inPts[i];
            float currDist = curr.Dot(planeN) - planeOffset;
            bool prevInside = prevDist <= 0.0f;
            bool currInside = currDist <= 0.0f;

            if (currInside) {
                if (!prevInside && outCount < 8) {
                    float denom = prevDist - currDist;
                    float t = (std::abs(denom) > EPSILON) ? (prevDist / denom) : 0.0f;
                    outPts[outCount++] = prev + (curr - prev) * t;
                }
                if (outCount < 8) {
                    outPts[outCount++] = curr;
                }
            } else if (prevInside && outCount < 8) {
                float denom = prevDist - currDist;
                float t = (std::abs(denom) > EPSILON) ? (prevDist / denom) : 0.0f;
                outPts[outCount++] = prev + (curr - prev) * t;
            }

            prev = curr;
            prevDist = currDist;
        }
        return outCount;
    }

    static void ClosestPointsOnSegments(const Vector3& a0, const Vector3& a1,
                                        const Vector3& b0, const Vector3& b1,
                                        Vector3& outA, Vector3& outB) {
        Vector3 dA = a1 - a0;
        Vector3 dB = b1 - b0;
        Vector3 r = a0 - b0;
        float a = dA.LengthSquared();
        float e = dB.LengthSquared();
        float f = dB.Dot(r);

        float s = 0.0f;
        float t = 0.0f;

        if (a <= EPSILON * EPSILON && e <= EPSILON * EPSILON) {
            outA = a0;
            outB = b0;
            return;
        }

        if (a <= EPSILON * EPSILON) {
            t = std::clamp(f / e, 0.0f, 1.0f);
        } else {
            float c = dA.Dot(r);
            if (e <= EPSILON * EPSILON) {
                s = std::clamp(-c / a, 0.0f, 1.0f);
            } else {
                float b = dA.Dot(dB);
                float denom = a * e - b * b;
                if (denom != 0.0f) {
                    s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
                }
                t = (b * s + f) / e;
                if (t < 0.0f) {
                    t = 0.0f;
                    s = std::clamp(-c / a, 0.0f, 1.0f);
                } else if (t > 1.0f) {
                    t = 1.0f;
                    s = std::clamp((b - c) / a, 0.0f, 1.0f);
                }
            }
        }

        outA = a0 + dA * s;
        outB = b0 + dB * t;
    }

    static Vector3 SupportVertex(const OBB& obb, const Vector3& dir) {
        return obb.center
            + obb.GetAxis(0) * (obb.extents.x * (obb.GetAxis(0).Dot(dir) >= 0.0f ? 1.0f : -1.0f))
            + obb.GetAxis(1) * (obb.extents.y * (obb.GetAxis(1).Dot(dir) >= 0.0f ? 1.0f : -1.0f))
            + obb.GetAxis(2) * (obb.extents.z * (obb.GetAxis(2).Dot(dir) >= 0.0f ? 1.0f : -1.0f));
    }

    static void SupportingEdge(const OBB& obb, int edgeAxis, const Vector3& searchDir,
                               Vector3& outA, Vector3& outB) {
        Vector3 mid = obb.center;
        for (int k = 0; k < 3; ++k) {
            if (k == edgeAxis) continue;
            float s = obb.GetAxis(k).Dot(searchDir) >= 0.0f ? 1.0f : -1.0f;
            mid += obb.GetAxis(k) * (ExtentComponent(obb.extents, k) * s);
        }
        Vector3 half = obb.GetAxis(edgeAxis) * ExtentComponent(obb.extents, edgeAxis);
        outA = mid - half;
        outB = mid + half;
    }

    static int IncidentFaceAxis(const OBB& obb, const Vector3& refNormal) {
        int best = 0;
        float bestDot = std::numeric_limits<float>::max();
        for (int i = 0; i < 3; ++i) {
            float d = obb.GetAxis(i).Dot(refNormal);
            if (d < bestDot) {
                bestDot = d;
                best = i;
            }
            if (-d < bestDot) {
                bestDot = -d;
                best = i;
            }
        }
        return best;
    }

    static void GetBoxFace(const OBB& obb, int axis, float sign,
                           Vector3& faceCenter, Vector3& faceNormal,
                           Vector3 verts[4]) {
        faceNormal = obb.GetAxis(axis) * sign;
        float ext = ExtentComponent(obb.extents, axis);
        faceCenter = obb.center + faceNormal * ext;

        int i1 = (axis + 1) % 3;
        int i2 = (axis + 2) % 3;
        Vector3 t1 = obb.GetAxis(i1) * ExtentComponent(obb.extents, i1);
        Vector3 t2 = obb.GetAxis(i2) * ExtentComponent(obb.extents, i2);
        verts[0] = faceCenter + t1 + t2;
        verts[1] = faceCenter - t1 + t2;
        verts[2] = faceCenter - t1 - t2;
        verts[3] = faceCenter + t1 - t2;
    }

    static void ReduceManifoldPoints(Vector3* pts, float* pens, int& count) {
        if (count <= 4) return;

        int iDeep = 0;
        for (int i = 1; i < count; ++i) {
            if (pens[i] > pens[iDeep]) iDeep = i;
        }

        int iFar = 0;
        float bestFar = -1.0f;
        for (int i = 0; i < count; ++i) {
            Vector3 d = pts[i] - pts[iDeep];
            float distSq = d.LengthSquared();
            if (distSq > bestFar) {
                bestFar = distSq;
                iFar = i;
            }
        }

        Vector3 line = pts[iFar] - pts[iDeep];
        Vector3 sideRef = (std::abs(line.x) < 0.9f) ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 1.0f, 0.0f);

        int iPos = -1;
        int iNeg = -1;
        float bestPos = -1.0f;
        float bestNeg = -1.0f;
        for (int i = 0; i < count; ++i) {
            Vector3 cross = line.Cross(pts[i] - pts[iDeep]);
            float mag = cross.LengthSquared();
            if (sideRef.Dot(cross) >= 0.0f) {
                if (mag > bestPos) {
                    bestPos = mag;
                    iPos = i;
                }
            } else if (mag > bestNeg) {
                bestNeg = mag;
                iNeg = i;
            }
        }

        int keep[4];
        int k = 0;
        auto TryAdd = [&](int idx) {
            if (idx < 0 || k >= 4) return;
            for (int j = 0; j < k; ++j) {
                if (keep[j] == idx) return;
            }
            keep[k++] = idx;
        };
        TryAdd(iDeep);
        TryAdd(iFar);
        TryAdd(iPos);
        TryAdd(iNeg);

        Vector3 npts[4];
        float npens[4];
        for (int i = 0; i < k; ++i) {
            npts[i] = pts[keep[i]];
            npens[i] = pens[keep[i]];
        }
        for (int i = 0; i < k; ++i) {
            pts[i] = npts[i];
            pens[i] = npens[i];
        }
        count = k;
    }

    bool CollisionDispatch::BoxVsBox(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold) {
        auto shapeA = std::static_pointer_cast<BoxShape>(bodyA->GetShape());
        auto shapeB = std::static_pointer_cast<BoxShape>(bodyB->GetShape());

        OBB obbA(bodyA->GetPosition(), shapeA->GetHalfExtents(), bodyA->GetRotation().Normalized());
        OBB obbB(bodyB->GetPosition(), shapeB->GetHalfExtents(), bodyB->GetRotation().Normalized());

        Vector3 aAxes[3] = { obbA.GetAxis(0), obbA.GetAxis(1), obbA.GetAxis(2) };
        Vector3 bAxes[3] = { obbB.GetAxis(0), obbB.GetAxis(1), obbB.GetAxis(2) };

        Vector3 delta = obbB.center - obbA.center;

        enum class AxisOwner { AFace, BFace, Edge };
        float minOverlap = std::numeric_limits<float>::max();
        Vector3 bestAxis;
        AxisOwner bestOwner = AxisOwner::AFace;
        int bestIndexA = 0;
        int bestIndexB = 0;

        auto TestAxis = [&](const Vector3& rawAxis, AxisOwner owner, int indexA, int indexB) -> bool {
            float lenSq = rawAxis.LengthSquared();
            if (lenSq < EPSILON * EPSILON) return true;

            Vector3 axis = rawAxis * (1.0f / std::sqrt(lenSq));
            float rA = ProjectRadius(obbA, axis);
            float rB = ProjectRadius(obbB, axis);
            float dist = std::abs(delta.Dot(axis));
            float overlap = (rA + rB) - dist;

            if (overlap < 0.0f) return false;

            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = (delta.Dot(axis) < 0.0f) ? -axis : axis;
                bestOwner = owner;
                bestIndexA = indexA;
                bestIndexB = indexB;
            }
            return true;
        };

        for (int i = 0; i < 3; ++i) {
            if (!TestAxis(aAxes[i], AxisOwner::AFace, i, -1)) return false;
        }
        for (int i = 0; i < 3; ++i) {
            if (!TestAxis(bAxes[i], AxisOwner::BFace, -1, i)) return false;
        }
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (!TestAxis(aAxes[i].Cross(bAxes[j]), AxisOwner::Edge, i, j)) return false;
            }
        }

        outManifold.normal = bestAxis;

        if (bestOwner == AxisOwner::Edge) {
            Vector3 a0, a1, b0, b1;
            SupportingEdge(obbA, bestIndexA, bestAxis, a0, a1);
            SupportingEdge(obbB, bestIndexB, -bestAxis, b0, b1);
            Vector3 pA, pB;
            ClosestPointsOnSegments(a0, a1, b0, b1, pA, pB);
            ContactPoint cp;
            cp.position = (pA + pB) * 0.5f;
            cp.penetration = minOverlap;
            cp.featureId = 100u + static_cast<uint32_t>(bestIndexA * 3 + bestIndexB);
            outManifold.AddPoint(cp);
            return true;
        }

        const bool refIsA = (bestOwner == AxisOwner::AFace);
        const OBB& ref = refIsA ? obbA : obbB;
        const OBB& inc = refIsA ? obbB : obbA;
        Vector3 refNormal = refIsA ? bestAxis : -bestAxis;

        int refAxis = refIsA ? bestIndexA : bestIndexB;
        float refSign = ref.GetAxis(refAxis).Dot(refNormal) >= 0.0f ? 1.0f : -1.0f;

        Vector3 refCenter, refFaceN, refVerts[4];
        GetBoxFace(ref, refAxis, refSign, refCenter, refFaceN, refVerts);
        (void)refVerts;

        int incAxis = IncidentFaceAxis(inc, refFaceN);
        float incSign = inc.GetAxis(incAxis).Dot(refFaceN) < 0.0f ? 1.0f : -1.0f;
        Vector3 incCenter, incFaceN, incVerts[4];
        GetBoxFace(inc, incAxis, incSign, incCenter, incFaceN, incVerts);
        (void)incCenter;
        (void)incFaceN;

        int i1 = (refAxis + 1) % 3;
        int i2 = (refAxis + 2) % 3;
        Vector3 sideN1 = ref.GetAxis(i1);
        Vector3 sideN2 = ref.GetAxis(i2);
        float e1 = ExtentComponent(ref.extents, i1);
        float e2 = ExtentComponent(ref.extents, i2);

        Vector3 clipA[8];
        Vector3 clipB[8];
        int count = 4;
        for (int i = 0; i < 4; ++i) clipA[i] = incVerts[i];

        // Keep points inside the reference face side planes
        count = ClipPolygonAgainstPlane(clipA, count, clipB,  sideN1,  ref.center.Dot(sideN1) + e1);
        count = ClipPolygonAgainstPlane(clipB, count, clipA, -sideN1, -ref.center.Dot(sideN1) + e1);
        count = ClipPolygonAgainstPlane(clipA, count, clipB,  sideN2,  ref.center.Dot(sideN2) + e2);
        count = ClipPolygonAgainstPlane(clipB, count, clipA, -sideN2, -ref.center.Dot(sideN2) + e2);

        if (count == 0) {
            ContactPoint cp;
            cp.position = SupportVertex(inc, -refFaceN);
            cp.penetration = minOverlap;
            cp.featureId = 0;
            outManifold.AddPoint(cp);
            return true;
        }

        Vector3 kept[8];
        float pens[8];
        int keptCount = 0;
        float planeOffset = refCenter.Dot(refFaceN);
        for (int i = 0; i < count; ++i) {
            float sep = clipA[i].Dot(refFaceN) - planeOffset;
            if (sep <= 0.0f) {
                kept[keptCount] = clipA[i];
                pens[keptCount] = -sep;
                ++keptCount;
            }
        }

        if (keptCount == 0) {
            ContactPoint cp;
            cp.position = SupportVertex(inc, -refFaceN);
            cp.penetration = minOverlap;
            cp.featureId = 0;
            outManifold.AddPoint(cp);
            return true;
        }

        ReduceManifoldPoints(kept, pens, keptCount);

        for (int i = 0; i < keptCount; ++i) {
            ContactPoint cp;
            cp.position = kept[i];
            cp.penetration = pens[i];
            cp.featureId = static_cast<uint32_t>(i);
            outManifold.AddPoint(cp);
        }

        return true;
    }
    // Ближайшая точка на треугольнике к точке p
    static Vector3 ClosestPointOnTriangle(const Vector3& p, const Triangle& tri) {
        Vector3 ab = tri.b - tri.a;
        Vector3 ac = tri.c - tri.a;
        Vector3 ap = p - tri.a;

        float d1 = ab.Dot(ap);
        float d2 = ac.Dot(ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return tri.a;

        Vector3 bp = p - tri.b;
        float d3 = ab.Dot(bp);
        float d4 = ac.Dot(bp);
        if (d3 >= 0.0f && d4 <= d3) return tri.b;

        Vector3 cp = p - tri.c;
        float d5 = ab.Dot(cp);
        float d6 = ac.Dot(cp);
        if (d6 >= 0.0f && d5 <= d6) return tri.c;

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
            float v = d1 / (d1 - d3);
            return tri.a + ab * v;
        }

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
            float w = d2 / (d2 - d6);
            return tri.a + ac * w;
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            return tri.b + (tri.c - tri.b) * w;
        }

        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom;
        float w = vc * denom;
        return tri.a + ab * v + ac * w;
    }

    bool CollisionDispatch::SphereVsTriangleMesh(RigidBody* bodySphere, RigidBody* bodyMesh,
        ContactManifold& outManifold) {
        auto sphere = std::static_pointer_cast<SphereShape>(bodySphere->GetShape());
        auto mesh = std::static_pointer_cast<TriangleMeshShape>(bodyMesh->GetShape());

        float radius = sphere->GetRadius();
        Vector3 sphereWorld = bodySphere->GetPosition();

        // Трансформировать центр сферы в локальное пространство меша
        Transform meshTransform = bodyMesh->GetTransform();
        Matrix3x3 meshRot = Matrix3x3::FromQuaternion(meshTransform.rotation);
        Matrix3x3 meshRotInv = meshRot.Transposed();
        Vector3 sphereLocal = meshRotInv * (sphereWorld - meshTransform.position);

        // AABB сферы в локальном пространстве меша для BVH запроса
        AABB sphereAABB(
            Vector3(sphereLocal.x - radius, sphereLocal.y - radius, sphereLocal.z - radius),
            Vector3(sphereLocal.x + radius, sphereLocal.y + radius, sphereLocal.z + radius)
        );

        const auto& triangles = mesh->GetTriangles();
        const auto& bvh = mesh->GetBVH();

        // Найти лучший контакт (наибольшее проникновение)
        struct BestContact {
            float penetration = -1.0f;
            Vector3 normal;
            Vector3 position;
        } best;

        bvh.Query(sphereAABB, [&](int triIdx) {
            const Triangle& tri = triangles[triIdx];

            Vector3 closest = ClosestPointOnTriangle(sphereLocal, tri);
            Vector3 diff = sphereLocal - closest;
            float distSq = diff.LengthSquared();

            if (distSq < radius * radius) {
                float dist = std::sqrt(distSq);
                float penetration = radius - dist;

                Vector3 localNormal;
                if (dist > EPSILON) {
                    localNormal = diff * (1.0f / dist);
                }
                else {
                    // Центр сферы прямо на треугольнике — используем нормаль треугольника
                    localNormal = tri.normal;
                    penetration = radius;
                }

                if (penetration > best.penetration) {
                    best.penetration = penetration;
                    best.normal = localNormal;
                    best.position = closest;
                }
            }
            });

        if (best.penetration < 0.0f) return false;
        // Отладка — временно

        // Трансформировать обратно в мировое пространство
        Vector3 worldNormal = meshRot * best.normal;
        Vector3 worldPosition = meshTransform.position + meshRot * best.position;

        ContactPoint cp;
        cp.penetration = best.penetration;
        cp.position = worldPosition;
        cp.featureId = 0;

        // Normal от Sphere (A) к Mesh (B)
        outManifold.normal = -worldNormal;
        outManifold.AddPoint(cp);
        return true;
    }

    struct OBBTriHit {
        Vector3 normal; // from triangle toward box, mesh-local
        float penetration{ 0.0f };
        Vector3 points[8];
        float pens[8];
        int count{ 0 };
    };

    static bool ClipIncidentToTriangle(const Vector3* incVerts, const Triangle& tri,
                                       const Vector3& refNormal, OBBTriHit& out) {
        Vector3 triVerts[3] = { tri.a, tri.b, tri.c };
        Vector3 buffers[2][8];
        int count = 4;
        int src = 0;
        for (int i = 0; i < 4; ++i) buffers[0][i] = incVerts[i];

        for (int e = 0; e < 3; ++e) {
            Vector3 e0 = triVerts[e];
            Vector3 e1 = triVerts[(e + 1) % 3];
            Vector3 edge = e1 - e0;
            Vector3 planeN = refNormal.Cross(edge);
            float nLenSq = planeN.LengthSquared();
            if (nLenSq < EPSILON * EPSILON) continue;
            planeN = planeN * (1.0f / std::sqrt(nLenSq));
            Vector3 other = triVerts[(e + 2) % 3];
            if (planeN.Dot(other) > planeN.Dot(e0)) {
                planeN = -planeN;
            }

            int dst = 1 - src;
            count = ClipPolygonAgainstPlane(buffers[src], count, buffers[dst], planeN, planeN.Dot(e0));
            if (count == 0) return false;
            src = dst;
        }

        float planeOffset = tri.a.Dot(refNormal);
        out.count = 0;
        out.penetration = 0.0f;
        for (int i = 0; i < count; ++i) {
            float sep = buffers[src][i].Dot(refNormal) - planeOffset;
            if (sep <= 0.0f) {
                out.points[out.count] = buffers[src][i];
                out.pens[out.count] = -sep;
                if (-sep > out.penetration) out.penetration = -sep;
                ++out.count;
                if (out.count >= 8) break;
            }
        }
        return out.count > 0;
    }

    static bool OBBVsTriangle(const OBB& obb, const Triangle& tri, OBBTriHit& out) {
        out = {};
        Vector3 triVerts[3] = { tri.a, tri.b, tri.c };
        Vector3 triEdges[3] = { tri.b - tri.a, tri.c - tri.b, tri.a - tri.c };
        Vector3 boxAxes[3] = { obb.GetAxis(0), obb.GetAxis(1), obb.GetAxis(2) };

        Vector3 triN = tri.normal;
        if (triN.LengthSquared() < EPSILON * EPSILON) {
            triN = triEdges[0].Cross(triEdges[1]);
            float nLenSq = triN.LengthSquared();
            if (nLenSq < EPSILON * EPSILON) return false;
            triN = triN * (1.0f / std::sqrt(nLenSq));
        }

        enum class AxisOwner { TriFace, BoxFace, Edge };
        float minOverlap = std::numeric_limits<float>::max();
        Vector3 bestAxis = triN;
        AxisOwner bestOwner = AxisOwner::TriFace;
        int bestBoxAxis = 0;
        int bestTriEdge = 0;

        auto TestAxis = [&](Vector3 rawAxis, AxisOwner owner, int boxAxis, int triEdge) -> bool {
            float lenSq = rawAxis.LengthSquared();
            if (lenSq < EPSILON * EPSILON) return true;
            Vector3 axis = rawAxis * (1.0f / std::sqrt(lenSq));

            float boxR = ProjectRadius(obb, axis);
            float boxC = obb.center.Dot(axis);
            float boxMin = boxC - boxR;
            float boxMax = boxC + boxR;

            float tMin = triVerts[0].Dot(axis);
            float tMax = tMin;
            for (int i = 1; i < 3; ++i) {
                float d = triVerts[i].Dot(axis);
                tMin = (std::min)(tMin, d);
                tMax = (std::max)(tMax, d);
            }

            float overlap = (std::min)(boxMax, tMax) - (std::max)(boxMin, tMin);
            if (overlap < 0.0f) return false;

            // Prefer the triangle face so a box sitting on a ramp doesn't pick an edge axis.
            float weighted = (owner == AxisOwner::TriFace) ? overlap * 0.95f : overlap;
            if (weighted < minOverlap) {
                minOverlap = weighted;
                out.penetration = overlap;
                bestAxis = (boxC >= (tMin + tMax) * 0.5f) ? axis : -axis;
                bestOwner = owner;
                bestBoxAxis = boxAxis;
                bestTriEdge = triEdge;
            }
            return true;
        };

        if (!TestAxis(triN, AxisOwner::TriFace, -1, -1)) return false;
        for (int i = 0; i < 3; ++i) {
            if (!TestAxis(boxAxes[i], AxisOwner::BoxFace, i, -1)) return false;
        }
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (!TestAxis(boxAxes[i].Cross(triEdges[j]), AxisOwner::Edge, i, j)) return false;
            }
        }

        out.normal = bestAxis;
        out.count = 0;

        if (bestOwner == AxisOwner::Edge) {
            Vector3 a0, a1;
            SupportingEdge(obb, bestBoxAxis, -bestAxis, a0, a1);
            Vector3 b0 = triVerts[bestTriEdge];
            Vector3 b1 = triVerts[(bestTriEdge + 1) % 3];
            Vector3 pA, pB;
            ClosestPointsOnSegments(a0, a1, b0, b1, pA, pB);
            out.points[0] = (pA + pB) * 0.5f;
            out.pens[0] = out.penetration;
            out.count = 1;
            return true;
        }

        if (bestOwner == AxisOwner::TriFace) {
            int incAxis = IncidentFaceAxis(obb, bestAxis);
            float incSign = obb.GetAxis(incAxis).Dot(bestAxis) < 0.0f ? 1.0f : -1.0f;
            Vector3 incCenter, incFaceN, incVerts[4];
            GetBoxFace(obb, incAxis, incSign, incCenter, incFaceN, incVerts);
            (void)incCenter;
            (void)incFaceN;
            if (ClipIncidentToTriangle(incVerts, tri, bestAxis, out)) {
                out.normal = bestAxis;
                return true;
            }
            Vector3 support = SupportVertex(obb, -bestAxis);
            Vector3 closest = ClosestPointOnTriangle(support, tri);
            out.points[0] = closest;
            out.pens[0] = out.penetration;
            out.count = 1;
            return true;
        }

        Vector3 refNormal = -bestAxis;
        int refAxis = bestBoxAxis;
        float refSign = obb.GetAxis(refAxis).Dot(refNormal) >= 0.0f ? 1.0f : -1.0f;
        Vector3 refCenter, refFaceN, refVerts[4];
        GetBoxFace(obb, refAxis, refSign, refCenter, refFaceN, refVerts);
        (void)refVerts;

        int i1 = (refAxis + 1) % 3;
        int i2 = (refAxis + 2) % 3;
        Vector3 sideN1 = obb.GetAxis(i1);
        Vector3 sideN2 = obb.GetAxis(i2);
        float e1 = ExtentComponent(obb.extents, i1);
        float e2 = ExtentComponent(obb.extents, i2);

        Vector3 clipA[8];
        Vector3 clipB[8];
        int count = 3;
        clipA[0] = tri.a;
        clipA[1] = tri.b;
        clipA[2] = tri.c;

        count = ClipPolygonAgainstPlane(clipA, count, clipB,  sideN1,  obb.center.Dot(sideN1) + e1);
        count = ClipPolygonAgainstPlane(clipB, count, clipA, -sideN1, -obb.center.Dot(sideN1) + e1);
        count = ClipPolygonAgainstPlane(clipA, count, clipB,  sideN2,  obb.center.Dot(sideN2) + e2);
        count = ClipPolygonAgainstPlane(clipB, count, clipA, -sideN2, -obb.center.Dot(sideN2) + e2);

        float planeOffset = refCenter.Dot(refFaceN);
        int kept = 0;
        float maxPen = 0.0f;
        for (int i = 0; i < count; ++i) {
            float sep = clipA[i].Dot(refFaceN) - planeOffset;
            if (sep <= 0.0f) {
                out.points[kept] = clipA[i];
                out.pens[kept] = -sep;
                if (-sep > maxPen) maxPen = -sep;
                ++kept;
                if (kept >= 8) break;
            }
        }

        if (kept == 0) {
            Vector3 closest = ClosestPointOnTriangle(obb.center, tri);
            out.points[0] = closest;
            out.pens[0] = out.penetration;
            out.count = 1;
        } else {
            out.count = kept;
            out.penetration = maxPen;
        }
        out.normal = bestAxis;
        return true;
    }

    bool CollisionDispatch::BoxVsTriangleMesh(RigidBody* bodyBox, RigidBody* bodyMesh,
        ContactManifold& outManifold) {
        auto box = std::static_pointer_cast<BoxShape>(bodyBox->GetShape());
        auto mesh = std::static_pointer_cast<TriangleMeshShape>(bodyMesh->GetShape());

        Transform meshTransform = bodyMesh->GetTransform();
        Matrix3x3 meshRot = Matrix3x3::FromQuaternion(meshTransform.rotation);
        Matrix3x3 meshRotInv = meshRot.Transposed();

        Transform boxTransform = bodyBox->GetTransform();
        Matrix3x3 boxRot = Matrix3x3::FromQuaternion(boxTransform.rotation);
        Vector3 localCenter = meshRotInv * (boxTransform.position - meshTransform.position);
        Matrix3x3 localBoxRot = meshRotInv * boxRot;
        OBB localObb(localCenter, box->GetHalfExtents(), localBoxRot);
        AABB queryAABB = localObb.ComputeAABB();

        const auto& triangles = mesh->GetTriangles();
        const auto& bvh = mesh->GetBVH();

        struct Cand {
            Vector3 pos;
            Vector3 normal;
            float pen{ 0.0f };
            uint32_t featureId{ 0 };
        };
        Cand cands[32];
        int candCount = 0;
        Vector3 bestNormal(0.0f, 1.0f, 0.0f);
        float bestPen = -1.0f;

        bvh.Query(queryAABB, [&](int triIdx) {
            const Triangle& tri = triangles[triIdx];
            OBBTriHit hit;
            if (!OBBVsTriangle(localObb, tri, hit) || hit.count <= 0) return;

            if (hit.penetration > bestPen) {
                bestPen = hit.penetration;
                bestNormal = hit.normal;
            }

            for (int i = 0; i < hit.count && candCount < 32; ++i) {
                Cand& c = cands[candCount++];
                c.pos = hit.points[i];
                c.normal = hit.normal;
                c.pen = hit.pens[i];
                c.featureId = (static_cast<uint32_t>(triIdx) << 3) | static_cast<uint32_t>(i);
            }
        });

        if (candCount == 0 || bestPen < 0.0f) return false;

        float nLen = bestNormal.Length();
        if (nLen > EPSILON) {
            bestNormal = bestNormal * (1.0f / nLen);
        } else {
            bestNormal = Vector3(0.0f, 1.0f, 0.0f);
        }

        Vector3 keptPts[32];
        float keptPens[32];
        uint32_t keptIds[32];
        int keptCount = 0;
        for (int i = 0; i < candCount; ++i) {
            if (cands[i].pen <= 0.0f) continue;
            float nDot = cands[i].normal.Dot(bestNormal);
            if (nDot < 0.7f) continue;
            keptPts[keptCount] = cands[i].pos;
            keptPens[keptCount] = cands[i].pen;
            keptIds[keptCount] = cands[i].featureId;
            ++keptCount;
        }
        if (keptCount == 0) return false;

        if (keptCount > 4) {
            Vector3 reducePts[32];
            float reducePens[32];
            for (int i = 0; i < keptCount; ++i) {
                reducePts[i] = keptPts[i];
                reducePens[i] = keptPens[i];
            }
            int reduceCount = keptCount;
            ReduceManifoldPoints(reducePts, reducePens, reduceCount);

            uint32_t newIds[4];
            for (int i = 0; i < reduceCount; ++i) {
                uint32_t id = keptIds[0];
                float bestDist = std::numeric_limits<float>::max();
                for (int j = 0; j < keptCount; ++j) {
                    float distSq = (reducePts[i] - keptPts[j]).LengthSquared();
                    if (distSq < bestDist) {
                        bestDist = distSq;
                        id = keptIds[j];
                    }
                }
                newIds[i] = id;
            }
            keptCount = reduceCount;
            for (int i = 0; i < keptCount; ++i) {
                keptPts[i] = reducePts[i];
                keptPens[i] = reducePens[i];
                keptIds[i] = newIds[i];
            }
        }

        Vector3 worldNormal = meshRot * bestNormal;
        float worldNLen = worldNormal.Length();
        if (worldNLen > EPSILON) {
            worldNormal = worldNormal * (1.0f / worldNLen);
        } else {
            worldNormal = Vector3(0.0f, 1.0f, 0.0f);
        }

        outManifold.normal = -worldNormal;
        for (int i = 0; i < keptCount; ++i) {
            ContactPoint cp;
            cp.position = meshTransform.position + meshRot * keptPts[i];
            cp.penetration = keptPens[i];
            cp.featureId = keptIds[i];
            outManifold.AddPoint(cp);
        }
        return outManifold.pointCount > 0;
    }

    static Vector3 InverseRotate(const Quaternion& q, const Vector3& v) {
        return Quaternion(-q.x, -q.y, -q.z, q.w).Rotate(v);
    }

    static Vector3 SupportWorld(RigidBody* body, const Vector3& dir) {
        auto shape = body->GetShape();
        const Transform& t = body->GetTransform();
        float lenSq = dir.LengthSquared();
        Vector3 n = (lenSq > EPSILON * EPSILON) ? dir * (1.0f / std::sqrt(lenSq)) : Vector3(1.0f, 0.0f, 0.0f);

        switch (shape->GetType()) {
        case ShapeType::Sphere: {
            auto s = std::static_pointer_cast<SphereShape>(shape);
            return t.position + n * s->GetRadius();
        }
        case ShapeType::Box: {
            auto s = std::static_pointer_cast<BoxShape>(shape);
            Vector3 local = InverseRotate(t.rotation, n);
            const Vector3& h = s->GetHalfExtents();
            Vector3 p(local.x >= 0.0f ? h.x : -h.x,
                local.y >= 0.0f ? h.y : -h.y,
                local.z >= 0.0f ? h.z : -h.z);
            return t.TransformPoint(p);
        }
        case ShapeType::ConvexMesh: {
            auto s = std::static_pointer_cast<ConvexMeshShape>(shape);
            Vector3 local = InverseRotate(t.rotation, n);
            return t.TransformPoint(s->GetSupportPoint(local));
        }
        default:
            return t.position;
        }
    }

    static bool FillGjkContact(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold) {
        Vector3 normal, point;
        float pen = 0.0f;
        auto supportA = [bodyA](const Vector3& d) { return SupportWorld(bodyA, d); };
        auto supportB = [bodyB](const Vector3& d) { return SupportWorld(bodyB, d); };
        if (!GJK::TestAndGetContact(supportA, supportB, normal, pen, point)) {
            return false;
        }
        ContactPoint cp;
        cp.penetration = (std::min)(pen, 0.25f);
        cp.position = point;
        cp.featureId = 0;
        outManifold.normal = normal;
        outManifold.AddPoint(cp);
        return true;
    }

    static Vector3 SnapNormalToBox(RigidBody* boxBody, const Vector3& normal) {
        auto box = std::static_pointer_cast<BoxShape>(boxBody->GetShape());
        OBB obb(boxBody->GetPosition(), box->GetHalfExtents(), boxBody->GetRotation());
        int best = 0;
        float bestAbs = -1.0f;
        float sign = 1.0f;
        for (int i = 0; i < 3; ++i) {
            float d = obb.GetAxis(i).Dot(normal);
            if (std::abs(d) > bestAbs) {
                bestAbs = std::abs(d);
                best = i;
                sign = (d >= 0.0f) ? 1.0f : -1.0f;
            }
        }
        Vector3 snapped = obb.GetAxis(best) * sign;
        float len = snapped.Length();
        if (len > EPSILON) snapped = snapped * (1.0f / len);
        return snapped;
    }

    static bool FillConvexManifold(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold) {
        Vector3 normal, epaPoint;
        float epaPen = 0.0f;
        auto supportA = [bodyA](const Vector3& d) { return SupportWorld(bodyA, d); };
        auto supportB = [bodyB](const Vector3& d) { return SupportWorld(bodyB, d); };
        if (!GJK::TestAndGetContact(supportA, supportB, normal, epaPen, epaPoint)) {
            return false;
        }

        ShapeType tA = bodyA->GetShape()->GetType();
        ShapeType tB = bodyB->GetShape()->GetType();
        if (tA == ShapeType::Box) {
            Vector3 snapped = SnapNormalToBox(bodyA, normal);
            if (snapped.Dot(normal) > 0.5f) normal = snapped;
        } else if (tB == ShapeType::Box) {
            Vector3 snapped = SnapNormalToBox(bodyB, -normal);
            snapped = -snapped;
            if (snapped.Dot(normal) > 0.5f) normal = snapped;
        }

        Vector3 pts[32];
        float pens[32];
        uint32_t ids[32];
        int count = 0;

        constexpr float kKeepSkin = 0.02f;

        auto collectConvexVerts = [&](RigidBody* convexBody, const Vector3& planePoint, bool convexIsB, uint32_t idBase) {
            auto convex = std::static_pointer_cast<ConvexMeshShape>(convexBody->GetShape());
            const Transform& t = convexBody->GetTransform();
            const auto& vs = convex->GetVertices();
            for (uint32_t i = 0; i < vs.size() && count < 32; ++i) {
                Vector3 worldV = t.TransformPoint(vs[i]);
                float pen = convexIsB ? (planePoint - worldV).Dot(normal) : (worldV - planePoint).Dot(normal);
                if (pen >= -kKeepSkin) {
                    pts[count] = worldV;
                    pens[count] = (std::max)(pen, 0.0f);
                    ids[count] = idBase + i;
                    ++count;
                }
            }
        };

        if (tB == ShapeType::ConvexMesh) {
            collectConvexVerts(bodyB, SupportWorld(bodyA, normal), true, 0);
        }
        if (tA == ShapeType::ConvexMesh) {
            collectConvexVerts(bodyA, SupportWorld(bodyB, -normal), false, 0x10000u);
        }

        if (count == 0) {
            ContactPoint cp;
            cp.penetration = (std::min)((std::max)(epaPen, 0.0f), 0.25f);
            cp.position = epaPoint;
            cp.featureId = 0;
            outManifold.normal = normal;
            outManifold.AddPoint(cp);
            return true;
        }

        float maxKeep = (std::max)(epaPen * 2.0f, 0.08f);
        int filtered = 0;
        for (int i = 0; i < count; ++i) {
            if (pens[i] <= maxKeep) {
                pts[filtered] = pts[i];
                pens[filtered] = pens[i];
                ids[filtered] = ids[i];
                ++filtered;
            }
        }
        if (filtered == 0) {
            ContactPoint cp;
            cp.penetration = (std::min)((std::max)(epaPen, 0.0f), 0.25f);
            cp.position = epaPoint;
            cp.featureId = 0;
            outManifold.normal = normal;
            outManifold.AddPoint(cp);
            return true;
        }
        count = filtered;

        if (count > 4) {
            Vector3 reducePts[32];
            float reducePens[32];
            int origCount = count;
            for (int i = 0; i < count; ++i) {
                reducePts[i] = pts[i];
                reducePens[i] = pens[i];
            }
            ReduceManifoldPoints(reducePts, reducePens, count);
            uint32_t newIds[4];
            for (int i = 0; i < count; ++i) {
                uint32_t id = ids[0];
                float bestDist = std::numeric_limits<float>::max();
                for (int j = 0; j < origCount; ++j) {
                    float distSq = (reducePts[i] - pts[j]).LengthSquared();
                    if (distSq < bestDist) {
                        bestDist = distSq;
                        id = ids[j];
                    }
                }
                newIds[i] = id;
            }
            for (int i = 0; i < count; ++i) {
                pts[i] = reducePts[i];
                pens[i] = reducePens[i];
                ids[i] = newIds[i];
            }
        }

        outManifold.normal = normal;
        for (int i = 0; i < count; ++i) {
            ContactPoint cp;
            cp.position = pts[i];
            cp.penetration = pens[i];
            cp.featureId = ids[i];
            outManifold.AddPoint(cp);
        }
        return outManifold.pointCount > 0;
    }

    bool CollisionDispatch::ConvexMeshVsConvexMesh(RigidBody* bodyA, RigidBody* bodyB, ContactManifold& outManifold) {
        return FillConvexManifold(bodyA, bodyB, outManifold);
    }

    bool CollisionDispatch::SphereVsConvexMesh(RigidBody* bodySphere, RigidBody* bodyConvex, ContactManifold& outManifold) {
        return FillGjkContact(bodySphere, bodyConvex, outManifold);
    }

    bool CollisionDispatch::BoxVsConvexMesh(RigidBody* bodyBox, RigidBody* bodyConvex, ContactManifold& outManifold) {
        return FillConvexManifold(bodyBox, bodyConvex, outManifold);
    }

    bool CollisionDispatch::ConvexMeshVsTriangleMesh(RigidBody* bodyConvex, RigidBody* bodyMesh,
        ContactManifold& outManifold) {
        auto convex = std::static_pointer_cast<ConvexMeshShape>(bodyConvex->GetShape());
        auto mesh = std::static_pointer_cast<TriangleMeshShape>(bodyMesh->GetShape());

        Transform meshTransform = bodyMesh->GetTransform();
        Matrix3x3 meshRot = Matrix3x3::FromQuaternion(meshTransform.rotation);
        Matrix3x3 meshRotInv = meshRot.Transposed();
        Transform convexTransform = bodyConvex->GetTransform();

        AABB worldAABB = convex->ComputeAABB(convexTransform);
        Vector3 corners[8] = {
            { worldAABB.min.x, worldAABB.min.y, worldAABB.min.z },
            { worldAABB.max.x, worldAABB.min.y, worldAABB.min.z },
            { worldAABB.min.x, worldAABB.max.y, worldAABB.min.z },
            { worldAABB.max.x, worldAABB.max.y, worldAABB.min.z },
            { worldAABB.min.x, worldAABB.min.y, worldAABB.max.z },
            { worldAABB.max.x, worldAABB.min.y, worldAABB.max.z },
            { worldAABB.min.x, worldAABB.max.y, worldAABB.max.z },
            { worldAABB.max.x, worldAABB.max.y, worldAABB.max.z },
        };
        Vector3 localMin(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
        Vector3 localMax(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
        for (const auto& c : corners) {
            Vector3 local = meshRotInv * (c - meshTransform.position);
            localMin.x = (std::min)(localMin.x, local.x);
            localMin.y = (std::min)(localMin.y, local.y);
            localMin.z = (std::min)(localMin.z, local.z);
            localMax.x = (std::max)(localMax.x, local.x);
            localMax.y = (std::max)(localMax.y, local.y);
            localMax.z = (std::max)(localMax.z, local.z);
        }
        AABB queryAABB(localMin, localMax);

        auto supportConvexLocal = [&](const Vector3& localDir) {
            Vector3 worldDir = meshRot * localDir;
            Vector3 worldPt = SupportWorld(bodyConvex, worldDir);
            return meshRotInv * (worldPt - meshTransform.position);
        };

        const auto& triangles = mesh->GetTriangles();
        const auto& bvh = mesh->GetBVH();

        Vector3 bestNormal(0.0f, 1.0f, 0.0f);
        Vector3 bestPoint(0.0f, 0.0f, 0.0f);
        float bestPen = -1.0f;

        bvh.Query(queryAABB, [&](int triIdx) {
            const Triangle& tri = triangles[triIdx];
            auto supportTri = [&](const Vector3& dir) {
                float da = tri.a.Dot(dir);
                float db = tri.b.Dot(dir);
                float dc = tri.c.Dot(dir);
                if (da >= db && da >= dc) return tri.a;
                if (db >= dc) return tri.b;
                return tri.c;
            };

            Vector3 n, p;
            float pen = 0.0f;
            if (!GJK::TestAndGetContact(supportConvexLocal, supportTri, n, pen, p)) return;
            if (pen > bestPen) {
                bestPen = pen;
                bestNormal = n;
                bestPoint = p;
            }
            });

        if (bestPen < 0.0f) return false;

        Vector3 worldNormal = meshRot * bestNormal;
        float nLen = worldNormal.Length();
        if (nLen > EPSILON) worldNormal = worldNormal * (1.0f / nLen);
        else worldNormal = Vector3(0.0f, 1.0f, 0.0f);

        ContactPoint cp;
        cp.penetration = bestPen;
        cp.position = meshTransform.position + meshRot * bestPoint;
        cp.featureId = 0;
        outManifold.normal = worldNormal;
        outManifold.AddPoint(cp);
        return true;
    }

} // namespace SunvoltumPhysics