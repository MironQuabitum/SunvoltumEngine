#include "PhysicsBody.h"
#include "PhysicsWorld.h"
#include "../Types/Matrix3x3.h"
#include <algorithm>
#include <vector>
#include <iostream>

namespace Sunover {

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
            o.m_actor       = nullptr;
            o.m_material    = nullptr;
            o.m_initialized = false;
        }
        return *this;
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

        // --- Геометрия на основе Shape и Size ---
        px::PxGeometryHolder geomHolder;
        switch (shape)
        {
            case Shape::Ball:
            {
                // Радиус = половина наименьшей оси (чтобы сфера влезала в AABB)
                float radius = std::min({ size.X, size.Y, size.Z }) * 0.5f;
                if (radius <= 0.0f) radius = 0.5f;
                geomHolder.storeAny(px::PxSphereGeometry(radius));
                break;
            }
            case Shape::Cylinder:
            {
                // PhysX не имеет нативного цилиндра — используем капсулу.
                // Ориентация капсулы в PhysX: ось X (halfHeight вдоль X).
                // ShapePart::Size: X=диаметр, Y=высота, Z=диаметр
                float radius     = (size.X * 0.5f);
                float halfHeight = (size.Y * 0.5f) - radius;
                if (radius     <= 0.0f) radius     = 0.5f;
                if (halfHeight <  0.0f) halfHeight  = 0.0f;
                geomHolder.storeAny(px::PxCapsuleGeometry(radius, halfHeight));
                break;
            }
            case Shape::Block:
            default:
            {
                // Half-extents = Size / 2
                float hx = size.X * 0.5f;
                float hy = size.Y * 0.5f;
                float hz = size.Z * 0.5f;
                if (hx <= 0.0f) hx = 0.5f;
                if (hy <= 0.0f) hy = 0.5f;
                if (hz <= 0.0f) hz = 0.5f;
                geomHolder.storeAny(px::PxBoxGeometry(hx, hy, hz));
                break;
            }
        }

        px::PxTransform transform = ToPxTransform(cf);

        // --- Актор ---
        m_anchored   = anchored;
        m_canCollide = canCollide;

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
                // Убираем флаг симуляции — shape не участвует в коллизиях.
                // eSCENE_QUERY_SHAPE оставляем (raycasts продолжают работать).
                s->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, false);
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
                // Тело продолжает падать под гравитацией, но ни с чем не сталкивается.
                s->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, false);
            }

            // Автоматически вычислить инерцию и массовый центр из формы.
            // Плотность пластика ≈ 1200 кг/м³
            px::PxRigidBodyExt::updateMassAndInertia(*dynActor, 1200.0f);

            // Максимальный шаг депенетрации — ограничиваем чтобы не было «взрывов»
            dynActor->setMaxDepenetrationVelocity(10.0f);

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

    void PhysicsBody::SetCanCollide(bool canCollide)
    {
        if (!m_actor || m_canCollide == canCollide) return;

        m_canCollide = canCollide;

        // Итерируем по всем shapes актора и меняем флаг
        const px::PxU32 count = m_actor->getNbShapes();
        std::vector<px::PxShape*> shapes(count);
        m_actor->getShapes(shapes.data(), count);

        for (px::PxShape* s : shapes)
            s->setFlag(px::PxShapeFlag::eSIMULATION_SHAPE, canCollide);
    }

} // namespace Sunover
