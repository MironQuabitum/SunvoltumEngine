#include "PhysicsWorld.h"
#include "PhysicsManager.h"
#include <iostream>

namespace Sunvoltum {

    PhysicsWorld::PhysicsWorld() = default;
    PhysicsWorld::~PhysicsWorld()
    {
        Shutdown();
    }

    bool PhysicsWorld::Init(PhysicsManager& manager, float gravityY)
    {
        (void)manager;
        if (m_initialized) return true;

        m_world = std::make_unique<SunvoltumPhysics::World>(
            SunvoltumPhysics::Vector3(0.0f, gravityY, 0.0f)
        );

        m_initialized = true;
        std::cout << "[PhysicsWorld] SunvoltumPhysics::World Init OK (gravity=" << gravityY << ")\n";
        return true;
    }

    void PhysicsWorld::Shutdown()
    {
        if (!m_initialized) return;

        m_world.reset();
        m_initialized = false;
        std::cout << "[PhysicsWorld] Shutdown\n";
    }

    void PhysicsWorld::Step(float dt)
    {
        if (!m_initialized || !m_world || dt <= 0.0f) return;
        m_world->Step(dt);
    }

    void PhysicsWorld::SetGravity(float gravityY)
    {
        if (m_world)
        {
            m_world->SetGravity(SunvoltumPhysics::Vector3(0.0f, gravityY, 0.0f));
        }
    }

} // namespace Sunvoltum
