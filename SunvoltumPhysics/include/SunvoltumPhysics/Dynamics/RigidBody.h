#pragma once

#include "SunvoltumPhysics/Common/Math.h"
#include "SunvoltumPhysics/Collision/Shapes/Shape.h"
#include "SunvoltumPhysics/Dynamics/Material.h"
#include <memory>

namespace SunvoltumPhysics {

    enum class BodyType {
        Static,
        Dynamic,
        Kinematic
    };

    class RigidBody {
    public:
        RigidBody(BodyType type, std::shared_ptr<Shape> shape, const Transform& transform = Transform(), std::shared_ptr<Material> material = nullptr)
            : m_type(type), m_shape(std::move(shape)), m_transform(transform), m_material(std::move(material)) {
            if (!m_material) {
                m_material = std::make_shared<Material>();
            }
            UpdateInertiaTensor();
            UpdateAABB();
        }

        BodyType GetType() const { return m_type; }
        void SetType(BodyType type) {
            m_type = type;
            UpdateInertiaTensor();
        }

        std::shared_ptr<Shape> GetShape() const { return m_shape; }
        void SetShape(std::shared_ptr<Shape> shape) {
            m_shape = std::move(shape);
            UpdateInertiaTensor();
            UpdateAABB();
        }

        std::shared_ptr<Material> GetMaterial() const { return m_material; }
        void SetMaterial(std::shared_ptr<Material> material) {
            m_material = std::move(material);
            if (!m_material) {
                m_material = std::make_shared<Material>();
            }
        }

        const Transform& GetTransform() const { return m_transform; }
        void SetTransform(const Transform& transform) {
            m_transform = transform;
            UpdateAABB();
            UpdateInertiaWorld();
        }

        const Vector3& GetPosition() const { return m_transform.position; }
        void SetPosition(const Vector3& pos) {
            m_transform.position = pos;
            UpdateAABB();
        }

        const Quaternion& GetRotation() const { return m_transform.rotation; }
        void SetRotation(const Quaternion& rot) {
            m_transform.rotation = rot;
            UpdateAABB();
            UpdateInertiaWorld();
        }

        const Vector3& GetLinearVelocity() const { return m_linearVelocity; }
        void SetLinearVelocity(const Vector3& v) { m_linearVelocity = v; }

        const Vector3& GetAngularVelocity() const { return m_angularVelocity; }
        void SetAngularVelocity(const Vector3& w) { m_angularVelocity = w; }

        float GetMass() const { return m_mass; }
        float GetInvMass() const { return m_type == BodyType::Dynamic ? m_invMass : 0.0f; }

        void SetMass(float mass) {
            m_mass = mass;
            m_invMass = (mass > EPSILON) ? (1.0f / mass) : 0.0f;
            UpdateInertiaTensor();
        }

        const Matrix3x3& GetInvInertiaWorld() const {
            if (m_type != BodyType::Dynamic) {
                // Default Matrix3x3 is identity; static/kinematic bodies must
                // contribute zero angular mass or contacts far from the COM
                // get a huge kNormal and almost no supporting impulse.
                static const Matrix3x3 zeroM = []() {
                    Matrix3x3 m;
                    m.m[0][0] = 0.0f;
                    m.m[1][1] = 0.0f;
                    m.m[2][2] = 0.0f;
                    return m;
                }();
                return zeroM;
            }
            return m_invInertiaWorld;
        }

        float GetFriction() const { return m_material ? m_material->GetDynamicFriction() : 0.4f; }
        void SetFriction(float friction) {
            if (m_material) {
                m_material->SetDynamicFriction(friction);
                m_material->SetStaticFriction(friction * 1.25f);
            }
        }

        float GetRestitution() const { return m_material ? m_material->GetRestitution() : 0.2f; }
        void SetRestitution(float restitution) {
            if (m_material) {
                m_material->SetRestitution(restitution);
            }
        }

        // PhysX-совместимые лимиты скорости и депенетрации
        float GetMaxLinearVelocity() const { return m_maxLinearVelocity; }
        void SetMaxLinearVelocity(float v) { m_maxLinearVelocity = v; }

        float GetMaxAngularVelocity() const { return m_maxAngularVelocity; }
        void SetMaxAngularVelocity(float w) { m_maxAngularVelocity = w; }

        float GetMaxDepenetrationVelocity() const { return m_maxDepenetrationVelocity; }
        void SetMaxDepenetrationVelocity(float v) { m_maxDepenetrationVelocity = v; }

        // Блокировка вращения вокруг осей (аналог PhysX PxRigidDynamicLockFlag, нужен для Humanoid LockUpright)
        void SetLockAngular(bool lockX, bool lockY, bool lockZ) {
            m_lockAngularX = lockX;
            m_lockAngularY = lockY;
            m_lockAngularZ = lockZ;
            if (lockX) m_angularVelocity.x = 0.0f;
            if (lockY) m_angularVelocity.y = 0.0f;
            if (lockZ) m_angularVelocity.z = 0.0f;
            UpdateInertiaWorld();
        }

        bool IsAngularLockedX() const { return m_lockAngularX; }
        bool IsAngularLockedY() const { return m_lockAngularY; }
        bool IsAngularLockedZ() const { return m_lockAngularZ; }

        const AABB& GetAABB() const { return m_aabb; }

        void ApplyForce(const Vector3& force) {
            if (m_type == BodyType::Dynamic) {
                m_forceAccumulator += force;
            }
        }

        void ApplyTorque(const Vector3& torque) {
            if (m_type == BodyType::Dynamic) {
                m_torqueAccumulator += torque;
            }
        }

        void ApplyForceAtWorldPoint(const Vector3& force, const Vector3& point) {
            if (m_type == BodyType::Dynamic) {
                m_forceAccumulator += force;
                m_torqueAccumulator += (point - m_transform.position).Cross(force);
            }
        }

        void ApplyLinearImpulse(const Vector3& impulse) {
            if (m_type == BodyType::Dynamic) {
                m_linearVelocity += impulse * m_invMass;
            }
        }

        void ApplyAngularImpulse(const Vector3& impulse) {
            if (m_type == BodyType::Dynamic) {
                m_angularVelocity += m_invInertiaWorld * impulse;
            }
        }

        void ApplyImpulseAtWorldPoint(const Vector3& impulse, const Vector3& point) {
            if (m_type == BodyType::Dynamic) {
                m_linearVelocity += impulse * m_invMass;
                m_angularVelocity += m_invInertiaWorld * ((point - m_transform.position).Cross(impulse));
            }
        }

        float GetLinearDamping() const { return m_linearDamping; }
        void SetLinearDamping(float damping) { m_linearDamping = damping; }

        float GetAngularDamping() const { return m_angularDamping; }
        void SetAngularDamping(float damping) { m_angularDamping = damping; }

        void IntegrateVelocities(float dt, const Vector3& gravity) {
            if (m_type != BodyType::Dynamic) return;

            // v += (gravity + F / m) * dt
            Vector3 linearAccel = gravity + m_forceAccumulator * m_invMass;
            m_linearVelocity += linearAccel * dt;

            // w += (I^-1 * torque) * dt
            Vector3 angularAccel = m_invInertiaWorld * m_torqueAccumulator;
            m_angularVelocity += angularAccel * dt;

            // Zero out locked angular axes
            if (m_lockAngularX) m_angularVelocity.x = 0.0f;
            if (m_lockAngularY) m_angularVelocity.y = 0.0f;
            if (m_lockAngularZ) m_angularVelocity.z = 0.0f;

            // Damping (drag and rolling resistance for stability)
            m_linearVelocity *= std::clamp(1.0f - m_linearDamping * dt, 0.0f, 1.0f);
            m_angularVelocity *= std::clamp(1.0f - m_angularDamping * dt, 0.0f, 1.0f);

            // Ограничения скорости как в PxRigidDynamic (maxLinearVelocity, maxAngularVelocity)
            float speedSq = m_linearVelocity.LengthSquared();
            if (speedSq > m_maxLinearVelocity * m_maxLinearVelocity && m_maxLinearVelocity > 0.0f) {
                m_linearVelocity = m_linearVelocity * (m_maxLinearVelocity / std::sqrt(speedSq));
            }

            float angSpeedSq = m_angularVelocity.LengthSquared();
            if (angSpeedSq > m_maxAngularVelocity * m_maxAngularVelocity && m_maxAngularVelocity > 0.0f) {
                m_angularVelocity = m_angularVelocity * (m_maxAngularVelocity / std::sqrt(angSpeedSq));
            }
        }

        void IntegratePositions(float dt) {
            if (m_type == BodyType::Static) return;

            // x += v * dt
            m_transform.position += m_linearVelocity * dt;

            if (m_lockAngularX) m_angularVelocity.x = 0.0f;
            if (m_lockAngularY) m_angularVelocity.y = 0.0f;
            if (m_lockAngularZ) m_angularVelocity.z = 0.0f;

            // Exact rotation for constant world-space ω: q = Δq(ω dt) * q
            // (first-order q += 0.5 ω q dt under-rotates and shears the orientation)
            float omegaLen = m_angularVelocity.Length();
            if (omegaLen > EPSILON) {
                Quaternion dq = Quaternion::FromAxisAngle(m_angularVelocity * (1.0f / omegaLen), omegaLen * dt);
                m_transform.rotation = (dq * m_transform.rotation).Normalized();
            }

            UpdateAABB();
            UpdateInertiaWorld();
        }

        void ClearAccumulators() {
            m_forceAccumulator = Vector3::Zero;
            m_torqueAccumulator = Vector3::Zero;
        }

    private:
        void UpdateInertiaTensor() {
            if (m_type != BodyType::Dynamic || !m_shape || m_mass <= EPSILON) {
                m_invInertiaLocal = Matrix3x3();
                m_invInertiaWorld = Matrix3x3();
                return;
            }

            Matrix3x3 inertia = m_shape->ComputeInertia(m_mass);
            // Invert diagonal/symmetric inertia
            m_invInertiaLocal = Matrix3x3();
            if (std::abs(inertia.m[0][0]) > EPSILON) m_invInertiaLocal.m[0][0] = 1.0f / inertia.m[0][0];
            if (std::abs(inertia.m[1][1]) > EPSILON) m_invInertiaLocal.m[1][1] = 1.0f / inertia.m[1][1];
            if (std::abs(inertia.m[2][2]) > EPSILON) m_invInertiaLocal.m[2][2] = 1.0f / inertia.m[2][2];

            UpdateInertiaWorld();
        }

        void UpdateInertiaWorld() {
            if (m_type != BodyType::Dynamic) return;
            Matrix3x3 R = Matrix3x3::FromQuaternion(m_transform.rotation);
            // I_world^-1 = R * I_local^-1 * R^T
            m_invInertiaWorld = R * m_invInertiaLocal * R.Transposed();

            // Zero out locked angular degrees of freedom in inverse inertia
            if (m_lockAngularX) {
                m_invInertiaWorld.m[0][0] = 0.0f; m_invInertiaWorld.m[0][1] = 0.0f; m_invInertiaWorld.m[0][2] = 0.0f;
                m_invInertiaWorld.m[1][0] = 0.0f; m_invInertiaWorld.m[2][0] = 0.0f;
            }
            if (m_lockAngularY) {
                m_invInertiaWorld.m[1][0] = 0.0f; m_invInertiaWorld.m[1][1] = 0.0f; m_invInertiaWorld.m[1][2] = 0.0f;
                m_invInertiaWorld.m[0][1] = 0.0f; m_invInertiaWorld.m[2][1] = 0.0f;
            }
            if (m_lockAngularZ) {
                m_invInertiaWorld.m[2][0] = 0.0f; m_invInertiaWorld.m[2][1] = 0.0f; m_invInertiaWorld.m[2][2] = 0.0f;
                m_invInertiaWorld.m[0][2] = 0.0f; m_invInertiaWorld.m[1][2] = 0.0f;
            }
        }

        void UpdateAABB() {
            if (m_shape) {
                m_aabb = m_shape->ComputeAABB(m_transform);
            }
        }

    private:
        BodyType m_type{ BodyType::Dynamic };
        std::shared_ptr<Shape> m_shape;
        Transform m_transform;

        Vector3 m_linearVelocity{ 0.0f, 0.0f, 0.0f };
        Vector3 m_angularVelocity{ 0.0f, 0.0f, 0.0f };

        Vector3 m_forceAccumulator{ 0.0f, 0.0f, 0.0f };
        Vector3 m_torqueAccumulator{ 0.0f, 0.0f, 0.0f };

        float m_mass{ 1.0f };
        float m_invMass{ 1.0f };

        Matrix3x3 m_invInertiaLocal;
        Matrix3x3 m_invInertiaWorld;

        std::shared_ptr<Material> m_material;

        float m_linearDamping{ 0.05f };
        float m_angularDamping{ 0.15f };

        float m_maxLinearVelocity{ 1000.0f };
        float m_maxAngularVelocity{ 100.0f };
        float m_maxDepenetrationVelocity{ 50.0f };

        bool m_lockAngularX{ false };
        bool m_lockAngularY{ false };
        bool m_lockAngularZ{ false };

        AABB m_aabb;
    };

} // namespace SunvoltumPhysics
