#include "PhysicsBody.h"
#include "PhysicsWorld.h"
#include "../Types/Matrix3x3.h"
#include <algorithm>
#include <vector>
#include <iostream>

namespace Sunvoltum {

    // -----------------------------------------------------------------------
    // Материал «пластик» — хардкод
    // -----------------------------------------------------------------------
    static constexpr float PLASTIC_STATIC_FRICTION  = 0.50f;
    static constexpr float PLASTIC_DYNAMIC_FRICTION = 0.40f;
    static constexpr float PLASTIC_RESTITUTION      = 0.20f;

    // -----------------------------------------------------------------------
    // Конвертеры CFrame ↔ PxTransform
    // -----------------------------------------------------------------------

    px::PxTransform PhysicsBody::ToPxTransform(const CFrame& cf)
    {
        const Vector3&   p = cf.Position;
        const Matrix3x3& r = cf.Rotation;

        // PxMat33 — колонки, Matrix3x3 — строки (row-major)
        // PhysX: column vectors, наша матрица: row-major, но содержимое одно
        px::PxMat33 mat(
            px::PxVec3(r.R00, r.R10, r.R20),  // column 0
            px::PxVec3(r.R01, r.R11, r.R21),  // column 1
            px::PxVec3(r.R02, r.R12, r.R22)   // column 2
        );

        px::PxQuat q(mat);
        q.normalize();

        return px::PxTransform(px::PxVec3(p.X, p.Y, p.Z), q);
    }

    CFrame PhysicsBody::FromPxTransform(const px::PxTransform& t)
    {
        const px::PxVec3& p = t.p;
        px::PxMat33 mat(t.q);

        // PxMat33 колонки → наша строчная матрица
        Matrix3x3 r;
        r.R00 = mat.column0.x; r.R01 = mat.column1.x; r.R02 = mat.column2.x;
        r.R10 = mat.column0.y; r.R11 = mat.column1.y; r.R12 = mat.column2.y;
        r.R20 = mat.column0.z; r.R21 = mat.column1.z; r.R22 = mat.column2.z;

        return CFrame(Vector3(p.x, p.y, p.z), r);
    }

    // -----------------------------------------------------------------------
    // Move
    // -----------------------------------------------------------------------

    PhysicsBody::PhysicsBody(PhysicsBody&& o) noexcept
        : m_actor(o.m_actor)
        , m_material(o.m_material)
        , m_anchored(o.m_anchored)
        , m_canCollide(o.m_canCollide)
        , m_initialized(o.m_initialized)
        , m_size(o.m_size)
        , m_shape(o.m_shape)
    {
        o.m_actor       = nullptr;
        o.m_material    = nullptr;
        o.m_initialized = false;
    }

    PhysicsBody& PhysicsBody::operator=(PhysicsBody&& o) noexcept
    {
        if (this != &o)
        {
            m_actor       = o.m_actor;
            m_material    = o.m_material;
            m_anchored    = o.m_anchored;
            m_canCollide  = o.m_canCollide;
            m_initialized = o.m_initialized;
            m_size        = o.m_size;
            m_shape       = o.m_shape;
            o.m_actor       = nullptr;
            o.m_material    = nullptr;
            o.m_initialized = false;
        }
        return *this;
    }

    // -----------------------------------------------------------------------
    // Хелпер: построить PxGeometryHolder из Shape + Size
    // -----------------------------------------------------------------------
    static px::PxGeometryHolder BuildGeometry(Shape shape, const Vector3& size)
    {
        px::PxGeometryHolder geomHolder;
        switch (shape)
        {
            case Shape::Ball:
            {
                float radius = std::min({ size.X, size.Y, size.Z }) * 0.5f;
                if (radius <= 0.0f) radius = 0.5f;
                geomHolder.storeAny(px::PxSphereGeometry(radius));
                break;
            }
            case Shape::Cylinder:
            {
                float radius     = size.X * 0.5f;
                float halfHeight = (size.Y * 0.5f) - radius;
                if (radius     <= 0.0f) radius     = 0.5f;
                if (halfHeight <  0.0f) halfHeight  = 0.0f;
                geomHolder.storeAny(px::PxCapsuleGeometry(radius, halfHeight));
                break;
            }
            case Shape::Block:
            default:
            {
                float hx = std::max(size.X * 0.5f, 0.001f);
                float hy = std::max(size.Y * 0.5f, 0.001f);
                float hz = std::max(size.Z * 0.5f, 0.001f);
                geomHolder.storeAny(px::PxBoxGeometry(hx, hy, hz));
                break;
            }
        }
        return geomHolder;
    }

    // -----------------------------------------------------------------------
    // Init
    // -----------------------------------------------------------------------

    bool PhysicsBody::Init(
        px::PxPhysics* physics,
        px::PxScene*   scene,
        const CFrame&  cf,
        const Vector3& size,
        Shape          shape,
        bool           anchored,
        bool           canCollide)
    {
        if (m_initialized) return true;

        // --- Материал ---
        m_material = physics->createMaterial(
            PLASTIC_STATIC_FRICTION,
            PLASTIC_DYNAMIC_FRICTION,
            PLASTIC_RESTITUTION
        );
        if (!m_material)
        {
            std::cerr << "[PhysicsBody] createMaterial failed\n";
            return false;
        }

        // --- Геометрия ---
        px::PxGeometryHolder geomHolder = BuildGeometry(shape, size);

        px::PxTransform transform = ToPxTransform(cf);

        // --- Актор ---
        m_anchored   = anchored;
        m_canCollide = canCollide;
        m_size       = size;
        m_shape      = shape;

        if (anchored)
        {
            // Static: не двигается, не получает силы
            px::PxRigidStatic* staticActor = physics->createRigidStatic(transform);
            if (!staticActor)
            {
                std::cerr << "[PhysicsBody] createRigidStatic failed\n";
                PxSafeRelease(m_material);
                return false;
            }
            px::PxShape* s = px::PxRigidActorExt::createExclusiveShape(
                *staticActor, geomHolder.any(), *m_material);
            if (s && !canCollide)
            {
                // Убираем флаг симуляции и scene query — shape невидим для физики и рейкастов.
                s->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, false);
                s->setFlag(px::PxShapeFlag::eSCENE_QUERY_SHAPE,  false);
            }
            m_actor = staticActor;
        }
        else
        {
            // Dynamic: полная симуляция с PGS
            px::PxRigidDynamic* dynActor = physics->createRigidDynamic(transform);
            if (!dynActor)
            {
                std::cerr << "[PhysicsBody] createRigidDynamic failed\n";
                PxSafeRelease(m_material);
                return false;
            }
            px::PxShape* s = px::PxRigidActorExt::createExclusiveShape(
                *dynActor, geomHolder.any(), *m_material);
            if (s && !canCollide)
            {
                // Тело продолжает падать под гравитацией, но ни с чем не сталкивается и не видно рейкастом.
                s->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, false);
                s->setFlag(px::PxShapeFlag::eSCENE_QUERY_SHAPE,  false);
            }

            // Автоматически вычислить инерцию и массовый центр из формы.
            // Плотность в studs-масштабе (PhysX работает в стадах напрямую)
            px::PxRigidBodyExt::updateMassAndInertia(*dynActor, 700.0f);

            // При гравитации 196 studs/s² объекты разгоняются быстро —
            // поднимаем лимиты скорости выше дефолтных PhysX
            dynActor->setMaxLinearVelocity(1000.0f);
            dynActor->setMaxAngularVelocity(100.0f);
            dynActor->setMaxDepenetrationVelocity(50.0f);

            m_actor = dynActor;
        }

        scene->addActor(*m_actor);

        m_initialized = true;
        return true;
    }

    // -----------------------------------------------------------------------
    // Shutdown
    // -----------------------------------------------------------------------

    void PhysicsBody::Shutdown(px::PxScene* scene)
    {
        if (!m_initialized) return;

        if (m_actor && scene)
            scene->removeActor(*m_actor);

        PxSafeRelease(m_actor);
        PxSafeRelease(m_material);

        m_initialized = false;
    }

    // -----------------------------------------------------------------------
    // Чтение состояния PhysX → DataModel
    // -----------------------------------------------------------------------

    CFrame PhysicsBody::GetCFrame() const
    {
        if (!m_actor) return CFrame{};
        return FromPxTransform(m_actor->getGlobalPose());
    }

    Vector3 PhysicsBody::GetLinearVelocity() const
    {
        if (!m_actor || m_anchored) return Vector3{};

        auto* dyn = static_cast<px::PxRigidDynamic*>(m_actor);
        px::PxVec3 v = dyn->getLinearVelocity();
        return Vector3(v.x, v.y, v.z);
    }

    Vector3 PhysicsBody::GetAngularVelocity() const
    {
        if (!m_actor || m_anchored) return Vector3{};

        auto* dyn = static_cast<px::PxRigidDynamic*>(m_actor);
        px::PxVec3 v = dyn->getAngularVelocity();
        return Vector3(v.x, v.y, v.z);
    }

    // -----------------------------------------------------------------------
    // Запись состояния DataModel → PhysX
    // -----------------------------------------------------------------------

    void PhysicsBody::SetCFrame(const CFrame& cf)
    {
        if (!m_actor) return;
        m_actor->setGlobalPose(ToPxTransform(cf));
    }

    void PhysicsBody::SetLinearVelocity(const Vector3& v)
    {
        if (!m_actor || m_anchored) return;
        auto* dyn = static_cast<px::PxRigidDynamic*>(m_actor);
        dyn->setLinearVelocity(px::PxVec3(v.X, v.Y, v.Z));
    }

    void PhysicsBody::SetAngularVelocity(const Vector3& v)
    {
        if (!m_actor || m_anchored) return;
        auto* dyn = static_cast<px::PxRigidDynamic*>(m_actor);
        dyn->setAngularVelocity(px::PxVec3(v.X, v.Y, v.Z));
    }

    void PhysicsBody::SetAnchored(bool anchored, px::PxPhysics* physics, px::PxScene* scene)
    {
        if (!m_initialized || m_anchored == anchored) return;

        // Запомним текущее состояние перед удалением актора
        CFrame  cf         = GetCFrame();
        Vector3 linVel     = GetLinearVelocity();
        Vector3 angVel     = GetAngularVelocity();
        bool    canCollide = m_canCollide;

        // Получаем геометрию из существующего shape
        px::PxShape* oldShape = nullptr;
        m_actor->getShapes(&oldShape, 1);
        if (!oldShape) return;

        px::PxGeometryHolder geomHolder;
        geomHolder.storeAny(oldShape->getGeometry());

        // Удаляем старый актор
        scene->removeActor(*m_actor);
        PxSafeRelease(m_actor);
        m_actor    = nullptr;
        m_anchored = anchored;

        // Создаём новый актор нужного типа
        px::PxTransform transform = ToPxTransform(cf);

        if (anchored)
        {
            px::PxRigidStatic* staticActor = physics->createRigidStatic(transform);
            if (!staticActor) return;
            px::PxShape* s = px::PxRigidActorExt::createExclusiveShape(
                *staticActor, geomHolder.any(), *m_material);
            if (s && !canCollide)
            {
                s->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, false);
                s->setFlag(px::PxShapeFlag::eSCENE_QUERY_SHAPE,  false);
            }
            m_actor = staticActor;
        }
        else
        {
            px::PxRigidDynamic* dynActor = physics->createRigidDynamic(transform);
            if (!dynActor) return;
            px::PxShape* s = px::PxRigidActorExt::createExclusiveShape(
                *dynActor, geomHolder.any(), *m_material);
            if (s && !canCollide)
            {
                s->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, false);
                s->setFlag(px::PxShapeFlag::eSCENE_QUERY_SHAPE,  false);
            }
            px::PxRigidBodyExt::updateMassAndInertia(*dynActor, 700.0f);
            dynActor->setMaxLinearVelocity(1000.0f);
            dynActor->setMaxAngularVelocity(100.0f);
            dynActor->setMaxDepenetrationVelocity(50.0f);
            dynActor->setLinearVelocity(px::PxVec3(linVel.X, linVel.Y, linVel.Z));
            dynActor->setAngularVelocity(px::PxVec3(angVel.X, angVel.Y, angVel.Z));
            m_actor = dynActor;
        }

        scene->addActor(*m_actor);
    }

    // -----------------------------------------------------------------------
    // LockUpright — запрещает PhysX вращать тело по X и Z.
    // После этого тело всегда остаётся вертикальным, заваливание невозможно.
    // -----------------------------------------------------------------------
    void PhysicsBody::LockUpright()
    {
        if (!m_initialized || m_anchored || !m_actor) return;
        auto* dyn = static_cast<px::PxRigidDynamic*>(m_actor);
        dyn->setRigidDynamicLockFlag(px::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, true);
        dyn->setRigidDynamicLockFlag(px::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, true);
    }

    void PhysicsBody::SetCanCollide(bool canCollide)
    {
        if (!m_actor || m_canCollide == canCollide) return;

        m_canCollide = canCollide;

        // Итерируем по всем shapes актора и меняем флаг
        const px::PxU32 count = m_actor->getNbShapes();
        std::vector<px::PxShape*> shapes(count);
        m_actor->getShapes(shapes.data(), count);

        for (px::PxShape* s : shapes)
        {
            s->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, canCollide);
            s->setFlag(px::PxShapeFlag::eSCENE_QUERY_SHAPE,  canCollide);
        }
    }

    // -----------------------------------------------------------------------
    // Resize — обновить геометрию shape без пересоздания актора.
    // PhysX не позволяет менять геометрию напрямую — нужно detach/attach shape.
    // -----------------------------------------------------------------------
    void PhysicsBody::Resize(const Vector3& newSize)
    {
        if (!m_initialized || !m_actor) return;
        m_size = newSize;

        // Получаем текущий shape
        px::PxShape* oldShape = nullptr;
        if (m_actor->getNbShapes() == 0) return;
        m_actor->getShapes(&oldShape, 1);
        if (!oldShape) return;

        // Строим новую геометрию с тем же Shape-типом
        px::PxGeometryHolder geomHolder = BuildGeometry(m_shape, m_size);

        // Отсоединяем старый shape, создаём новый и присоединяем
        m_actor->detachShape(*oldShape);

        px::PxShape* newShape = m_actor->getScene()
            ? nullptr
            : nullptr; // scope guard

        // PxRigidActorExt::createExclusiveShape добавляет shape и даёт ownership актору
        newShape = px::PxRigidActorExt::createExclusiveShape(*m_actor, geomHolder.any(), *m_material);

        if (newShape && !m_canCollide)
        {
            newShape->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, false);
            newShape->setFlag(px::PxShapeFlag::eSCENE_QUERY_SHAPE,  false);
        }

        // Пересчитываем массу для dynamic тела
        if (!m_anchored)
        {
            auto* dyn = static_cast<px::PxRigidDynamic*>(m_actor);
            px::PxRigidBodyExt::updateMassAndInertia(*dyn, 700.0f);
        }
    }

    // -----------------------------------------------------------------------
    // Reshape — сменить тип геометрии (Block↔Ball↔Cylinder).
    // Аналогично Resize: detach старый shape, attach новый.
    // -----------------------------------------------------------------------
    void PhysicsBody::Reshape(Shape newShape, px::PxPhysics* /*physics*/, px::PxScene* /*scene*/)
    {
        if (!m_initialized || !m_actor || m_shape == newShape) return;
        m_shape = newShape;

        px::PxShape* oldShape = nullptr;
        if (m_actor->getNbShapes() == 0) return;
        m_actor->getShapes(&oldShape, 1);
        if (!oldShape) return;

        px::PxGeometryHolder geomHolder = BuildGeometry(m_shape, m_size);

        m_actor->detachShape(*oldShape);

        px::PxShape* newShapePtr = px::PxRigidActorExt::createExclusiveShape(
            *m_actor, geomHolder.any(), *m_material);

        if (newShapePtr && !m_canCollide)
        {
            newShapePtr->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, false);
            newShapePtr->setFlag(px::PxShapeFlag::eSCENE_QUERY_SHAPE,  false);
        }

        if (!m_anchored)
        {
            auto* dyn = static_cast<px::PxRigidDynamic*>(m_actor);
            px::PxRigidBodyExt::updateMassAndInertia(*dyn, 700.0f);
        }
    }

} // namespace Sunvoltum
