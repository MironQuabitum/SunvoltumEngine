#pragma once

#include "PhysicsCommon.h"

// PhysicsManager НЕ экспортируется через LibSunover —
// он полностью внутренний для LibSunover.dll и никогда не используется
// клиентским кодом напрямую. Это устраняет C4251 для PhysX-типов.

namespace Sunover {

    // PhysicsManager — владеет глобальными синглтонами PhysX 5:
    //   PxFoundation, PxPhysics, PxDefaultCpuDispatcher.
    // Один экземпляр живёт на весь движок (хранится в Engine через PhysicsBridge).
    class PhysicsManager
    {
    public:
        PhysicsManager()  = default;
        ~PhysicsManager() = default;

        PhysicsManager(const PhysicsManager&)            = delete;
        PhysicsManager& operator=(const PhysicsManager&) = delete;

        // Инициализировать PhysX Foundation + Physics.
        // numThreads — число потоков CPU-диспетчера; 0 = один поток.
        bool Init(uint32_t numThreads = 0);

        // Освободить все PhysX-объекты.
        void Shutdown();

        bool IsInitialized() const { return m_initialized; }

        px::PxPhysics*              GetPhysics()    const { return m_physics;    }
        px::PxDefaultCpuDispatcher* GetDispatcher() const { return m_dispatcher; }

    private:
        // Allocator и ErrorCallback живут здесь по значению —
        // они должны переживать PxFoundation.
        // Нет LibSunover на классе → нет C4251.
        px::PxDefaultAllocator     m_allocator;
        px::PxDefaultErrorCallback m_errorCallback;

        px::PxFoundation*            m_foundation  = nullptr;
        px::PxPhysics*               m_physics     = nullptr;
        px::PxDefaultCpuDispatcher*  m_dispatcher  = nullptr;

        bool m_initialized = false;
    };

} // namespace Sunover
