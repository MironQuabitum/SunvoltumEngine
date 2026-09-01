#include "PhysicsWorld.h"
#include "PhysicsManager.h"
#include <iostream>

namespace Sunvoltum {

    bool PhysicsWorld::Init(PhysicsManager& manager, float gravityY)
    {
        if (m_initialized) return true;

        px::PxPhysics*              physics    = manager.GetPhysics();
        px::PxDefaultCpuDispatcher* dispatcher = manager.GetDispatcher();

        if (!physics || !dispatcher)
        {
            std::cerr << "[PhysicsWorld] PhysicsManager not initialized\n";
            return false;
        }

        px::PxSceneDesc sceneDesc(physics->getTolerancesScale());

        // Гравитация: только Y-вниз, X/Z = 0
        sceneDesc.gravity       = px::PxVec3(0.0f, gravityY, 0.0f);
        sceneDesc.cpuDispatcher = dispatcher;

        // PGS solver — выбираем явно, хотя это и дефолт PhysX 5
        sceneDesc.solverType    = px::PxSolverType::ePGS;

        // Флаги: отключаем то, что нам не нужно
        // eENABLE_CCD — continuous collision detection (дорого, не нужно пока)
        // eENABLE_GPU_DYNAMICS — GPU-симуляция (не используем)
        // Оставляем только базовую твёрдотельную симуляцию
        sceneDesc.flags =
            px::PxSceneFlag::eENABLE_ACTIVE_ACTORS;  // быстрый доступ к двигавшимся акторам

        sceneDesc.filterShader  = px::PxDefaultSimulationFilterShader;

        m_scene = physics->createScene(sceneDesc);
        if (!m_scene)
        {
            std::cerr << "[PhysicsWorld] createScene failed\n";
            return false;
        }

        m_initialized = true;
        std::cout << "[PhysicsWorld] Init OK (gravity=" << gravityY << ")\n";
        return true;
    }

    void PhysicsWorld::Shutdown()
    {
        if (!m_initialized) return;

        PxSafeRelease(m_scene);
        m_initialized = false;
        std::cout << "[PhysicsWorld] Shutdown\n";
    }

    void PhysicsWorld::Step(float dt)
    {
        if (!m_initialized || dt <= 0.0f) return;

        m_scene->simulate(dt);
        m_scene->fetchResults(true); // true = блокирует до завершения шага
    }

    void PhysicsWorld::SetGravity(float gravityY)
    {
        if (m_scene)
            m_scene->setGravity(px::PxVec3(0.0f, gravityY, 0.0f));
    }

} // namespace Sunvoltum
