#pragma once

#include "PhysicsCommon.h"
#include <memory>

namespace Sunvoltum {

    class PhysicsManager;

    // PhysicsWorld — обёртка над SunvoltumPhysics::World.
    class PhysicsWorld
    {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&)            = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        bool Init(PhysicsManager& manager, float gravityY = -196.2f);
        void Shutdown();

        // Шаг симуляции с фиксированным шагом dt
        void Step(float dt);

        // Изменить гравитацию на лету
        void SetGravity(float gravityY);

        bool IsInitialized() const { return m_initialized; }

        SunvoltumPhysics::World& GetWorld() { return *m_world; }
        const SunvoltumPhysics::World& GetWorld() const { return *m_world; }

    private:
        std::unique_ptr<SunvoltumPhysics::World> m_world;
        bool m_initialized = false;
    };

} // namespace Sunvoltum
