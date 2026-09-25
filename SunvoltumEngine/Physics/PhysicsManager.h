#pragma once

#include "PhysicsCommon.h"

namespace Sunvoltum {

    class PhysicsManager
    {
    public:
        PhysicsManager()  = default;
        ~PhysicsManager() = default;

        PhysicsManager(const PhysicsManager&)            = delete;
        PhysicsManager& operator=(const PhysicsManager&) = delete;

        bool Init(uint32_t numThreads = 0)
        {
            (void)numThreads;
            m_initialized = true;
            return true;
        }

        void Shutdown()
        {
            m_initialized = false;
        }

        bool IsInitialized() const { return m_initialized; }

    private:
        bool m_initialized = false;
    };

} // namespace Sunvoltum
