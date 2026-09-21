#include "SunvoltumPhysics/Solver/PGSSolver.h"
#include <algorithm>
#include <cmath>

namespace SunvoltumPhysics {

    void PGSSolver::Solve(std::vector<ContactManifold>& manifolds, float dt, uint32_t iterations) {
        if (manifolds.empty()) return;

        PrepareConstraints(manifolds, dt);
        WarmStart();

        for (uint32_t it = 0; it < iterations; ++it) {
            SolveVelocityConstraints();
        }

        StoreImpulses();
    }

    void PGSSolver::PrepareConstraints(std::vector<ContactManifold>& manifolds, float dt) {
        m_constraints.clear();
        m_constraints.reserve(manifolds.size());

        float invDt = dt > 0.0f ? 1.0f / dt : 0.0f;

        for (auto& manifold : manifolds) {
            if (manifold.pointCount == 0) continue;

            RigidBody* bA = manifold.bodyA;
            RigidBody* bB = manifold.bodyB;

            SolverConstraint sc;
            sc.bodyA = bA;
            sc.bodyB = bB;
            sc.normal = manifold.normal;
            sc.friction = manifold.friction;
            sc.pointCount = manifold.pointCount;
            sc.sourceManifold = &manifold;

            Vector3 u(1.0f, 0.0f, 0.0f);
            if (std::abs(sc.normal.x) > 0.9f) {
                u = Vector3(0.0f, 1.0f, 0.0f);
            }
            sc.tangent1 = sc.normal.Cross(u).Normalized();
            sc.tangent2 = sc.normal.Cross(sc.tangent1).Normalized();

            float mA = bA->GetInvMass();
            float mB = bB->GetInvMass();
            const Matrix3x3& iA = bA->GetInvInertiaWorld();
            const Matrix3x3& iB = bB->GetInvInertiaWorld();

            for (int i = 0; i < manifold.pointCount; ++i) {
                const auto& cp = manifold.points[i];
                auto& scp = sc.points[i];

                scp.rA = cp.position - bA->GetPosition();
                scp.rB = cp.position - bB->GetPosition();
                scp.featureId = cp.featureId;
                scp.normalImpulse = cp.normalImpulse;
                scp.tangentImpulse1 = cp.tangentImpulse1;
                scp.tangentImpulse2 = cp.tangentImpulse2;

                Vector3 rnA = scp.rA.Cross(sc.normal);
                Vector3 rnB = scp.rB.Cross(sc.normal);
                float kNormal = mA + mB + (iA * rnA).Cross(scp.rA).Dot(sc.normal) + (iB * rnB).Cross(scp.rB).Dot(sc.normal);
                scp.normalMass = kNormal > EPSILON ? 1.0f / kNormal : 0.0f;

                Vector3 rt1A = scp.rA.Cross(sc.tangent1);
                Vector3 rt1B = scp.rB.Cross(sc.tangent1);
                float kTangent1 = mA + mB + (iA * rt1A).Cross(scp.rA).Dot(sc.tangent1) + (iB * rt1B).Cross(scp.rB).Dot(sc.tangent1);
                scp.tangentMass1 = kTangent1 > EPSILON ? 1.0f / kTangent1 : 0.0f;

                Vector3 rt2A = scp.rA.Cross(sc.tangent2);
                Vector3 rt2B = scp.rB.Cross(sc.tangent2);
                float kTangent2 = mA + mB + (iA * rt2A).Cross(scp.rA).Dot(sc.tangent2) + (iB * rt2B).Cross(scp.rB).Dot(sc.tangent2);
                scp.tangentMass2 = kTangent2 > EPSILON ? 1.0f / kTangent2 : 0.0f;

                float penetrationError = (std::max)(0.0f, cp.penetration - Settings::PENETRATION_SLOP);
                scp.bias = (Settings::BAUMGARTE_FACTOR * invDt) * penetrationError;

                float maxDepen = (std::min)(bA->GetMaxDepenetrationVelocity(), bB->GetMaxDepenetrationVelocity());
                float maxBias = (std::min)(Settings::MAX_PENETRATION_CORRECTION, maxDepen);
                scp.bias = (std::min)(scp.bias, maxBias);

                Vector3 vA = bA->GetLinearVelocity() + bA->GetAngularVelocity().Cross(scp.rA);
                Vector3 vB = bB->GetLinearVelocity() + bB->GetAngularVelocity().Cross(scp.rB);
                float vn = (vB - vA).Dot(sc.normal);

                if (vn < -1.5f) {
                    scp.bias += -manifold.restitution * vn;
                }
            }

            m_constraints.push_back(sc);
        }
    }

    void PGSSolver::WarmStart() {
        for (auto& sc : m_constraints) {
            RigidBody* bA = sc.bodyA;
            RigidBody* bB = sc.bodyB;

            for (int i = 0; i < sc.pointCount; ++i) {
                auto& scp = sc.points[i];
                Vector3 P = sc.normal * scp.normalImpulse +
                            sc.tangent1 * scp.tangentImpulse1 +
                            sc.tangent2 * scp.tangentImpulse2;

                bA->ApplyImpulseAtWorldPoint(-P, bA->GetPosition() + scp.rA);
                bB->ApplyImpulseAtWorldPoint(P, bB->GetPosition() + scp.rB);
            }
        }
    }

    void PGSSolver::SolveVelocityConstraints() {
        for (auto& sc : m_constraints) {
            RigidBody* bA = sc.bodyA;
            RigidBody* bB = sc.bodyB;

            for (int i = 0; i < sc.pointCount; ++i) {
                auto& scp = sc.points[i];

                Vector3 vA = bA->GetLinearVelocity() + bA->GetAngularVelocity().Cross(scp.rA);
                Vector3 vB = bB->GetLinearVelocity() + bB->GetAngularVelocity().Cross(scp.rB);
                Vector3 dv = vB - vA;

                // Coulomb cone: clamp the two tangent impulses together so friction
                // cannot invent a twist around the contact normal.
                float vt1 = dv.Dot(sc.tangent1);
                float vt2 = dv.Dot(sc.tangent2);
                float dLambdaT1 = scp.tangentMass1 * (-vt1);
                float dLambdaT2 = scp.tangentMass2 * (-vt2);
                float newLambdaT1 = scp.tangentImpulse1 + dLambdaT1;
                float newLambdaT2 = scp.tangentImpulse2 + dLambdaT2;
                float maxFriction = sc.friction * scp.normalImpulse;
                float tauSq = newLambdaT1 * newLambdaT1 + newLambdaT2 * newLambdaT2;
                if (tauSq > maxFriction * maxFriction && tauSq > EPSILON * EPSILON) {
                    float scale = maxFriction / std::sqrt(tauSq);
                    newLambdaT1 *= scale;
                    newLambdaT2 *= scale;
                }
                dLambdaT1 = newLambdaT1 - scp.tangentImpulse1;
                dLambdaT2 = newLambdaT2 - scp.tangentImpulse2;
                scp.tangentImpulse1 = newLambdaT1;
                scp.tangentImpulse2 = newLambdaT2;

                Vector3 PT = sc.tangent1 * dLambdaT1 + sc.tangent2 * dLambdaT2;
                bA->ApplyImpulseAtWorldPoint(-PT, bA->GetPosition() + scp.rA);
                bB->ApplyImpulseAtWorldPoint(PT, bB->GetPosition() + scp.rB);

                vA = bA->GetLinearVelocity() + bA->GetAngularVelocity().Cross(scp.rA);
                vB = bB->GetLinearVelocity() + bB->GetAngularVelocity().Cross(scp.rB);
                dv = vB - vA;

                float vn = dv.Dot(sc.normal);
                float dLambdaN = scp.normalMass * (-(vn - scp.bias));
                float newLambdaN = (std::max)(0.0f, scp.normalImpulse + dLambdaN);
                dLambdaN = newLambdaN - scp.normalImpulse;
                scp.normalImpulse = newLambdaN;
                    
                Vector3 PN = sc.normal * dLambdaN;
                bA->ApplyImpulseAtWorldPoint(-PN, bA->GetPosition() + scp.rA);
                bB->ApplyImpulseAtWorldPoint(PN, bB->GetPosition() + scp.rB);
            }
        }
    }

    void PGSSolver::StoreImpulses() {
        for (auto& sc : m_constraints) {
            if (!sc.sourceManifold) continue;
            for (int i = 0; i < sc.pointCount; ++i) {
                sc.sourceManifold->points[i].normalImpulse = sc.points[i].normalImpulse;
                sc.sourceManifold->points[i].tangentImpulse1 = sc.points[i].tangentImpulse1;
                sc.sourceManifold->points[i].tangentImpulse2 = sc.points[i].tangentImpulse2;
            }
        }
    }

} // namespace SunvoltumPhysics
