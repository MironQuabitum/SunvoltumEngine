#include "PhysicsManager.h"
#include <iostream>

namespace Sunvoltum {

    bool PhysicsManager::Init(uint32_t numThreads)
    {
        if (m_initialized) return true;

        // --- Foundation ---
        m_foundation = PxCreateFoundation(
            PX_PHYSICS_VERSION,
            m_allocator,
            m_errorCallback
        );
        if (!m_foundation)
        {
            std::cerr << "[PhysicsManager] PxCreateFoundation failed\n";
            return false;
        }

        // --- Physics (без PVD в релизе) ---
        m_physics = PxCreatePhysics(
            PX_PHYSICS_VERSION,
            *m_foundation,
            px::PxTolerancesScale(),
            false,   // trackOutstandingAllocations
            nullptr  // PVD
        );
        if (!m_physics)
        {
            std::cerr << "[PhysicsManager] PxCreatePhysics failed\n";
            PxSafeRelease(m_foundation);
            return false;
        }

        // --- CPU Dispatcher (PGS работает на CPU) ---
        m_dispatcher = px::PxDefaultCpuDispatcherCreate(numThreads);
        if (!m_dispatcher)
        {
            std::cerr << "[PhysicsManager] PxDefaultCpuDispatcherCreate failed\n";
            PxSafeRelease(m_physics);
            PxSafeRelease(m_foundation);
            return false;
        }

        m_initialized = true;
        std::cout << "[PhysicsManager] Init OK (threads=" << numThreads << ")\n";
        return true;
    }

    void PhysicsManager::Shutdown()
    {
        if (!m_initialized) return;

        PxSafeRelease(m_dispatcher);
        PxSafeRelease(m_physics);
        PxSafeRelease(m_foundation);

        m_initialized = false;
        std::cout << "[PhysicsManager] Shutdown\n";
    }

} // namespace Sunvoltum
