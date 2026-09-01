#pragma once

#include "PhysicsCommon.h"
#include "../Types/CFrame.h"
#include "../Types/Vector3.h"
#include "../Types/Shape.h"

namespace Sunvoltum {

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

        // Сменить anchored-режим на лету: пересоздаёт актор как Static или Dynamic.
        // physics/scene нужны для удаления старого и создания нового актора.
        void SetAnchored(bool anchored, px::PxPhysics* physics, px::PxScene* scene);

        // Заморозить вращение по осям X и Z (только dynamic).
        // Используется для Humanoid-тел: персонаж не должен заваливаться.
        void LockUpright();

        // Изменить размер тела (обновляет геометрию PhysX shape без пересоздания актора).
        void Resize(const Vector3& newSize);

        // Изменить форму тела (пересоздаёт геометрию; актор остаётся тем же).
        void Reshape(Shape newShape, px::PxPhysics* physics, px::PxScene* scene);

        // Получить сырой указатель на PxRigidActor (для raycast-фильтрации).
        px::PxRigidActor* GetActor() const { return m_actor; }

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
        // Построить PxTransform из Sunvoltum::CFrame
        static px::PxTransform ToPxTransform(const CFrame& cf);

        // Построить Sunvoltum::CFrame из PxTransform
        static CFrame FromPxTransform(const px::PxTransform& t);

        px::PxRigidActor* m_actor       = nullptr;
        px::PxMaterial*   m_material    = nullptr;
        bool              m_anchored    = false;
        bool              m_canCollide  = true;
        bool              m_initialized = false;

        // Запоминаем последние Size и Shape чтобы Resize/Reshape могли их использовать
        Vector3 m_size  = { 1.0f, 1.0f, 1.0f };
        Shape   m_shape = Shape::Block;
    };

} // namespace Sunvoltum
