#pragma once

#include "SunvoltumPhysics/Dynamics/RigidBody.h"
#include <memory>
#include <algorithm>

namespace SunvoltumPhysics {

    enum class JointType {
        Fixed,
        Motor
    };

    class Joint {
    public:
        Joint(JointType type, RigidBody* bodyA, RigidBody* bodyB,
              const Transform& localFrameA = Transform(),
              const Transform& localFrameB = Transform())
            : m_type(type), m_bodyA(bodyA), m_bodyB(bodyB)
            , m_localFrameA(localFrameA), m_localFrameB(localFrameB) {}

        virtual ~Joint() = default;

        JointType GetType() const { return m_type; }
        RigidBody* GetBodyA() const { return m_bodyA; }
        RigidBody* GetBodyB() const { return m_bodyB; }

        const Transform& GetLocalFrameA() const { return m_localFrameA; }
        void SetLocalFrameA(const Transform& t) { m_localFrameA = t; }

        const Transform& GetLocalFrameB() const { return m_localFrameB; }
        void SetLocalFrameB(const Transform& t) { m_localFrameB = t; }

        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool enabled) { m_enabled = enabled; }

        Vector3 GetLinearImpulse() const { return m_linearImpulse; }
        void SetLinearImpulse(const Vector3& imp) { m_linearImpulse = imp; }

        Vector3 GetAngularImpulse() const { return m_angularImpulse; }
        void SetAngularImpulse(const Vector3& imp) { m_angularImpulse = imp; }

    protected:
        JointType m_type;
        RigidBody* m_bodyA{ nullptr };
        RigidBody* m_bodyB{ nullptr };
        Transform m_localFrameA;
        Transform m_localFrameB;
        bool m_enabled{ true };

        Vector3 m_linearImpulse{ 0.0f, 0.0f, 0.0f };
        Vector3 m_angularImpulse{ 0.0f, 0.0f, 0.0f };
    };

    class FixedJoint : public Joint {
    public:
        FixedJoint(RigidBody* bodyA, RigidBody* bodyB,
                   const Transform& localFrameA = Transform(),
                   const Transform& localFrameB = Transform())
            : Joint(JointType::Fixed, bodyA, bodyB, localFrameA, localFrameB) {}
    };

    class MotorJoint : public Joint {
    public:
        MotorJoint(RigidBody* bodyA, RigidBody* bodyB,
                   const Transform& localFrameA = Transform(),
                   const Transform& localFrameB = Transform())
            : Joint(JointType::Motor, bodyA, bodyB, localFrameA, localFrameB) {}

        float GetCurrentAngle() const { return m_currentAngle; }
        void SetCurrentAngle(float a) { m_currentAngle = a; }

        float GetDesiredAngle() const { return m_desiredAngle; }
        void SetDesiredAngle(float a) { m_desiredAngle = a; }

        float GetMaxVelocity() const { return m_maxVelocity; }
        void SetMaxVelocity(float v) { m_maxVelocity = (std::max)(0.0f, v); }

        float GetCurrentVelocity() const { return m_currentVelocity; }
        void SetCurrentVelocity(float v) { m_currentVelocity = v; }

        float GetMotorImpulse() const { return m_motorImpulse; }
        void SetMotorImpulse(float imp) { m_motorImpulse = imp; }

        const Vector3& GetSwingImpulse() const { return m_swingImpulse; }
        void SetSwingImpulse(const Vector3& imp) { m_swingImpulse = imp; }

    private:
        float m_currentAngle{ 0.0f };
        float m_desiredAngle{ 0.0f };
        float m_maxVelocity{ 3.0f };
        float m_currentVelocity{ 0.0f };
        float m_motorImpulse{ 0.0f };
        Vector3 m_swingImpulse{ 0.0f, 0.0f, 0.0f };
    };

} // namespace SunvoltumPhysics
