#pragma once

#include "PhysicsCommon.h"
#include "../Types/CFrame.h"
#include "../Types/Vector3.h"
#include "../Types/Shape.h"
#include <memory>

namespace Sunvoltum {

    class PhysicsWorld;

    class PhysicsBody
    {
    public:
        PhysicsBody()  = default;
        ~PhysicsBody() = default;

        PhysicsBody(const PhysicsBody&)            = delete;
        PhysicsBody& operator=(const PhysicsBody&) = delete;

        PhysicsBody(PhysicsBody&&) noexcept;
        PhysicsBody& operator=(PhysicsBody&&) noexcept;

        bool Init(
            PhysicsWorld*  world,
            const CFrame&  cf,
            const Vector3& size,
            Shape          shape,
            bool           anchored,
            bool           canCollide = true,
            bool           kinematic  = false
        );

        void Shutdown(PhysicsWorld* world);

        bool IsInitialized() const { return m_initialized; }
        bool IsAnchored()    const { return m_anchored;    }

        void SetCanCollide(bool canCollide);
        void SetAnchored(bool anchored, PhysicsWorld* world);
        void LockUpright();

        void Resize(const Vector3& newSize);
        void Reshape(Shape newShape);

        std::shared_ptr<SunvoltumPhysics::RigidBody> GetRigidBody() const { return m_body; }

        CFrame  GetCFrame()       const;
        Vector3 GetLinearVelocity()  const;
        Vector3 GetAngularVelocity() const;

        void SetCFrame(const CFrame& cf);
        void SetLinearVelocity(const Vector3& v);
        void SetAngularVelocity(const Vector3& v);

        bool IsKinematic() const { return m_kinematic; }
        void SetKinematic(bool kinematic);
        void SetKinematicTarget(const CFrame& cf);
        void WakeUp();

    private:
        static std::shared_ptr<SunvoltumPhysics::Shape> CreateShape(Shape shape, const Vector3& size);

        std::shared_ptr<SunvoltumPhysics::RigidBody> m_body;
        bool    m_anchored    = false;
        bool    m_kinematic   = false;
        bool    m_canCollide  = true;
        bool    m_initialized = false;
        Vector3 m_size        = { 1.0f, 1.0f, 1.0f };
        Shape   m_shape       = Shape::Block;
    };

} // namespace Sunvoltum
