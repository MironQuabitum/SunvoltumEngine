#include "PhysicsBridge.h"
#include "PhysicsManager.h"
#include "PhysicsWorld.h"
#include "PhysicsBody.h"
#include "../Core/Engine.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceClasses/BasePart.h"
#include "../DataModel/InstanceClasses/ShapePart.h"
#include "../DataModel/InstanceClasses/JointInstance.h"
#include "../DataModel/InstanceClasses/Weld.h"
#include "../DataModel/InstanceClasses/Motor6D.h"
#include "../DataModel/InstanceClasses/Humanoid.h"
#include "../DataModel/InstanceClasses/Workspace.h"
#include "../DataModel/InstanceClasses/Model.h"
#include "../DataModel/InstanceClasses/Folder.h"
#include "../DataModel/InstanceRegistry.h"
#include "../DataModel/PropertyValue.h"
#include "../DataModel/PropertyManager.h"
#include "../DataModel/InstanceParent.h"
#include "../Types/RaycastResult.h"
#include "../Types/CFrameUtils.h"
#include "../Types/Matrix3x3.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <cmath>

namespace Sunvoltum {

    static float GravityToPhysX(float studsPerSecSq)
    {
        return -std::abs(studsPerSecSq);
    }

    // -----------------------------------------------------------------------
    // Локальный конвертер CFrame → PxTransform (дублирует PhysicsBody::ToPxTransform,
    // но тот приватный, поэтому держим свой здесь).
    // -----------------------------------------------------------------------
    static px::PxTransform CFrameToPx(const CFrame& cf)
    {
        const Vector3&   p = cf.Position;
        const Matrix3x3& r = cf.Rotation;
        px::PxMat33 mat(
            px::PxVec3(r.R00, r.R10, r.R20),
            px::PxVec3(r.R01, r.R11, r.R21),
            px::PxVec3(r.R02, r.R12, r.R22)
        );
        px::PxQuat q(mat);
        q.normalize();
        return px::PxTransform(px::PxVec3(p.X, p.Y, p.Z), q);
    }

    // -----------------------------------------------------------------------
    // Создать PxFixedJoint между двумя телами по формуле Weld.
    //
    // Joint-фреймы задаются в мировых координатах (PxFixedJointCreate принимает
    // localFrame0 и localFrame1 относительно соответствующих акторов):
    //
    //   attachment_world = Part0.CFrame * C0
    //   localFrame0      = Inverse(Part0.CFrame) * attachment_world  = C0
    //   localFrame1      = Inverse(Part1.CFrame) * attachment_world
    //                    = Inverse(Part1.CFrame) * Part0.CFrame * C0
    //
    // При правильно выставленных C0/C1 (как в SyncJoints):
    //   Part1.CFrame = Part0.CFrame * C0 * Inverse(C1)
    // значит localFrame1 = C1.
    // -----------------------------------------------------------------------
    static px::PxFixedJoint* CreateFixedJoint(
        px::PxPhysics* physics,
        px::PxRigidActor* actor0, const CFrame& cf0, const CFrame& c0,
        px::PxRigidActor* actor1, const CFrame& c1)
    {
        px::PxTransform localFrame0 = CFrameToPx(c0);
        px::PxTransform localFrame1 = CFrameToPx(c1);

        px::PxFixedJoint* joint = PxFixedJointCreate(
            *physics,
            actor0, localFrame0,
            actor1, localFrame1
        );

        if (joint)
        {
            // Отключаем collision между связанными телами — иначе они будут
            // сами себя выталкивать при начальном перекрытии.
            joint->setConstraintFlag(px::PxConstraintFlag::eCOLLISION_ENABLED, false);
        }

        return joint;
    }

    // -----------------------------------------------------------------------
    // CreateMotorJoint — создаёт PxD6Joint для Motor6D.
    //
    // Конфигурация: все 6 степеней свободы заблокированы кроме eTWIST (вращение
    // вокруг X-оси local frame). Привод eTWIST_DRIVE задаёт скоростной режим:
    //   driveVelocity = sign(DesiredAngle - CurrentAngle) * MaxVelocity
    // Это повторяет поведение Roblox Motor6D — мотор крутится с постоянной
    // скоростью MaxVelocity пока не достигнет DesiredAngle, затем останавливается.
    //
    // Возвращает PxD6Joint* (приводится к PxJoint* и хранится в JointEntry).
    // -----------------------------------------------------------------------
    static px::PxD6Joint* CreateMotorJoint(
        px::PxPhysics* physics,
        px::PxRigidActor* actor0, const CFrame& c0,
        px::PxRigidActor* actor1, const CFrame& c1)
    {
        px::PxTransform localFrame0 = CFrameToPx(c0);
        px::PxTransform localFrame1 = CFrameToPx(c1);

        px::PxD6Joint* joint = PxD6JointCreate(
            *physics,
            actor0, localFrame0,
            actor1, localFrame1
        );

        if (!joint) return nullptr;

        // Блокируем все линейные и угловые степени свободы...
        joint->setMotion(px::PxD6Axis::eX,     px::PxD6Motion::eLOCKED);
        joint->setMotion(px::PxD6Axis::eY,     px::PxD6Motion::eLOCKED);
        joint->setMotion(px::PxD6Axis::eZ,     px::PxD6Motion::eLOCKED);
        joint->setMotion(px::PxD6Axis::eSWING1, px::PxD6Motion::eLOCKED);
        joint->setMotion(px::PxD6Axis::eSWING2, px::PxD6Motion::eLOCKED);
        // ...кроме eTWIST — вращение вокруг X-оси
        joint->setMotion(px::PxD6Axis::eTWIST, px::PxD6Motion::eFREE);

        // Настраиваем скоростной привод на оси TWIST.
        // Стiffness=0, damping=1 — чистый velocity drive (нет пружины).
        // forceLimit = большое число — не ограничиваем усилие.
        px::PxD6JointDrive drive(
            /*stiffness=*/  0.0f,
            /*damping=*/    1.0f,
            /*forceLimit=*/ PX_MAX_F32,
            /*isAcceleration=*/ false
        );
        joint->setDrive(px::PxD6Drive::eTWIST, drive);

        // Отключаем коллизию между связанными телами.
        joint->setConstraintFlag(px::PxConstraintFlag::eCOLLISION_ENABLED, false);

        return joint;
    }


    // -----------------------------------------------------------------------
    struct BodySubscriptions
    {
        PropertyToken anchored;
        PropertyToken canCollide;
        PropertyToken cframe;
        PropertyToken size;
        PropertyToken shape;
    };

    // -----------------------------------------------------------------------
    // Данные одного Joint-сустава (Weld, Motor6D и т.д.).
    //
    // joint хранит базовый PxJoint*. Конкретный тип сустава определяется
    // classId инстанса (CLASS_WELD → PxFixedJoint, CLASS_MOTOR6D → PxRevoluteJoint / PxD6Joint).
    // Приводить к нужному типу следует через joint->getConcreteType() или
    // через classId самого jointInst.
    // -----------------------------------------------------------------------
    struct JointEntry
    {
        Instance*     jointInst = nullptr; // сам Joint-инстанс (Weld, Motor6D, ...)
        Instance*     part0     = nullptr; // кэш Part0 (обновляется при изменении свойства)
        Instance*     part1     = nullptr; // кэш Part1
        px::PxJoint*  joint     = nullptr; // PhysX сустав (nullptr если не создан)
    };

    // -----------------------------------------------------------------------
    // Подписки на свойства одного Joint-инстанса.
    // -----------------------------------------------------------------------
    struct JointSubscriptions
    {
        PropertyToken part0;
        PropertyToken part1;
        PropertyToken c0;
        PropertyToken c1;
        PropertyToken enabled;
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
        std::unordered_map<uintptr_t, PhysicsBody>        bodies;
        std::vector<uintptr_t>                            bodyOrder;
        std::unordered_map<uintptr_t, BodySubscriptions>  bodyTokens;

        PropertyToken            gravityToken;
        ChildAddedToken          workspaceChildToken;
        std::vector<ChildAddedToken> containerTokens;

        // Телепорты dynamic тел в текущем тике
        std::unordered_set<uintptr_t> pendingTeleports;

        // Суставы (Weld, Motor6D, ...): ключ — reinterpret_cast<uintptr_t>(Joint Instance*)
        std::unordered_map<uintptr_t, JointEntry>         joints;
        std::vector<uintptr_t>                            jointOrder;
        std::unordered_map<uintptr_t, JointSubscriptions> jointTokens;

        // true пока выполняется SyncOut
        bool inSyncOut   = false;
        bool initialized = false;

        void RegisterBody(Instance* inst);
        void RegisterBodyRecursive(Instance* inst);
        void SubscribeBody(Instance* inst);
        void SubscribeWorkspace();
        void SyncIn();
        void SyncOut();

        void RegisterJoint(Instance* inst);
        void SubscribeJoint(Instance* inst);
        void SyncJointsDrive(); // до world.Step: применяет DriveVelocity для Motor6D
        void SyncJointsRead();  // после world.Step: читает CurrentAngle из PhysX

        // -----------------------------------------------------------------------
        // Humanoid-записи: per-Humanoid runtime-состояние.
        // Ключ — reinterpret_cast<uintptr_t>(humanoidInst*).
        // -----------------------------------------------------------------------
        struct HumanoidEntry
        {
            Instance* humanoidInst = nullptr; // сам Humanoid
            Instance* hrpInst      = nullptr; // HumanoidRootPart (братья в Model)
            float     facingYaw    = 0.0f;    // текущий угол поворота тела (рад)
        };

        std::unordered_map<uintptr_t, HumanoidEntry> humanoids;

        void RegisterHumanoid(Instance* humanoidInst);
        void SyncHumanoidTurn(float dt); // плавный поворот HRP к MoveDirection
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

        m_impl->workspaceChildToken.Disconnect();
        m_impl->containerTokens.clear();
        m_impl->bodyTokens.clear();
        m_impl->gravityToken.Disconnect();
        m_impl->pendingTeleports.clear();
        m_impl->bodyOrder.clear();

        m_impl->jointTokens.clear();
        m_impl->humanoids.clear();

        // Освобождаем PxJoint'ы до очистки тел
        for (auto& [key, entry] : m_impl->joints)
        {
            if (entry.joint)
            {
                entry.joint->release();
                entry.joint = nullptr;
            }
        }
        m_impl->joints.clear();
        m_impl->jointOrder.clear();

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
    // Step: SyncIn -> SyncJointsDrive -> simulate -> SyncOut -> SyncJointsRead
    // -----------------------------------------------------------------------
    void PhysicsBridge::Step(float dt)
    {
        if (!m_impl->initialized) return;

        m_impl->SyncIn();
        m_impl->SyncHumanoidTurn(dt); // поворот HRP к MoveDirection — до симуляции
        m_impl->SyncJointsDrive();
        m_impl->world.Step(dt);
        m_impl->SyncOut();
        m_impl->SyncJointsRead();
    }

    // -----------------------------------------------------------------------
    // RegisterBodyRecursive
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::RegisterBodyRecursive(Instance* inst)
    {
        if (!inst) return;

        const int8_t cls = inst->GetClassId();

        if (Classes::IsBasePart(cls))
        {
            RegisterBody(inst);
            return;
        }

        if (Classes::IsJoint(cls))
        {
            RegisterJoint(inst);
            return;
        }

        // Humanoid — не BasePart и не Joint, но нужно зарегистрировать
        if (cls == Classes::CLASS_HUMANOID)
        {
            RegisterHumanoid(inst);
            return;
        }

        bool isContainer = (cls == Classes::CLASS_MODEL)
                        || (cls == Classes::CLASS_FOLDER)
                        || (cls == Classes::Workspace::ClassId);
        if (!isContainer) return;

        for (auto& child : inst->GetChildren())
            RegisterBodyRecursive(child.get());

        containerTokens.push_back(inst->SubscribeChildAdded(
            [this, inst](Instance& child)
            {
                RegisterBodyRecursive(&child);

                // Если в Model добавляется Humanoid — ищем HumanoidRootPart среди
                // уже зарегистрированных братьев и применяем LockUpright.
                if (child.GetClassId() == Classes::CLASS_HUMANOID &&
                    inst->GetClassId() == Classes::CLASS_MODEL)
                {
                    for (auto& sibling : inst->GetChildren())
                    {
                        if (sibling->GetName() == "HumanoidRootPart" &&
                            Classes::IsBasePart(sibling->GetClassId()))
                        {
                            uintptr_t hrpKey = reinterpret_cast<uintptr_t>(sibling.get());
                            auto it = bodies.find(hrpKey);
                            if (it != bodies.end() && it->second.IsInitialized() &&
                                !it->second.IsAnchored())
                            {
                                it->second.LockUpright();
                                std::cout << "[PhysicsBridge] LockUpright applied via"
                                             " late Humanoid to HumanoidRootPart\n";
                            }
                        }
                    }
                    RegisterHumanoid(&child);
                }
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

        std::cout << "[PhysicsBridge] SubscribeWorkspace: ws.children="
                  << ws->GetChildren().size() << "\n";

        RegisterBodyRecursive(ws);
    }

    // -----------------------------------------------------------------------
    // FindHumanoidInParent — ищет Humanoid-инстанс среди братьев inst.
    //
    // Используется в RegisterBody: если Part с именем "HumanoidRootPart"
    // регистрируется в Model, которая содержит Humanoid, значит этот Part
    // является корневым телом персонажа и должен быть заблокирован по осям X/Z.
    // -----------------------------------------------------------------------
    static Instance* FindHumanoidInParent(Instance* inst)
    {
        if (!inst) return nullptr;
        InstanceParent* parent = inst->GetParent();
        if (!parent) return nullptr;
        Instance* parentInst = dynamic_cast<Instance*>(parent);
        if (!parentInst || parentInst->GetClassId() != Classes::CLASS_MODEL) return nullptr;
        for (auto& child : parentInst->GetChildren())
        {
            if (child->GetClassId() == Classes::CLASS_HUMANOID)
                return child.get();
        }
        return nullptr;
    }

    // -----------------------------------------------------------------------
    // RegisterBody
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::RegisterBody(Instance* inst)
    {
        uintptr_t key = reinterpret_cast<uintptr_t>(inst);
        if (bodies.count(key)) return;

        px::PxPhysics* physics = manager.GetPhysics();
        px::PxScene*   scene   = world.GetScene();

        CFrame  cf        = {};
        Vector3 size      = { 1.0f, 1.0f, 1.0f };
        Shape   shape     = Shape::Block;
        bool    anchored  = false;
        bool    canCollide = true;

        auto* cfProp = inst->GetProperty(Classes::BasePart::CFrame);
        if (cfProp && cfProp->Type == PropertyType::CFrame)
            cf = cfProp->Value.AsCFrame;

        auto* sizeProp = inst->GetProperty(Classes::BasePart::Size);
        if (sizeProp && sizeProp->Type == PropertyType::Vector3)
            size = sizeProp->Value.AsVector3;

        auto* shapeProp = inst->GetProperty(Classes::BasePart::Shape);
        if (shapeProp && shapeProp->Type == PropertyType::Shape)
            shape = shapeProp->Value.AsShape;

        auto* anchorProp = inst->GetProperty(Classes::BasePart::Anchored);
        if (anchorProp && anchorProp->Type == PropertyType::Bool)
            anchored = anchorProp->Value.AsBool;

        auto* canCollideProp = inst->GetProperty(Classes::BasePart::CanCollide);
        if (canCollideProp && canCollideProp->Type == PropertyType::Bool)
            canCollide = canCollideProp->Value.AsBool;

        PhysicsBody body;
        if (!body.Init(physics, scene, cf, size, shape, anchored, canCollide, false))
            return;

        if (!anchored)
        {
            auto* posVelProp = inst->GetProperty(Classes::BasePart::PosVelocity);
            if (posVelProp && posVelProp->Type == PropertyType::Vector3)
                body.SetLinearVelocity(posVelProp->Value.AsVector3);

            auto* rotVelProp = inst->GetProperty(Classes::BasePart::RotVelocity);
            if (rotVelProp && rotVelProp->Type == PropertyType::Vector3)
                body.SetAngularVelocity(rotVelProp->Value.AsVector3);
        }

        bodies.emplace(key, std::move(body));
        bodyOrder.push_back(key);

        SubscribeBody(inst);

        // Если это HumanoidRootPart и в родительской Model есть Humanoid —
        // автоматически блокируем вращение по осям X/Z (персонаж не заваливается).
        if (inst->GetName() == "HumanoidRootPart" && !anchored)
        {
            if (FindHumanoidInParent(inst))
            {
                auto it = bodies.find(key);
                if (it != bodies.end())
                {
                    it->second.LockUpright();
                    std::cout << "[PhysicsBridge] LockUpright applied to HumanoidRootPart: "
                              << inst->GetName() << "\n";
                }
            }
        }

        std::cout << "[PhysicsBridge] RegisterBody: " << inst->GetName()
                  << " anchored=" << anchored << "\n";
    }

    // -----------------------------------------------------------------------
    // SubscribeBody
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SubscribeBody(Instance* inst)
    {
        uintptr_t key = reinterpret_cast<uintptr_t>(inst);
        auto& subs = bodyTokens[key];

        px::PxPhysics* physics = manager.GetPhysics();
        px::PxScene*   scene   = world.GetScene();

        subs.anchored = PropertyManager::Get().Subscribe(
            inst, Classes::BasePart::Anchored,
            [this, key, physics, scene](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Bool) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                bool nowAnchored = val.Value.AsBool;
                if (nowAnchored != it->second.IsAnchored())
                    it->second.SetAnchored(nowAnchored, physics, scene);
            });

        subs.canCollide = PropertyManager::Get().Subscribe(
            inst, Classes::BasePart::CanCollide,
            [this, key](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Bool) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                it->second.SetCanCollide(val.Value.AsBool);
            });

        subs.cframe = PropertyManager::Get().Subscribe(
            inst, Classes::BasePart::CFrame,
            [this, key](const PropertyValue& val)
            {
                if (inSyncOut) return;
                if (val.Type != PropertyType::CFrame) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                if (it->second.IsKinematic()) return;
                it->second.SetCFrame(val.Value.AsCFrame);
                if (!it->second.IsAnchored())
                    pendingTeleports.insert(key);
            });

        subs.size = PropertyManager::Get().Subscribe(
            inst, Classes::BasePart::Size,
            [this, key](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Vector3) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                it->second.Resize(val.Value.AsVector3);
            });

        subs.shape = PropertyManager::Get().Subscribe(
            inst, Classes::BasePart::Shape,
            [this, key, physics, scene](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Shape) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                it->second.Reshape(val.Value.AsShape, physics, scene);
            });
    }

    // -----------------------------------------------------------------------
    // CreateJointConstraint — создаёт PhysX-сустав нужного типа.
    //
    // Диспетчер по classId:
    //   CLASS_WELD    → PxFixedJoint   (жёсткое соединение)
    //   CLASS_MOTOR6D → PxD6Joint      (вращательный привод по оси eTWIST)
    //
    // Возвращает базовый PxJoint* (владение передаётся JointEntry).
    // -----------------------------------------------------------------------
    static px::PxJoint* CreateJointConstraint(
        int8_t classId,
        px::PxPhysics* physics,
        px::PxRigidActor* actor0, const CFrame& cf0, const CFrame& c0,
        px::PxRigidActor* actor1, const CFrame& c1)
    {
        using namespace Classes;

        if (classId == CLASS_WELD)
        {
            // Weld → PxFixedJoint
            return CreateFixedJoint(physics, actor0, cf0, c0, actor1, c1);
        }

        if (classId == CLASS_MOTOR6D)
        {
            // Motor6D → PxD6Joint с velocity drive по оси eTWIST.
            // cf0 не нужен: local frames уже кодируют attachment в пространстве акторов.
            return CreateMotorJoint(physics, actor0, c0, actor1, c1);
        }

        return nullptr;
    }

    // -----------------------------------------------------------------------
    // RegisterJoint — регистрирует любой Joint-инстанс (Weld, Motor6D, ...).
    //
    // Создаёт соответствующий PhysX-сустав через CreateJointConstraint.
    // Part0 и Part1 остаются dynamic телами — гравитация работает на обоих,
    // ноги стула касаются пола и передают constraint через сустав на сидение.
    // Коллизии между Part0 и Part1 отключены на уровне сустава.
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::RegisterJoint(Instance* inst)
    {
        uintptr_t key = reinterpret_cast<uintptr_t>(inst);
        if (joints.count(key)) return;

        const int8_t classId = inst->GetClassId();

        JointEntry entry;
        entry.jointInst = inst;

        auto* p0prop = inst->GetProperty(Classes::JointInstance::Part0);
        if (p0prop && p0prop->Type == PropertyType::InstanceRef)
            entry.part0 = p0prop->Value.AsInstanceRef;

        auto* p1prop = inst->GetProperty(Classes::JointInstance::Part1);
        if (p1prop && p1prop->Type == PropertyType::InstanceRef)
            entry.part1 = p1prop->Value.AsInstanceRef;

        std::cout << "[PhysicsBridge] RegisterJoint (" << inst->GetName()
                  << "): Part0=" << (entry.part0 ? entry.part0->GetName() : "null")
                  << " Part1=" << (entry.part1 ? entry.part1->GetName() : "null")
                  << "\n";

        // Создаём PhysX-сустав если оба тела зарегистрированы.
        if (entry.part0 && entry.part1)
        {
            uintptr_t p0key = reinterpret_cast<uintptr_t>(entry.part0);
            uintptr_t p1key = reinterpret_cast<uintptr_t>(entry.part1);
            auto b0it = bodies.find(p0key);
            auto b1it = bodies.find(p1key);

            if (b0it != bodies.end() && b0it->second.IsInitialized() &&
                b1it != bodies.end() && b1it->second.IsInitialized())
            {
                static const CFrame kIdentity = CFrame::FromPosition(0.0f, 0.0f, 0.0f);
                CFrame cf0 = kIdentity, c0 = kIdentity, c1 = kIdentity;

                auto* cf0prop = entry.part0->GetProperty(Classes::BasePart::CFrame);
                if (cf0prop && cf0prop->Type == PropertyType::CFrame)
                    cf0 = cf0prop->Value.AsCFrame;

                auto* c0prop = inst->GetProperty(Classes::JointInstance::C0);
                if (c0prop && c0prop->Type == PropertyType::CFrame)
                    c0 = c0prop->Value.AsCFrame;
                auto* c1prop = inst->GetProperty(Classes::JointInstance::C1);
                if (c1prop && c1prop->Type == PropertyType::CFrame)
                    c1 = c1prop->Value.AsCFrame;

                entry.joint = CreateJointConstraint(
                    classId,
                    manager.GetPhysics(),
                    b0it->second.GetActor(), cf0, c0,
                    b1it->second.GetActor(), c1
                );

                if (entry.joint)
                    std::cout << "[PhysicsBridge] RegisterJoint: PxJoint created for "
                              << inst->GetName() << "\n";
                else
                    std::cerr << "[PhysicsBridge] RegisterJoint: joint creation failed for "
                              << inst->GetName() << "\n";
            }
            else
            {
                std::cout << "[PhysicsBridge] RegisterJoint: bodies not ready for "
                          << inst->GetName()
                          << " b0=" << (b0it != bodies.end())
                          << " b1=" << (b1it != bodies.end()) << "\n";
            }
        }

        joints.emplace(key, std::move(entry));
        jointOrder.push_back(key);

        SubscribeJoint(inst);
    }

    // -----------------------------------------------------------------------
    // SubscribeJoint — реактивные подписки на Part0, Part1, C0, C1, Enabled.
    //
    // Единый для всех типов суставов: при смене Part1 пересоздаём сустав
    // через тот же диспетчер CreateJointConstraint, который знает classId.
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SubscribeJoint(Instance* inst)
    {
        uintptr_t key = reinterpret_cast<uintptr_t>(inst);
        auto& subs = jointTokens[key];

        subs.part0 = PropertyManager::Get().Subscribe(
            inst, Classes::JointInstance::Part0,
            [this, key](const PropertyValue& val)
            {
                auto it = joints.find(key);
                if (it == joints.end()) return;
                it->second.part0 = (val.Type == PropertyType::InstanceRef)
                                   ? val.Value.AsInstanceRef : nullptr;
            });

        subs.part1 = PropertyManager::Get().Subscribe(
            inst, Classes::JointInstance::Part1,
            [this, key](const PropertyValue& val)
            {
                auto it = joints.find(key);
                if (it == joints.end()) return;

                // Освобождаем старый joint перед сменой Part1
                if (it->second.joint)
                {
                    it->second.joint->release();
                    it->second.joint = nullptr;
                }

                it->second.part1 = (val.Type == PropertyType::InstanceRef)
                                   ? val.Value.AsInstanceRef : nullptr;

                // Пересоздаём joint если оба тела готовы
                Instance* newPart1  = it->second.part1;
                Instance* newPart0  = it->second.part0;
                Instance* jointInst = it->second.jointInst;
                if (!newPart0 || !newPart1) return;

                uintptr_t p0key = reinterpret_cast<uintptr_t>(newPart0);
                uintptr_t p1key = reinterpret_cast<uintptr_t>(newPart1);
                auto b0it = bodies.find(p0key);
                auto b1it = bodies.find(p1key);

                if (b0it != bodies.end() && b0it->second.IsInitialized() &&
                    b1it != bodies.end() && b1it->second.IsInitialized())
                {
                    static const CFrame kIdentity = CFrame::FromPosition(0.0f, 0.0f, 0.0f);
                    CFrame cf0 = kIdentity, c0 = kIdentity, c1 = kIdentity;

                    auto* cf0prop = newPart0->GetProperty(Classes::BasePart::CFrame);
                    if (cf0prop && cf0prop->Type == PropertyType::CFrame)
                        cf0 = cf0prop->Value.AsCFrame;

                    auto* c0prop = jointInst->GetProperty(Classes::JointInstance::C0);
                    if (c0prop && c0prop->Type == PropertyType::CFrame)
                        c0 = c0prop->Value.AsCFrame;
                    auto* c1prop = jointInst->GetProperty(Classes::JointInstance::C1);
                    if (c1prop && c1prop->Type == PropertyType::CFrame)
                        c1 = c1prop->Value.AsCFrame;

                    it->second.joint = CreateJointConstraint(
                        jointInst->GetClassId(),
                        manager.GetPhysics(),
                        b0it->second.GetActor(), cf0, c0,
                        b1it->second.GetActor(), c1
                    );

                    std::cout << "[PhysicsBridge] SubscribeJoint Part1 set: "
                              << newPart1->GetName()
                              << " joint=" << (it->second.joint ? "ok" : "fail") << "\n";
                }
            });

        subs.c0 = PropertyManager::Get().Subscribe(
            inst, Classes::JointInstance::C0,
            [](const PropertyValue&) { /* PhysX solver читает C0/C1 через joint frames */ });

        subs.c1 = PropertyManager::Get().Subscribe(
            inst, Classes::JointInstance::C1,
            [](const PropertyValue&) { /* PhysX solver читает C0/C1 через joint frames */ });

        subs.enabled = PropertyManager::Get().Subscribe(
            inst, Classes::JointInstance::Enabled,
            [this, key](const PropertyValue& val)
            {
                // Включение/выключение сустава — снимаем/добавляем constraint в solver
                if (val.Type != PropertyType::Bool) return;
                auto it = joints.find(key);
                if (it == joints.end() || !it->second.joint) return;
                it->second.joint->setConstraintFlag(
                    px::PxConstraintFlag::eDISABLE_CONSTRAINT, !val.Value.AsBool);
            });
    }

    // -----------------------------------------------------------------------
    // SyncJointsDrive — вызывается ДО world.Step().
    //
    // Для Motor6D: читает DesiredAngle и MaxVelocity, вычисляет DriveVelocity
    // и применяет его в PxD6Joint. PhysX решает constraint в следующем шаге.
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SyncJointsDrive()
    {
        using namespace Classes;

        for (uintptr_t key : jointOrder)
        {
            auto it = joints.find(key);
            if (it == joints.end() || !it->second.joint) continue;

            Instance* inst = it->second.jointInst;
            if (!inst || inst->GetClassId() != CLASS_MOTOR6D) continue;

            px::PxD6Joint* d6 = static_cast<px::PxD6Joint*>(it->second.joint);

            // Проверяем Enabled — если выключен, останавливаем привод
            auto* enProp = inst->GetProperty(JointInstance::Enabled);
            if (enProp && enProp->Type == PropertyType::Bool && !enProp->Value.AsBool)
            {
                d6->setDriveVelocity(px::PxVec3(0.0f), px::PxVec3(0.0f));
                continue;
            }

            auto* desiredProp = inst->GetProperty(Motor6D::DesiredAngle);
            auto* maxVelProp  = inst->GetProperty(Motor6D::MaxVelocity);
            if (!desiredProp || !maxVelProp) continue;

            float desiredAngle = (desiredProp->Type == PropertyType::Float)
                                 ? desiredProp->Value.AsFloat : 0.0f;
            float maxVelocity  = (maxVelProp->Type == PropertyType::Float)
                                 ? maxVelProp->Value.AsFloat : 0.0f;

            // Читаем текущий угол из физики для расчёта знака
            float currentAngle = d6->getTwistAngle();
            float angleDiff    = desiredAngle - currentAngle;

            float driveVel = 0.0f;
            constexpr float kEpsilon = 1e-4f;
            if (std::abs(angleDiff) > kEpsilon && maxVelocity > 0.0f)
                driveVel = (angleDiff > 0.0f ? 1.0f : -1.0f) * maxVelocity;

            // setDriveVelocity(linear, angular) — два PxVec3
            // Угловая скорость по оси X (eTWIST)
            d6->setDriveVelocity(px::PxVec3(0.0f), px::PxVec3(driveVel, 0.0f, 0.0f));
        }
    }

    // -----------------------------------------------------------------------
    // SyncJointsRead — вызывается ПОСЛЕ world.Step() и SyncOut().
    //
    // Для Motor6D: читает getTwistAngle() и записывает CurrentAngle в DataModel.
    // Это значение реплицируется клиентам через ServerReplicator (reliable channel).
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SyncJointsRead()
    {
        using namespace Classes;

        for (uintptr_t key : jointOrder)
        {
            auto it = joints.find(key);
            if (it == joints.end() || !it->second.joint) continue;

            Instance* inst = it->second.jointInst;
            if (!inst || inst->GetClassId() != CLASS_MOTOR6D) continue;

            px::PxD6Joint* d6 = static_cast<px::PxD6Joint*>(it->second.joint);
            float currentAngle = d6->getTwistAngle();

            inst->SetProperty(Motor6D::CurrentAngle, PropertyValue::Float(currentAngle));
        }
    }

    // -----------------------------------------------------------------------
    // SyncIn — velocity sync для dynamic тел
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SyncIn()
    {
        for (uintptr_t key : bodyOrder)
        {
            auto bodyIt = bodies.find(key);
            if (bodyIt == bodies.end()) continue;

            PhysicsBody& body = bodyIt->second;
            if (body.IsAnchored())  continue;
            if (body.IsKinematic()) continue;

            Instance* inst = reinterpret_cast<Instance*>(key);

            auto* posVelProp = inst->GetProperty(Classes::BasePart::PosVelocity);
            if (posVelProp && posVelProp->Type == PropertyType::Vector3)
                body.SetLinearVelocity(posVelProp->Value.AsVector3);

            auto* rotVelProp = inst->GetProperty(Classes::BasePart::RotVelocity);
            if (rotVelProp && rotVelProp->Type == PropertyType::Vector3)
                body.SetAngularVelocity(rotVelProp->Value.AsVector3);
        }
    }

    // -----------------------------------------------------------------------
    // SyncOut — PhysX → DataModel для dynamic тел
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SyncOut()
    {
        inSyncOut = true;

        static int s_syncOutLog = 0;
        const bool doLog = (s_syncOutLog < 5);

        for (uintptr_t key : bodyOrder)
        {
            auto bodyIt = bodies.find(key);
            if (bodyIt == bodies.end()) continue;

            PhysicsBody& body = bodyIt->second;
            if (!body.IsInitialized() || body.IsAnchored()) continue;
            if (pendingTeleports.count(key))                continue;
            if (body.IsKinematic())                         continue;

            Instance* inst = reinterpret_cast<Instance*>(key);

            CFrame  cf     = body.GetCFrame();
            Vector3 linVel = body.GetLinearVelocity();
            Vector3 angVel = body.GetAngularVelocity();

            if (doLog && std::string(inst->GetName()).find("Chair") != std::string::npos)
            {
                std::cout << "[SyncOut] " << inst->GetName()
                          << " physY=" << cf.Position.Y
                          << " velY=" << linVel.Y << "\n";
            }

            inst->SetProperty(Classes::BasePart::CFrame,       PropertyValue::CFrame(cf));
            inst->SetProperty(Classes::BasePart::PosVelocity,  PropertyValue::Vector3(linVel));
            inst->SetProperty(Classes::BasePart::RotVelocity,  PropertyValue::Vector3(angVel));
        }

        if (doLog) ++s_syncOutLog;

        inSyncOut = false;
        pendingTeleports.clear();
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
    // Raycast
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

        px::PxVec3 dir(direction.X, direction.Y, direction.Z);
        float len = dir.magnitude();
        if (len < 1e-6f) return result;
        dir /= len;

        px::PxVec3 orig(origin.X, origin.Y, origin.Z);

        px::PxQueryFilterData filterData;
        filterData.flags = px::PxQueryFlag::eSTATIC | px::PxQueryFlag::eDYNAMIC;

        struct IgnoreActorFilter : px::PxQueryFilterCallback
        {
            px::PxRigidActor* ignored = nullptr;

            px::PxQueryHitType::Enum preFilter(
                const px::PxFilterData&, const px::PxShape*,
                const px::PxRigidActor* actor, px::PxHitFlags&) override
            {
                if (ignored && actor == ignored)
                    return px::PxQueryHitType::eNONE;
                return px::PxQueryHitType::eBLOCK;
            }

            px::PxQueryHitType::Enum postFilter(
                const px::PxFilterData&, const px::PxQueryHit&,
                const px::PxShape*, const px::PxRigidActor*) override
            {
                return px::PxQueryHitType::eBLOCK;
            }
        } filter;

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
                                    px::PxHitFlag::eDEFAULT, filterData, &filter);
        }
        else
        {
            status = scene->raycast(orig, dir, maxDist, hit,
                                    px::PxHitFlag::eDEFAULT, filterData);
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

    // -----------------------------------------------------------------------
    // RegisterHumanoid — регистрирует Humanoid и находит его HumanoidRootPart.
    //
    // Вызывается из RegisterBodyRecursive когда обходим дерево, или из
    // SubscribeChildAdded когда Humanoid добавляется в Model позже.
    // Инициализирует FacingYaw из текущей ориентации HRP (если уже есть CFrame).
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::RegisterHumanoid(Instance* humanoidInst)
    {
        if (!humanoidInst) return;

        uintptr_t key = reinterpret_cast<uintptr_t>(humanoidInst);
        if (humanoids.count(key)) return;

        // Ищем HumanoidRootPart среди братьев (дети родительской Model)
        Instance* hrpInst = nullptr;
        InstanceParent* parent = humanoidInst->GetParent();
        if (parent)
        {
            Instance* parentInst = dynamic_cast<Instance*>(parent);
            if (parentInst && parentInst->GetClassId() == Classes::CLASS_MODEL)
            {
                for (auto& child : parentInst->GetChildren())
                {
                    if (child->GetName() == "HumanoidRootPart" &&
                        Classes::IsBasePart(child->GetClassId()))
                    {
                        hrpInst = child.get();
                        break;
                    }
                }
            }
        }

        HumanoidEntry entry;
        entry.humanoidInst = humanoidInst;
        entry.hrpInst      = hrpInst;
        entry.facingYaw    = 0.0f;

        // Инициализируем facingYaw из текущей ориентации HRP
        if (hrpInst)
        {
            auto* cfProp = hrpInst->GetProperty(Classes::BasePart::CFrame);
            if (cfProp && cfProp->Type == PropertyType::CFrame)
            {
                // Извлекаем yaw из матрицы: atan2(R02, R22) — по формуле ToEuler
                const Matrix3x3& rot = cfProp->Value.AsCFrame.Rotation;
                entry.facingYaw = std::atan2(rot.R02, rot.R22);
            }
        }

        humanoids.emplace(key, std::move(entry));

        std::cout << "[PhysicsBridge] RegisterHumanoid: "
                  << humanoidInst->GetName()
                  << " hrp=" << (hrpInst ? hrpInst->GetName() : "null")
                  << " facingYaw=" << entry.facingYaw << "\n";
    }

    // -----------------------------------------------------------------------
    // SyncHumanoidTurn — плавно поворачивает HRP в сторону MoveDirection.
    //
    // Алгоритм из main.cpp:
    //   1. Читаем MoveDirection из Humanoid.
    //   2. Если есть движение — вычисляем targetYaw = atan2(dx, dz).
    //   3. Интерполируем facingYaw → targetYaw со скоростью TURN_SPEED рад/с.
    //   4. Записываем новый CFrame в HRP: позиция из физики, ротация = FromEuler(0, facingYaw, 0).
    //   5. Обновляем Humanoid::FacingYaw для репликации на клиент.
    //
    // Вызывается в Step() между SyncIn и world.Step — PhysX увидит правильный
    // поворот уже в текущем шаге симуляции.
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::SyncHumanoidTurn(float dt)
    {
        using H  = Classes::Humanoid;
        using BP = Classes::BasePart;

        constexpr float TURN_SPEED = 14.0f; // рад/с — из main.cpp

        for (auto& [key, entry] : humanoids)
        {
            Instance* humanoid = entry.humanoidInst;
            Instance* hrp      = entry.hrpInst;
            if (!humanoid || !hrp) continue;

            // Читаем Health — не крутим мёртвого персонажа
            auto* healthProp = humanoid->GetProperty(H::Health);
            if (healthProp && healthProp->Type == PropertyType::Float &&
                healthProp->Value.AsFloat <= 0.0f)
                continue;

            // Читаем MoveDirection
            auto* mdProp = humanoid->GetProperty(H::MoveDirection);
            if (!mdProp || mdProp->Type != PropertyType::Vector3) continue;
            const Vector3& md = mdProp->Value.AsVector3;

            float moveLen = std::sqrt(md.X * md.X + md.Z * md.Z);
            if (moveLen < 1e-4f) continue; // стоит — не трогаем поворот

            // Целевой yaw из вектора движения
            float targetYaw = std::atan2(md.X, md.Z);

            // Кратчайший путь через окружность
            float diff = targetYaw - entry.facingYaw;
            while (diff >  3.14159265f) diff -= 2.0f * 3.14159265f;
            while (diff < -3.14159265f) diff += 2.0f * 3.14159265f;

            float step = TURN_SPEED * dt;
            if (std::abs(diff) <= step)
                entry.facingYaw = targetYaw;
            else
                entry.facingYaw += (diff > 0.0f ? step : -step);

            // Читаем текущую позицию HRP из DataModel (SyncIn уже применил физику)
            auto* cfProp = hrp->GetProperty(BP::CFrame);
            if (!cfProp || cfProp->Type != PropertyType::CFrame) continue;

            // Строим новый CFrame: позиция физики + чистая Y-ротация
            CFrame newCF(
                cfProp->Value.AsCFrame.Position,
                Matrix3x3::FromEuler(0.0f, entry.facingYaw, 0.0f)
            );

            // Записываем silent=true — не уведомляем ServerReplicator лишний раз
            // (CFrame пойдёт через SyncOut в следующем тике как обычно)
            hrp->SetProperty(BP::CFrame, PropertyValue::CFrame(newCF), false, true);

            // Физически применяем поворот в PhysX
            uintptr_t hrpKey = reinterpret_cast<uintptr_t>(hrp);
            auto bodyIt = bodies.find(hrpKey);
            if (bodyIt != bodies.end() && bodyIt->second.IsInitialized())
                bodyIt->second.SetCFrame(newCF);

            // Обновляем FacingYaw в DataModel для репликации
            humanoid->SetProperty(H::FacingYaw,
                PropertyValue::Float(entry.facingYaw));
        }
    }

} // namespace Sunvoltum
