#pragma once

#include <memory>
#include "../LibSunvoltum.h"
#include "../Types/Vector3.h"
#include "../Types/CFrame.h"
#include "../Types/RaycastResult.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunvoltum {

    class Engine;
    class Instance;

    class LibSunvoltum PhysicsBridge
    {
    public:
        PhysicsBridge();
        ~PhysicsBridge();

        PhysicsBridge(const PhysicsBridge&)            = delete;
        PhysicsBridge& operator=(const PhysicsBridge&) = delete;

        bool Init(Engine& engine);
        void Step(float dt);
        void Shutdown();
        bool IsInitialized() const;

        void ApplyImpulse(Instance& inst, const Vector3& force);
        void LockUpright(Instance& inst);

        // Cast a ray from origin in direction (need not be normalized) up to maxDist studs.
        // Returns RaycastResult with Hit=true if any physics body was found.
        // ignoreInst: pass nullptr to ignore nothing.
        RaycastResult Raycast(const Vector3& origin,
                              const Vector3& direction,
                              float          maxDist,
                              Instance*      ignoreInst = nullptr) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace Sunvoltum

#pragma warning(pop)
