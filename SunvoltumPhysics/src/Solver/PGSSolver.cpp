#include "SunvoltumPhysics/Solver/PGSSolver.h"
#include <algorithm>
#include <cmath>

namespace SunvoltumPhysics {

    void PGSSolver::Solve(std::vector<ContactManifold>& manifolds, float dt, uint32_t iterations) {
        std::vector<std::shared_ptr<Joint>> emptyJoints;
        Solve(manifolds, emptyJoints, dt, iterations);
    }

    void PGSSolver::Solve(std::vector<ContactManifold>& manifolds, std::vector<std::shared_ptr<Joint>>& joints, float dt, uint32_t iterations) {
        if (manifolds.empty() && joints.empty()) return;

        PrepareConstraints(manifolds, dt);
        PrepareJointConstraints(joints, dt);

        WarmStart();
        WarmStartJoints();

        for (uint32_t it = 0; it < iterations; ++it) {
            SolveJointConstraints();
            SolveVelocityConstraints();
            SolveJointConstraints();
        }

        StoreImpulses();
        StoreJointImpulses(dt);
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

    void PGSSolver::PrepareJointConstraints(std::vector<std::shared_ptr<Joint>>& joints, float dt) {
        m_jointConstraints.clear();
        m_jointConstraints.reserve(joints.size());

        if (dt <= 0.0f) return;
        float invDt = 1.0f / dt;

        for (auto& jointPtr : joints) {
            if (!jointPtr || !jointPtr->IsEnabled()) continue;

            Joint* joint = jointPtr.get();
            RigidBody* bA = joint->GetBodyA();
            RigidBody* bB = joint->GetBodyB();
            if (!bA || !bB) continue;

            // If both bodies are static/kinematic, no constraint needed
            if (bA->GetType() != BodyType::Dynamic && bB->GetType() != BodyType::Dynamic) {
                continue;
            }

            SolverJointConstraint jc;
            jc.joint = joint;
            jc.bodyA = bA;
            jc.bodyB = bB;

            float mA = bA->GetInvMass();
            float mB = bB->GetInvMass();
            const Matrix3x3& iA = bA->GetInvInertiaWorld();
            const Matrix3x3& iB = bB->GetInvInertiaWorld();

            const Transform& tA = bA->GetTransform();
            const Transform& tB = bB->GetTransform();

            Vector3 worldPosA = tA.TransformPoint(joint->GetLocalFrameA().position);
            Vector3 worldPosB = tB.TransformPoint(joint->GetLocalFrameB().position);

            jc.rA = worldPosA - tA.position;
            jc.rB = worldPosB - tB.position;

            // K_linear = (mA + mB) * I - rA_skew * iA * rA_skew - rB_skew * iB * rB_skew
            Matrix3x3 rA_skew = Matrix3x3::SkewSymmetric(jc.rA);
            Matrix3x3 rB_skew = Matrix3x3::SkewSymmetric(jc.rB);

            Matrix3x3 kLinear;
            kLinear.m[0][0] = mA + mB;
            kLinear.m[1][1] = mA + mB;
            kLinear.m[2][2] = mA + mB;

            // Note: v.Cross(I * v.Cross(w)) = - skew(v) * I * skew(v) * w
            kLinear = kLinear - (rA_skew * iA * rA_skew) - (rB_skew * iB * rB_skew);
            jc.linearEffectiveMass = kLinear.Inversed();

            Vector3 linearPosError = worldPosB - worldPosA;
            // Balanced Baumgarte stabilization (0.25f) to avoid over-shooting jitter
            constexpr float JOINT_BAUMGARTE = 0.25f;
            constexpr float MAX_JOINT_BIAS = 5.0f; // clamp bias velocity to prevent explosive impulses

            auto ClampVec = [](const Vector3& v, float maxLen) {
                float lenSq = v.LengthSquared();
                if (lenSq > maxLen * maxLen && lenSq > EPSILON * EPSILON) {
                    return v * (maxLen / std::sqrt(lenSq));
                }
                return v;
            };

            jc.linearBias = ClampVec(linearPosError * (JOINT_BAUMGARTE * invDt), MAX_JOINT_BIAS);
            jc.accumulatedLinearImpulse = joint->GetLinearImpulse();

            if (joint->GetType() == JointType::Fixed) {
                // Fixed joint: lock relative orientation (3 DOF)
                Matrix3x3 kAngular = iA + iB;
                jc.angularEffectiveMass = kAngular.Inversed();

                // Target orientation for B: R_target = R_A * frameA.rot * frameB.rot^(-1)
                Quaternion qTarget = tA.rotation * joint->GetLocalFrameA().rotation * joint->GetLocalFrameB().rotation.Inversed();
                Quaternion qDiff = tB.rotation * qTarget.Inversed();
                if (qDiff.w < 0.0f) {
                    qDiff.x = -qDiff.x;
                    qDiff.y = -qDiff.y;
                    qDiff.z = -qDiff.z;
                    qDiff.w = -qDiff.w;
                }
                Vector3 angularError(qDiff.x * 2.0f, qDiff.y * 2.0f, qDiff.z * 2.0f);
                jc.angularBias = ClampVec(angularError * (JOINT_BAUMGARTE * invDt), MAX_JOINT_BIAS);
                jc.accumulatedAngularImpulse = joint->GetAngularImpulse();
            }
            else if (joint->GetType() == JointType::Motor) {
                MotorJoint* motor = static_cast<MotorJoint*>(joint);

                // Local frame A rotation in world space defines the 3 joint axes
                Quaternion qAWorld = tA.rotation * motor->GetLocalFrameA().rotation;
                jc.twistAxis  = qAWorld.Rotate(Vector3(1.0f, 0.0f, 0.0f)).Normalized();
                jc.swingAxis1 = qAWorld.Rotate(Vector3(0.0f, 1.0f, 0.0f)).Normalized();
                jc.swingAxis2 = qAWorld.Rotate(Vector3(0.0f, 0.0f, 1.0f)).Normalized();

                // Swing axes: lock angular motion around Y and Z
                // K_swing = axis . ((iA + iB) * axis)
                float kSwing1 = (iA * jc.swingAxis1 + iB * jc.swingAxis1).Dot(jc.swingAxis1);
                jc.swingEffectiveMass1 = kSwing1 > EPSILON ? 1.0f / kSwing1 : 0.0f;

                float kSwing2 = (iA * jc.swingAxis2 + iB * jc.swingAxis2).Dot(jc.swingAxis2);
                jc.swingEffectiveMass2 = kSwing2 > EPSILON ? 1.0f / kSwing2 : 0.0f;

                // IMPORTANT: Calculate swing error strictly from the tilt of the twist axis!
                // Do NOT use full orientation difference because rotation around twist axis
                // must be 100% free and must not contaminate swing1 / swing2!
                Quaternion qBWorld = tB.rotation * motor->GetLocalFrameB().rotation;
                Vector3 twistAxisB = qBWorld.Rotate(Vector3(1.0f, 0.0f, 0.0f)).Normalized();

                // swingError is the cross product between desired twist axis (jc.twistAxis) and actual (twistAxisB)
                Vector3 swingAngError = jc.twistAxis.Cross(twistAxisB);

                float b1 = swingAngError.Dot(jc.swingAxis1) * (JOINT_BAUMGARTE * invDt);
                float b2 = swingAngError.Dot(jc.swingAxis2) * (JOINT_BAUMGARTE * invDt);
                jc.swingBias1 = (std::max)(-MAX_JOINT_BIAS, (std::min)(MAX_JOINT_BIAS, b1));
                jc.swingBias2 = (std::max)(-MAX_JOINT_BIAS, (std::min)(MAX_JOINT_BIAS, b2));

                const Vector3& swImp = motor->GetSwingImpulse();
                jc.accumulatedSwingImpulse1 = swImp.x;
                jc.accumulatedSwingImpulse2 = swImp.y;

                // Twist axis (Motor drive)
                float kTwist = (iA * jc.twistAxis + iB * jc.twistAxis).Dot(jc.twistAxis);
                jc.twistEffectiveMass = kTwist > EPSILON ? 1.0f / kTwist : 0.0f;

                // Compute desired drive velocity
                float desiredAngle = motor->GetDesiredAngle();
                float currentAngle = motor->GetCurrentAngle();
                float maxVelocity = motor->GetMaxVelocity();
                float angleDiff = desiredAngle - currentAngle;

                float targetVel = 0.0f;
                constexpr float kEps = 1e-3f;
                if (std::abs(angleDiff) > kEps && maxVelocity > 0.0f) {
                    targetVel = (angleDiff > 0.0f ? 1.0f : -1.0f) * maxVelocity;
                }
                motor->SetCurrentVelocity(targetVel);
                jc.twistTargetVelocity = targetVel;
                jc.accumulatedTwistImpulse = motor->GetMotorImpulse();
            }

            m_jointConstraints.push_back(jc);
        }
    }

    void PGSSolver::WarmStartJoints() {
        for (auto& jc : m_jointConstraints) {
            RigidBody* bA = jc.bodyA;
            RigidBody* bB = jc.bodyB;

            // Apply accumulated linear impulse
            bA->ApplyImpulseAtWorldPoint(-jc.accumulatedLinearImpulse, bA->GetPosition() + jc.rA);
            bB->ApplyImpulseAtWorldPoint(jc.accumulatedLinearImpulse, bB->GetPosition() + jc.rB);

            if (jc.joint->GetType() == JointType::Fixed) {
                bA->ApplyAngularImpulse(-jc.accumulatedAngularImpulse);
                bB->ApplyAngularImpulse(jc.accumulatedAngularImpulse);
            }
            else if (jc.joint->GetType() == JointType::Motor) {
                Vector3 swingImpulse = jc.swingAxis1 * jc.accumulatedSwingImpulse1 +
                                       jc.swingAxis2 * jc.accumulatedSwingImpulse2;
                Vector3 twistImpulse = jc.twistAxis * jc.accumulatedTwistImpulse;
                Vector3 totalAngular = swingImpulse + twistImpulse;

                bA->ApplyAngularImpulse(-totalAngular);
                bB->ApplyAngularImpulse(totalAngular);
            }
        }
    }

    void PGSSolver::SolveJointConstraints() {
        for (auto& jc : m_jointConstraints) {
            RigidBody* bA = jc.bodyA;
            RigidBody* bB = jc.bodyB;

            // 1. Solve Linear Constraint (Point-to-Point)
            Vector3 vA = bA->GetLinearVelocity() + bA->GetAngularVelocity().Cross(jc.rA);
            Vector3 vB = bB->GetLinearVelocity() + bB->GetAngularVelocity().Cross(jc.rB);
            Vector3 cDotLinear = (vB - vA) + jc.linearBias;

            Vector3 dImpulseLinear = jc.linearEffectiveMass * (-cDotLinear);
            jc.accumulatedLinearImpulse += dImpulseLinear;

            bA->ApplyImpulseAtWorldPoint(-dImpulseLinear, bA->GetPosition() + jc.rA);
            bB->ApplyImpulseAtWorldPoint(dImpulseLinear, bB->GetPosition() + jc.rB);

            // 2. Solve Angular Constraint
            if (jc.joint->GetType() == JointType::Fixed) {
                Vector3 wA = bA->GetAngularVelocity();
                Vector3 wB = bB->GetAngularVelocity();
                Vector3 cDotAngular = (wB - wA) + jc.angularBias;

                Vector3 dImpulseAngular = jc.angularEffectiveMass * (-cDotAngular);
                jc.accumulatedAngularImpulse += dImpulseAngular;

                bA->ApplyAngularImpulse(-dImpulseAngular);
                bB->ApplyAngularImpulse(dImpulseAngular);
            }
            else if (jc.joint->GetType() == JointType::Motor) {
                Vector3 wA = bA->GetAngularVelocity();
                Vector3 wB = bB->GetAngularVelocity();
                Vector3 dw = wB - wA;

                // Swing 1
                float cDotSwing1 = dw.Dot(jc.swingAxis1) + jc.swingBias1;
                float dImpulseSwing1 = jc.swingEffectiveMass1 * (-cDotSwing1);
                jc.accumulatedSwingImpulse1 += dImpulseSwing1;
                Vector3 pSwing1 = jc.swingAxis1 * dImpulseSwing1;
                bA->ApplyAngularImpulse(-pSwing1);
                bB->ApplyAngularImpulse(pSwing1);

                // Swing 2
                wA = bA->GetAngularVelocity();
                wB = bB->GetAngularVelocity();
                dw = wB - wA;

                float cDotSwing2 = dw.Dot(jc.swingAxis2) + jc.swingBias2;
                float dImpulseSwing2 = jc.swingEffectiveMass2 * (-cDotSwing2);
                jc.accumulatedSwingImpulse2 += dImpulseSwing2;
                Vector3 pSwing2 = jc.swingAxis2 * dImpulseSwing2;
                bA->ApplyAngularImpulse(-pSwing2);
                bB->ApplyAngularImpulse(pSwing2);

                // Twist Motor Drive
                wA = bA->GetAngularVelocity();
                wB = bB->GetAngularVelocity();
                dw = wB - wA;

                float currentTwistSpeed = dw.Dot(jc.twistAxis);
                float speedError = currentTwistSpeed - jc.twistTargetVelocity;
                float dImpulseTwist = jc.twistEffectiveMass * (-speedError);
                jc.accumulatedTwistImpulse += dImpulseTwist;

                Vector3 pTwist = jc.twistAxis * dImpulseTwist;
                bA->ApplyAngularImpulse(-pTwist);
                bB->ApplyAngularImpulse(pTwist);
            }
        }
    }

    void PGSSolver::StoreJointImpulses(float dt) {
        for (auto& jc : m_jointConstraints) {
            Joint* joint = jc.joint;
            if (!joint) continue;

            joint->SetLinearImpulse(jc.accumulatedLinearImpulse);

            if (joint->GetType() == JointType::Fixed) {
                joint->SetAngularImpulse(jc.accumulatedAngularImpulse);
            }
            else if (joint->GetType() == JointType::Motor) {
                MotorJoint* motor = static_cast<MotorJoint*>(joint);
                motor->SetSwingImpulse(Vector3(jc.accumulatedSwingImpulse1, jc.accumulatedSwingImpulse2, 0.0f));
                motor->SetMotorImpulse(jc.accumulatedTwistImpulse);

                // Integrate CurrentAngle from relative angular velocity around twist axis
                RigidBody* bA = jc.bodyA;
                RigidBody* bB = jc.bodyB;
                float relTwistVel = (bB->GetAngularVelocity() - bA->GetAngularVelocity()).Dot(jc.twistAxis);
                float curAngle = motor->GetCurrentAngle() + relTwistVel * dt;
                motor->SetCurrentAngle(curAngle);
            }
        }
    }

} // namespace SunvoltumPhysics
