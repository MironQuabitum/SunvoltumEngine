#include "PhysicsBridge.h"
#include "PhysicsManager.h"
#include "PhysicsWorld.h"
#include "PhysicsBody.h"
#include "../Core/Engine.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceClasses/ShapePart.h"
#include "../DataModel/InstanceClasses/Workspace.h"
#include "../DataModel/InstanceClasses/Model.h"
#include "../DataModel/InstanceClasses/Folder.h"
#include "../DataModel/InstanceClasses/Motor6D.h"
#include "../DataModel/PropertyValue.h"
#include "../DataModel/PropertyManager.h"
#include "../DataModel/InstanceParent.h"   // ChildAddedToken
#include "../Types/RaycastResult.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <iostream>

namespace Sunvoltum {

    // PhysX works in studs directly -- no unit conversion needed.
    // 1 stud = 1 PhysX unit. Gravity is set in studs/s^2.
    static constexpr float STUDS_TO_METERS = 1.0f;

    static float GravityToPhysX(float studsPerSecSq)
    {
        return -std::abs(studsPerSecSq);
    }

    // -----------------------------------------------------------------------
    // Инверсия CFrame (только для ортогональных матриц вращения).
    // Inv(cf) = CFrame(-R^T * p, R^T)
    // Используется в Motor6D: worldCF1 = Part0.CFrame * C0 * Inv(C1)
    // -----------------------------------------------------------------------
    static CFrame CFrameInverse(const CFrame& cf)
    {
        // Транспонирование матрицы вращения = её обратная (т.к. ортогональная)
        const Matrix3x3& R = cf.Rotation;
        Matrix3x3 Rt(
            R.R00, R.R10, R.R20,
            R.R01, R.R11, R.R21,
            R.R02, R.R12, R.R22
        );
        // Обратная позиция: -R^T * p
        const Vector3& p = cf.Position;
        Vector3 invPos(
            -(Rt.R00 * p.X + Rt.R01 * p.Y + Rt.R02 * p.Z),
            -(Rt.R10 * p.X + Rt.R11 * p.Y + Rt.R12 * p.Z),
            -(Rt.R20 * p.X + Rt.R21 * p.Y + Rt.R22 * p.Z)
        );
        return CFrame(invPos, Rt);
    }

    // -----------------------------------------------------------------------
    // Данные одного Motor6D-соединения.
    // -----------------------------------------------------------------------
    struct Motor6DEntry
    {
        Instance* motorInst = nullptr; // сам объект Motor6D
        Instance* part0     = nullptr; // опорная часть
        Instance* part1     = nullptr; // ведомая часть

        // Подписки на изменения C0, C1, Part0, Part1 у этого мотора
        PropertyToken c0Token;
        PropertyToken c1Token;
        PropertyToken part0Token;
        PropertyToken part1Token;
    };

    // -----------------------------------------------------------------------
    // Подписки на свойства одного ShapePart-тела (редко меняющиеся).
    // -----------------------------------------------------------------------
    struct BodySubscriptions
    {
        PropertyToken anchored;
        PropertyToken canCollide;
        PropertyToken cframe;   // телепорт
        PropertyToken size;
        PropertyToken shape;
    };

    // -----------------------------------------------------------------------
    // pImpl
    // -----------------------------------------------------------------------
    struct PhysicsBridge::Impl
    {
        Engine*    engine    = nullptr;
        DataModel* dataModel = nullptr;

        PhysicsManager manager;
        PhysicsWorld   world;

        // Кэш тел: ключ — reinterpret_cast<uintptr_t>(Instance*)
        std::unordered_map<uintptr_t, PhysicsBody>       bodies;

        // Порядок добавления — SyncIn/SyncOut итерируют именно его.
        // Никакого обхода дерева в hot path.
        std::vector<uintptr_t>                           bodyOrder;

        // Подписки per-тело
        std::unordered_map<uintptr_t, BodySubscriptions> bodyTokens;

        // Подписка на Workspace::Gravity
        PropertyToken gravityToken;

        // Подписка на ChildAdded Workspace — живёт до Shutdown
        ChildAddedToken workspaceChildToken;

        // Подписки на ChildAdded вложенных контейнеров (Model, Folder)
        std::vector<ChildAddedToken> containerTokens;

        // Телепорты dynamic тел в текущем тике (SetProperty(CFrame) извне)
        std::unordered_set<uintptr_t> pendingTeleports;

        // -----------------------------------------------------------------------
        // Motor6D: список активных соединений.
        // Ключ — reinterpret_cast<uintptr_t>(Instance*) мотора.
        // -----------------------------------------------------------------------
        std::unordered_map<uintptr_t, Motor6DEntry> motors;
        std::vector<uintptr_t>                      motorOrder;

        // true пока выполняется SyncOut — CFrame-callback пропускает round-trip
        bool inSyncOut   = false;
        bool initialized = false;

        // Создать PhysicsBody для ShapePart и зарегистрировать подписки.
        // Вызывается из SubscribeWorkspace (и через ChildAdded коллбэк).
        void RegisterBody(Instance* inst);

        // Зарегистрировать Motor6D-соединение.
        void RegisterMotor(Instance* inst);

        // Рекурсивно обходит контейнер (Workspace/Model/Folder), регистрирует ShapePart'ы.
        void RegisterBodyRecursive(Instance* inst);

        // Подписки на редко меняющиеся свойства тела
        void SubscribeBody(Instance* inst);

        // Подписаться на ChildAdded Workspace; зарегистрировать уже существующие тела
        void SubscribeWorkspace();

        // velocity sync + SyncOut
        void SyncIn();
        void SyncOut();
        void SyncMotors(); // Motor6D: обновляем CFrame ведомых частей
    };

    // -----------------------------------------------------------------------
    // Ctor / Dtor
    // -----------------------------------------------------------------------
    PhysicsBridge::PhysicsBridge()  : m_impl(std::make_unique<Impl>()) {}
    PhysicsBridge::~PhysicsBridge() { Shutdown(); }

    // -----------------------------------------------------------------------
    // Init
    // -----------------------------------------------------------------------
    bool PhysicsBridge::Init(Engine& engine)
    {
        if (m_impl->initialized) return true;

        m_impl->engine    = &engine;
        m_impl->dataModel = &engine.DataModel;

        if (!m_impl->manager.Init(0))
            return false;

        // Начальная гравитация из Workspace
        float gravityStuds = 196.2f;
        {
            Instance* ws = m_impl->dataModel->FindByName("Workspace");
            if (ws)
            {
                auto* prop = ws->GetProperty(Classes::Workspace::Gravity);
                if (prop && prop->Type == PropertyType::Float)
                    gravityStuds = std::abs(prop->Value.AsFloat);
                else if (prop && prop->Type == PropertyType::Number)
                    gravityStuds = static_cast<float>(std::abs(prop->Value.AsNumber));
            }
        }

        if (!m_impl->world.Init(m_impl->manager, GravityToPhysX(gravityStuds)))
        {
            m_impl->manager.Shutdown();
            return false;
        }

        // Подписка на изменение гравитации
        {
            Instance* ws = m_impl->dataModel->FindByName("Workspace");
            if (ws)
            {
                m_impl->gravityToken = PropertyManager::Get().Subscribe(
                    ws, Classes::Workspace::Gravity,
                    [this](const PropertyValue& val)
                    {
                        float studs = 0.0f;
                        if (val.Type == PropertyType::Float)
                            studs = std::abs(val.Value.AsFloat);
                        else if (val.Type == PropertyType::Number)
                            studs = static_cast<float>(std::abs(val.Value.AsNumber));
                        else
                            return;
                        m_impl->world.SetGravity(GravityToPhysX(studs));
                        std::cout << "[PhysicsBridge] Gravity -> " << studs << " studs/s^2\n";
                    });
            }
        }

        // Подписываемся на Workspace::ChildAdded и регистрируем существующие тела
        m_impl->SubscribeWorkspace();

        m_impl->initialized = true;
        std::cout << "[PhysicsBridge] Init OK\n";
        return true;
    }

    // -----------------------------------------------------------------------
    // Shutdown
    // -----------------------------------------------------------------------
    void PhysicsBridge::Shutdown()
    {
        if (!m_impl || !m_impl->initialized) return;

        // Отписываемся от ChildAdded до очистки тел
        m_impl->workspaceChildToken.Disconnect();
        m_impl->containerTokens.clear();

        // Отписываемся от всех property-подписок
        m_impl->bodyTokens.clear();
        m_impl->gravityToken.Disconnect();
        m_impl->pendingTeleports.clear();
        m_impl->bodyOrder.clear();

        // Очищаем Motor6D-соединения
        m_impl->motors.clear();
        m_impl->motorOrder.clear();

        for (auto& [key, body] : m_impl->bodies)
            body.Shutdown(m_impl->world.GetScene());
        m_impl->bodies.clear();

        m_impl->world.Shutdown();
        m_impl->manager.Shutdown();

        m_impl->initialized = false;
        std::cout << "[PhysicsBridge] Shutdown\n";
    }

    bool PhysicsBridge::IsInitialized() const
    {
        return m_impl && m_impl->initialized;
    }

    // -----------------------------------------------------------------------
    // Step: SyncIn -> simulate -> SyncOut
    // -----------------------------------------------------------------------
    void PhysicsBridge::Step(float dt)
    {
        if (!m_impl->initialized) return;

        m_impl->SyncIn();
        m_impl->world.Step(dt);
        m_impl->SyncOut();
        m_impl->SyncMotors(); // обновляем конечности после физики
    }

    // -----------------------------------------------------------------------
    // RegisterBodyRecursive — рекурсивно обходит контейнер и регистрирует
    // ShapePart'ы как физические тела.
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::RegisterBodyRecursive(Instance* inst)
    {
        if (!inst) return;

        const int8_t cls = inst->GetClassId();

        if (cls == Classes::ShapePart::ClassId)
        {
            RegisterBody(inst);
            return;
        }

        // Motor6D — регистрируем соединение, не физическое тело
        if (cls == Classes::CLASS_MOTOR6D)
        {
            RegisterMotor(inst);
            return;
        }

        bool isContainer = (cls == Classes::CLASS_MODEL)
                        || (cls == Classes::CLASS_FOLDER)
                        || (cls == Classes::Workspace::ClassId);
        if (!isContainer) return;

        for (auto& child : inst->GetChildren())
            RegisterBodyRecursive(child.get());

        containerTokens.push_back(inst->SubscribeChildAdded(
            [this](Instance& child)
            {
                RegisterBodyRecursive(&child);
            }));
    }

    // -----------------------------------------------------------------------
    // SubscribeWorkspace
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SubscribeWorkspace()
    {
        Instance* ws = dataModel->FindByName("Workspace");
        if (!ws)
        {
            std::cerr << "[PhysicsBridge] SubscribeWorkspace: Workspace not found\n";
            return;
        }

        // Рекурсивно регистрируем всё дерево Workspace
        RegisterBodyRecursive(ws);
    }

    // -----------------------------------------------------------------------
    // RegisterBody — создать PhysicsBody для ShapePart и подписаться.
    // Вызывается из SubscribeWorkspace и ChildAdded-коллбэка.
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::RegisterBody(Instance* inst)
    {
        uintptr_t key = reinterpret_cast<uintptr_t>(inst);
        if (bodies.count(key)) return; // уже зарегистрирован

        px::PxPhysics* physics = manager.GetPhysics();
        px::PxScene*   scene   = world.GetScene();

        CFrame  cf        = {};
        Vector3 size      = { 1.0f, 1.0f, 1.0f };
        Shape   shape     = Shape::Block;
        bool    anchored  = false;
        bool    canCollide = true;

        auto* cfProp = inst->GetProperty(Classes::ShapePart::CFrame);
        if (cfProp && cfProp->Type == PropertyType::CFrame)
            cf = cfProp->Value.AsCFrame;

        auto* sizeProp = inst->GetProperty(Classes::ShapePart::Size);
        if (sizeProp && sizeProp->Type == PropertyType::Vector3)
            size = sizeProp->Value.AsVector3;

        auto* shapeProp = inst->GetProperty(Classes::ShapePart::Shape);
        if (shapeProp && shapeProp->Type == PropertyType::Shape)
            shape = shapeProp->Value.AsShape;

        auto* anchorProp = inst->GetProperty(Classes::ShapePart::Anchored);
        if (anchorProp && anchorProp->Type == PropertyType::Bool)
            anchored = anchorProp->Value.AsBool;

        auto* canCollideProp = inst->GetProperty(Classes::ShapePart::CanCollide);
        if (canCollideProp && canCollideProp->Type == PropertyType::Bool)
            canCollide = canCollideProp->Value.AsBool;

        PhysicsBody body;
        if (!body.Init(physics, scene, cf, size, shape, anchored, canCollide))
            return;

        if (!anchored)
        {
            auto* posVelProp = inst->GetProperty(Classes::ShapePart::PosVelocity);
            if (posVelProp && posVelProp->Type == PropertyType::Vector3)
                body.SetLinearVelocity(posVelProp->Value.AsVector3);

            auto* rotVelProp = inst->GetProperty(Classes::ShapePart::RotVelocity);
            if (rotVelProp && rotVelProp->Type == PropertyType::Vector3)
                body.SetAngularVelocity(rotVelProp->Value.AsVector3);
        }

        bodies.emplace(key, std::move(body));
        bodyOrder.push_back(key);

        SubscribeBody(inst);
    }

    // -----------------------------------------------------------------------
    // RegisterMotor — зарегистрировать Motor6D-соединение.
    // Читает Part0/Part1 и подписывается на изменения C0, C1, Part0, Part1.
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::RegisterMotor(Instance* inst)
    {
        uintptr_t key = reinterpret_cast<uintptr_t>(inst);
        if (motors.count(key)) return; // уже зарегистрирован

        Motor6DEntry entry;
        entry.motorInst = inst;

        // Читаем Part0 и Part1 из свойств
        auto* p0prop = inst->GetProperty(Classes::Motor6D::Part0);
        if (p0prop && p0prop->Type == PropertyType::InstanceRef)
            entry.part0 = p0prop->Value.AsInstanceRef;

        auto* p1prop = inst->GetProperty(Classes::Motor6D::Part1);
        if (p1prop && p1prop->Type == PropertyType::InstanceRef)
            entry.part1 = p1prop->Value.AsInstanceRef;

        // Подписка на смену Part0
        entry.part0Token = PropertyManager::Get().Subscribe(
            inst, Classes::Motor6D::Part0,
            [this, key](const PropertyValue& val)
            {
                auto it = motors.find(key);
                if (it == motors.end()) return;
                it->second.part0 = (val.Type == PropertyType::InstanceRef)
                                   ? val.Value.AsInstanceRef : nullptr;
            });

        // Подписка на смену Part1
        entry.part1Token = PropertyManager::Get().Subscribe(
            inst, Classes::Motor6D::Part1,
            [this, key](const PropertyValue& val)
            {
                auto it = motors.find(key);
                if (it == motors.end()) return;
                it->second.part1 = (val.Type == PropertyType::InstanceRef)
                                   ? val.Value.AsInstanceRef : nullptr;
            });

        // C0 и C1 читаются напрямую из Instance каждый кадр в SyncOut —
        // подписки нужны только если хочется реагировать немедленно.
        // Для простоты используем polling в SyncOut, токены оставляем пустыми.

        motors.emplace(key, std::move(entry));
        motorOrder.push_back(key);

        std::cout << "[PhysicsBridge] Motor6D registered: " << inst->GetName() << "\n";
    }

    // -----------------------------------------------------------------------
    // SubscribeBody — подписки на редко меняющиеся свойства тела.
    // Velocity намеренно не здесь — она меняется каждый тик (feedback loop).
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SubscribeBody(Instance* inst)
    {
        uintptr_t key = reinterpret_cast<uintptr_t>(inst);
        auto& subs = bodyTokens[key];

        px::PxPhysics* physics = manager.GetPhysics();
        px::PxScene*   scene   = world.GetScene();

        // Anchored
        subs.anchored = PropertyManager::Get().Subscribe(
            inst, Classes::ShapePart::Anchored,
            [this, key, physics, scene](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Bool) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                bool nowAnchored = val.Value.AsBool;
                if (nowAnchored != it->second.IsAnchored())
                    it->second.SetAnchored(nowAnchored, physics, scene);
            });

        // CanCollide
        subs.canCollide = PropertyManager::Get().Subscribe(
            inst, Classes::ShapePart::CanCollide,
            [this, key](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Bool) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                it->second.SetCanCollide(val.Value.AsBool);
            });

        // CFrame — телепорт (игнорируется если пишет SyncOut)
        subs.cframe = PropertyManager::Get().Subscribe(
            inst, Classes::ShapePart::CFrame,
            [this, key](const PropertyValue& val)
            {
                if (inSyncOut) return;
                if (val.Type != PropertyType::CFrame) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                it->second.SetCFrame(val.Value.AsCFrame);
                if (!it->second.IsAnchored())
                    pendingTeleports.insert(key);
            });

        // Size
        subs.size = PropertyManager::Get().Subscribe(
            inst, Classes::ShapePart::Size,
            [this, key](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Vector3) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                it->second.Resize(val.Value.AsVector3);
            });

        // Shape
        subs.shape = PropertyManager::Get().Subscribe(
            inst, Classes::ShapePart::Shape,
            [this, key, physics, scene](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Shape) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                it->second.Reshape(val.Value.AsShape, physics, scene);
            });
    }

    // -----------------------------------------------------------------------
    // SyncIn — только velocity sync для dynamic тел.
    //
    // Регистрация новых тел полностью вынесена в RegisterBody / SubscribeWorkspace.
    // Никакого обхода дерева здесь нет.
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SyncIn()
    {
        for (uintptr_t key : bodyOrder)
        {
            auto bodyIt = bodies.find(key);
            if (bodyIt == bodies.end()) continue;

            PhysicsBody& body = bodyIt->second;
            if (body.IsAnchored()) continue;

            // Velocity меняется каждый тик — поллинг дешевле чем callback
            Instance* inst = reinterpret_cast<Instance*>(key);

            auto* posVelProp = inst->GetProperty(Classes::ShapePart::PosVelocity);
            if (posVelProp && posVelProp->Type == PropertyType::Vector3)
                body.SetLinearVelocity(posVelProp->Value.AsVector3);

            auto* rotVelProp = inst->GetProperty(Classes::ShapePart::RotVelocity);
            if (rotVelProp && rotVelProp->Type == PropertyType::Vector3)
                body.SetAngularVelocity(rotVelProp->Value.AsVector3);
        }
    }

    // -----------------------------------------------------------------------
    // SyncOut -- PhysX -> DataModel for dynamic bodies.
    //
    // inSyncOut = true на время обхода: CFrame-callback в SubscribeBody
    // видит этот флаг и не делает SetCFrame обратно в PhysX (избегаем цикла).
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SyncOut()
    {
        inSyncOut = true;

        for (uintptr_t key : bodyOrder)
        {
            auto bodyIt = bodies.find(key);
            if (bodyIt == bodies.end()) continue;

            PhysicsBody& body = bodyIt->second;
            if (!body.IsInitialized() || body.IsAnchored()) continue;
            if (pendingTeleports.count(key))             continue;

            Instance* inst = reinterpret_cast<Instance*>(key);

            inst->SetProperty(Classes::ShapePart::CFrame,
                PropertyValue::CFrame(body.GetCFrame()));
            inst->SetProperty(Classes::ShapePart::PosVelocity,
                PropertyValue::Vector3(body.GetLinearVelocity()));
            inst->SetProperty(Classes::ShapePart::RotVelocity,
                PropertyValue::Vector3(body.GetAngularVelocity()));
        }

        inSyncOut = false;
        pendingTeleports.clear();
    }

    // -----------------------------------------------------------------------
    // Motor6D: вычисляем и применяем мировой CFrame для Part1.
    // Выполняется отдельным проходом после основного SyncOut.
    //
    // Формула: worldCF1 = Part0.CFrame * C0 * Inv(C1)
    //
    // Запись без silent=true: рендер подписан на ShapePart::CFrame через
    // PropertyManager — без уведомления dirty-флаг меша не обновится
    // и конечности не сдвинутся визуально.
    //
    // Цикла нет: конечности Anchored=true, PhysicsBody для них не создаётся,
    // CFrame-callback из SubscribeBody для них не зарегистрирован.
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SyncMotors()
    {
        for (uintptr_t mkey : motorOrder)
        {
            auto mit = motors.find(mkey);
            if (mit == motors.end()) continue;

            Motor6DEntry& entry = mit->second;
            if (!entry.part0 || !entry.part1) continue;

            // Мировой CFrame Part0 из DataModel
            const PropertyValue* cf0prop = entry.part0->GetProperty(Classes::ShapePart::CFrame);
            if (!cf0prop || cf0prop->Type != PropertyType::CFrame) continue;
            const CFrame& worldCF0 = cf0prop->Value.AsCFrame;

            // C0 и C1 из мотора
            CFrame c0 = CFrame::FromPosition(0.0f, 0.0f, 0.0f);
            CFrame c1 = CFrame::FromPosition(0.0f, 0.0f, 0.0f);

            const PropertyValue* c0prop = entry.motorInst->GetProperty(Classes::Motor6D::C0);
            if (c0prop && c0prop->Type == PropertyType::CFrame)
                c0 = c0prop->Value.AsCFrame;

            const PropertyValue* c1prop = entry.motorInst->GetProperty(Classes::Motor6D::C1);
            if (c1prop && c1prop->Type == PropertyType::CFrame)
                c1 = c1prop->Value.AsCFrame;

            // worldCF1 = worldCF0 * C0 * Inv(C1)
            CFrame worldCF1 = worldCF0 * c0 * CFrameInverse(c1);

            entry.part1->SetProperty(Classes::ShapePart::CFrame,
                PropertyValue::CFrame(worldCF1));
        }
    }

    // -----------------------------------------------------------------------
    // LockUpright
    // -----------------------------------------------------------------------
    void PhysicsBridge::LockUpright(Instance& inst)
    {
        if (!m_impl || !m_impl->initialized) return;
        uintptr_t key = reinterpret_cast<uintptr_t>(&inst);
        auto it = m_impl->bodies.find(key);
        if (it == m_impl->bodies.end()) return;
        it->second.LockUpright();
    }

    // -----------------------------------------------------------------------
    // ApplyImpulse
    // -----------------------------------------------------------------------
    void PhysicsBridge::ApplyImpulse(Instance& inst, const Vector3& force)
    {
        if (!m_impl || !m_impl->initialized) return;

        uintptr_t key = reinterpret_cast<uintptr_t>(&inst);
        auto it = m_impl->bodies.find(key);
        if (it == m_impl->bodies.end()) return;

        PhysicsBody& body = it->second;
        if (!body.IsInitialized() || body.IsAnchored()) return;

        body.SetLinearVelocity(Vector3(
            body.GetLinearVelocity().X + force.X,
            body.GetLinearVelocity().Y + force.Y,
            body.GetLinearVelocity().Z + force.Z
        ));
    }

    // -----------------------------------------------------------------------
    // Raycast -- cast a ray from origin in direction for up to maxDist studs.
    //
    // Uses PxScene::raycast with PxHitFlag::eDEFAULT (position + normal + distance).
    // ignoreInst: if non-null, the PhysicsBody that belongs to this Instance
    //             is filtered out so a body can ray-cast from inside itself.
    //
    // Typical use: ground check from the bottom of HumanoidRootPart.
    //   origin    = hrp center - (0, halfHeight, 0)
    //   direction = (0, -1, 0)
    //   maxDist   = 0.1 studs
    // -----------------------------------------------------------------------
    RaycastResult PhysicsBridge::Raycast(const Vector3& origin,
                                         const Vector3& direction,
                                         float          maxDist,
                                         Instance*      ignoreInst) const
    {
        RaycastResult result;
        if (!m_impl || !m_impl->initialized) return result;

        px::PxScene* scene = m_impl->world.GetScene();
        if (!scene) return result;

        // Normalize direction
        px::PxVec3 dir(direction.X, direction.Y, direction.Z);
        float len = dir.magnitude();
        if (len < 1e-6f) return result;
        dir /= len;

        px::PxVec3 orig(origin.X, origin.Y, origin.Z);

        // Build filter data to optionally ignore the caller's own actor
        px::PxQueryFilterData filterData;
        filterData.flags = px::PxQueryFlag::eSTATIC | px::PxQueryFlag::eDYNAMIC;

        // Custom filter callback to skip ignoreInst's actor.
        // preFilter runs before the intersection test and returns eNONE to skip.
        // postFilter is required by the interface but never called since we use
        // only ePREFILTER (not ePOSTFILTER) — it just returns eBLOCK as a no-op.
        struct IgnoreActorFilter : px::PxQueryFilterCallback
        {
            px::PxRigidActor* ignored = nullptr;

            px::PxQueryHitType::Enum preFilter(
                const px::PxFilterData&,
                const px::PxShape*,
                const px::PxRigidActor* actor,
                px::PxHitFlags&) override
            {
                if (ignored && actor == ignored)
                    return px::PxQueryHitType::eNONE;
                return px::PxQueryHitType::eBLOCK;
            }

            px::PxQueryHitType::Enum postFilter(
                const px::PxFilterData&,
                const px::PxQueryHit&,
                const px::PxShape*,
                const px::PxRigidActor*) override
            {
                return px::PxQueryHitType::eBLOCK;
            }
        } filter;

        // Resolve the actor to ignore
        if (ignoreInst)
        {
            uintptr_t key = reinterpret_cast<uintptr_t>(ignoreInst);
            auto it = m_impl->bodies.find(key);
            if (it != m_impl->bodies.end() && it->second.IsInitialized())
                filter.ignored = it->second.GetActor();
        }

        px::PxRaycastBuffer hit;
        bool status;

        if (filter.ignored)
        {
            filterData.flags |= px::PxQueryFlag::ePREFILTER;
            status = scene->raycast(orig, dir, maxDist, hit,
                                    px::PxHitFlag::eDEFAULT,
                                    filterData, &filter);
        }
        else
        {
            status = scene->raycast(orig, dir, maxDist, hit,
                                    px::PxHitFlag::eDEFAULT,
                                    filterData);
        }

        if (status && hit.hasBlock)
        {
            result.Hit      = true;
            result.Distance = hit.block.distance;
            result.Position = Vector3(hit.block.position.x,
                                      hit.block.position.y,
                                      hit.block.position.z);
            result.Normal   = Vector3(hit.block.normal.x,
                                      hit.block.normal.y,
                                      hit.block.normal.z);
        }
        return result;
    }

} // namespace Sunvoltum
