#pragma once

#include "PhysicsCommon.h"
#include "../Types/CFrame.h"
#include "../Types/Vector3.h"
#include "../Types/Shape.h"

namespace Sunover {

    class PhysicsWorld;

    // PhysicsBody — обёртка над одним PhysX-актором (static или dynamic rigid body).
    //
    // Соответствие типам:
    //   Anchored=true  → PxRigidStatic  (не двигается, не получает силы)
    //   Anchored=false → PxRigidDynamic (полная симуляция)
    //
    // Форма ShapePart::Shape маппится на примитивные геометрии PhysX:
    //   Block    → PxBoxGeometry    (halfExtents = Size/2)
    //   Ball     → PxSphereGeometry (radius = min(Size)/2)
    //   Cylinder → PxCapsuleGeometry (radius = Size.X/2, halfHeight = Size.Y/2)
    //
    // Материал захардкожен под пластик:
    //   staticFriction  = 0.50
    //   dynamicFriction = 0.40
    //   restitution     = 0.20
    class PhysicsBody
    {
    public:
        PhysicsBody()  = default;
        ~PhysicsBody() = default;

        PhysicsBody(const PhysicsBody&)            = delete;
        PhysicsBody& operator=(const PhysicsBody&) = delete;

        PhysicsBody(PhysicsBody&&) noexcept;
        PhysicsBody& operator=(PhysicsBody&&) noexcept;

        // Создать актор и добавить его в сцену.
        // physics    — из PhysicsManager::GetPhysics()
        // scene      — из PhysicsWorld::GetScene()
        // cf         — начальная позиция/ориентация
        // size       — ShapePart::Size (полный размер, не half-extents)
        // shape      — тип примитива
        // anchored   — статик или динамик
        // canCollide — если false, shape не участвует в коллизиях (но гравитация работает)
        bool Init(
            px::PxPhysics* physics,
            px::PxScene*   scene,
            const CFrame&  cf,
            const Vector3& size,
            Shape          shape,
            bool           anchored,
            bool           canCollide = true
        );

        // Удалить актор из сцены и освободить ресурсы.
        void Shutdown(px::PxScene* scene);

        bool IsInitialized() const { return m_initialized; }
        bool IsAnchored()    const { return m_anchored;    }

        // Включить/выключить коллизии на лету (меняет флаги shape без пересоздания актора).
        void SetCanCollide(bool canCollide);

        // --- Чтение состояния из PhysX → DataModel ---

        // Получить текущий CFrame (позиция + ориентация) из актора.
        CFrame  GetCFrame()       const;

        // Получить линейную скорость (только dynamic).
        Vector3 GetLinearVelocity()  const;

        // Получить угловую скорость (только dynamic).
        Vector3 GetAngularVelocity() const;

        // --- Запись состояния DataModel → PhysX ---

        // Телепортировать актор (установить позицию напрямую, без физики).
        void SetCFrame(const CFrame& cf);

        // Установить линейную скорость (только dynamic).
        void SetLinearVelocity(const Vector3& v);

        // Установить угловую скорость (только dynamic).
        void SetAngularVelocity(const Vector3& v);

    private:
        // Построить PxTransform из Sunover::CFrame
        static px::PxTransform ToPxTransform(const CFrame& cf);

        // Построить Sunover::CFrame из PxTransform
        static CFrame FromPxTransform(const px::PxTransform& t);

        px::PxRigidActor* m_actor       = nullptr;
        px::PxMaterial*   m_material    = nullptr;
        bool              m_anchored    = false;
        bool              m_canCollide  = true;
        bool              m_initialized = false;
    };

} // namespace Sunover
