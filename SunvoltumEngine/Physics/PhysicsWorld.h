#pragma once

#include "PhysicsCommon.h"

namespace Sunvoltum {

    class PhysicsManager;

    // PhysicsWorld — обёртка над PxScene.
    // Использует PGS (Projected Gauss-Seidel) solver — дефолтный и
    // наименее ресурсоёмкий решатель в PhysX 5.
    // Один экземпляр на движок; создаётся PhysicsBridge после Init менеджера.
    class PhysicsWorld
    {
    public:
        PhysicsWorld()  = default;
        ~PhysicsWorld() = default;

        PhysicsWorld(const PhysicsWorld&)            = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        // Создать PxScene с заданной гравитацией.
        bool Init(PhysicsManager& manager, float gravityY = -9.8f);

        // Освободить PxScene.
        void Shutdown();

        // Продвинуть симуляцию на dt секунд (вызывать с фиксированным шагом).
        void Step(float dt);

        // Изменить гравитацию на лету (например, при смене Workspace.Gravity).
        void SetGravity(float gravityY);

        bool IsInitialized() const { return m_initialized; }

        px::PxScene* GetScene() const { return m_scene; }

    private:
        px::PxScene* m_scene       = nullptr;
        bool         m_initialized = false;
    };

} // namespace Sunvoltum
