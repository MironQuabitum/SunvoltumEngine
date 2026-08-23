#pragma once

#include <memory>
#include "../LibSunover.h"
#include "../Types/Vector3.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunover {

    class Engine;
    class Instance;

    class LibSunover PhysicsBridge
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

        // Применить импульс к физическому телу, соответствующему Instance.
        // force  — вектор силы в studs/s² (автоматически пересчитывается в м/с²).
        // Если Instance не найден в кэше или тело Anchored — вызов игнорируется.
        void ApplyImpulse(Instance& inst, const Vector3& force);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace Sunover

#pragma warning(pop)
