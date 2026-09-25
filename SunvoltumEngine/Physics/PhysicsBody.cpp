#include "PhysicsBody.h"
#include "PhysicsWorld.h"
#include <algorithm>
#include <iostream>

namespace Sunvoltum {

    static constexpr float PLASTIC_FRICTION    = 0.40f;
    static constexpr float PLASTIC_RESTITUTION = 0.20f;

    std::shared_ptr<SunvoltumPhysics::Shape> PhysicsBody::CreateShape(Shape shape, const Vector3& size)
    {
        switch (shape)
        {
            case Shape::Ball:
            {
                float radius = std::min({ size.X, size.Y, size.Z }) * 0.5f;
                if (radius <= 0.0f) radius = 0.5f;
                return std::make_shared<SunvoltumPhysics::SphereShape>(radius);
            }
            case Shape::Cylinder:
            case Shape::Block:
            default:
            {
                float hx = std::max(size.X * 0.5f, 0.001f);
                float hy = std::max(size.Y * 0.5f, 0.001f);
                float hz = std::max(size.Z * 0.5f, 0.001f);
                return std::make_shared<SunvoltumPhysics::BoxShape>(SunvoltumPhysics::Vector3(hx, hy, hz));
            }
        }
    }

    PhysicsBody::PhysicsBody(PhysicsBody&& o) noexcept
        : m_body(std::move(o.m_body))
        , m_anchored(o.m_anchored)
        , m_kinematic(o.m_kinematic)
        , m_canCollide(o.m_canCollide)
        , m_initialized(o.m_initialized)
        , m_size(o.m_size)
        , m_shape(o.m_shape)
    {
        o.m_initialized = false;
    }

    PhysicsBody& PhysicsBody::operator=(PhysicsBody&& o) noexcept
    {
        if (this != &o)
        {
            m_body        = std::move(o.m_body);
            m_anchored    = o.m_anchored;
            m_kinematic   = o.m_kinematic;
            m_canCollide  = o.m_canCollide;
            m_initialized = o.m_initialized;
            m_size        = o.m_size;
            m_shape       = o.m_shape;
            o.m_initialized = false;
        }
        return *this;
    }

    bool PhysicsBody::Init(
        PhysicsWorld*  world,
        const CFrame&  cf,
        const Vector3& size,
        Shape          shape,
        bool           anchored,
        bool           canCollide,
        bool           kinematic)
    {
        if (m_initialized) return true;
        if (!world) return false;

        m_anchored   = anchored;
        m_kinematic  = kinematic && !anchored;
        m_canCollide = canCollide;
        m_size       = size;
        m_shape      = shape;

        auto pShape = CreateShape(shape, size);
        SunvoltumPhysics::Transform transform = ToPhysicsTransform(cf);

        SunvoltumPhysics::BodyType bodyType = SunvoltumPhysics::BodyType::Dynamic;
        if (anchored) {
            bodyType = SunvoltumPhysics::BodyType::Static;
        } else if (kinematic) {
            bodyType = SunvoltumPhysics::BodyType::Kinematic;
        }

        m_body = std::make_shared<SunvoltumPhysics::RigidBody>(bodyType, pShape, transform);

        // Настройка физических параметров
        m_body->SetFriction(PLASTIC_FRICTION);
        m_body->SetRestitution(PLASTIC_RESTITUTION);

        if (!anchored)
        {
            // Масса на основе объёма (плотность ~ 700)
            float volume = size.X * size.Y * size.Z;
            if (volume <= 0.0001f) volume = 1.0f;
            m_body->SetMass(volume * 0.7f);

            m_body->SetMaxLinearVelocity(1000.0f);
            m_body->SetMaxAngularVelocity(100.0f);
            m_body->SetMaxDepenetrationVelocity(50.0f);
        }

        world->GetWorld().AddBody(m_body);
        m_initialized = true;
        return true;
    }

    void PhysicsBody::Shutdown(PhysicsWorld* world)
    {
        if (!m_initialized) return;

        if (m_body && world)
        {
            world->GetWorld().RemoveBody(m_body);
        }
        m_body.reset();
        m_initialized = false;
    }

    CFrame PhysicsBody::GetCFrame() const
    {
        if (!m_body) return CFrame{};
        return FromPhysicsTransform(m_body->GetTransform());
    }

    Vector3 PhysicsBody::GetLinearVelocity() const
    {
        if (!m_body || m_anchored) return Vector3{};
        return FromPhysicsVec3(m_body->GetLinearVelocity());
    }

    Vector3 PhysicsBody::GetAngularVelocity() const
    {
        if (!m_body || m_anchored) return Vector3{};
        return FromPhysicsVec3(m_body->GetAngularVelocity());
    }

    void PhysicsBody::SetCFrame(const CFrame& cf)
    {
        if (!m_body) return;
        m_body->SetTransform(ToPhysicsTransform(cf));
    }

    void PhysicsBody::SetLinearVelocity(const Vector3& v)
    {
        if (!m_body || m_anchored) return;
        m_body->SetLinearVelocity(ToPhysicsVec3(v));
    }

    void PhysicsBody::SetAngularVelocity(const Vector3& v)
    {
        if (!m_body || m_anchored) return;
        m_body->SetAngularVelocity(ToPhysicsVec3(v));
    }

    void PhysicsBody::SetKinematic(bool kinematic)
    {
        if (!m_initialized || m_anchored || !m_body) return;
        if (m_kinematic == kinematic) return;

        m_kinematic = kinematic;
        m_body->SetType(kinematic ? SunvoltumPhysics::BodyType::Kinematic : SunvoltumPhysics::BodyType::Dynamic);
    }

    void PhysicsBody::SetKinematicTarget(const CFrame& cf)
    {
        if (!m_initialized || m_anchored || !m_kinematic || !m_body) return;
        m_body->SetTransform(ToPhysicsTransform(cf));
    }

    void PhysicsBody::WakeUp()
    {
        // В PGS solver bodies не деактивируются перманентно
    }

    void PhysicsBody::SetAnchored(bool anchored, PhysicsWorld* world)
    {
        if (!m_initialized || !m_body || m_anchored == anchored) return;

        (void)world;
        m_anchored = anchored;
        if (anchored)
        {
            m_body->SetType(SunvoltumPhysics::BodyType::Static);
            m_body->SetLinearVelocity(SunvoltumPhysics::Vector3::Zero);
            m_body->SetAngularVelocity(SunvoltumPhysics::Vector3::Zero);
        }
        else
        {
            m_body->SetType(m_kinematic ? SunvoltumPhysics::BodyType::Kinematic : SunvoltumPhysics::BodyType::Dynamic);
            float volume = m_size.X * m_size.Y * m_size.Z;
            if (volume <= 0.0001f) volume = 1.0f;
            m_body->SetMass(volume * 0.7f);
        }
    }

    void PhysicsBody::LockUpright()
    {
        if (!m_initialized || m_anchored || !m_body) return;
        m_body->SetLockAngular(true, false, true);
    }

    void PhysicsBody::SetCanCollide(bool canCollide)
    {
        m_canCollide = canCollide;
        // SunvoltumPhysics collision filtering: canCollide can be checked in broadphase/narrowphase
    }

    void PhysicsBody::Resize(const Vector3& newSize)
    {
        if (!m_initialized || !m_body) return;
        m_size = newSize;
        m_body->SetShape(CreateShape(m_shape, newSize));
    }

    void PhysicsBody::Reshape(Shape newShape)
    {
        if (!m_initialized || !m_body) return;
        m_shape = newShape;
        m_body->SetShape(CreateShape(newShape, m_size));
    }

} // namespace Sunvoltum
