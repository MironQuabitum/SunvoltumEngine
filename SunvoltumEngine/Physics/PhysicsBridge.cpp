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

    static float GravityToEngine(float studsPerSecSq)
    {
        return -std::abs(studsPerSecSq);
    }

    // -----------------------------------------------------------------------
    // Создать Joint (Weld -> FixedJoint, Motor6D -> MotorJoint)
    // -----------------------------------------------------------------------
    static std::shared_ptr<SunvoltumPhysics::Joint> CreateJointConstraint(
        int8_t classId,
        const std::shared_ptr<SunvoltumPhysics::RigidBody>& body0, const CFrame& c0,
        const std::shared_ptr<SunvoltumPhysics::RigidBody>& body1, const CFrame& c1)
    {
        using namespace Classes;

        if (!body0 || !body1) return nullptr;

        SunvoltumPhysics::Transform localFrame0 = ToPhysicsTransform(c0);
        SunvoltumPhysics::Transform localFrame1 = ToPhysicsTransform(c1);

        if (classId == CLASS_WELD)
        {
            return std::make_shared<SunvoltumPhysics::FixedJoint>(
                body0.get(), body1.get(),
                localFrame0, localFrame1
            );
        }

        if (classId == CLASS_MOTOR6D)
        {
            return std::make_shared<SunvoltumPhysics::MotorJoint>(
                body0.get(), body1.get(),
                localFrame0, localFrame1
            );
        }

        return nullptr;
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

    struct JointEntry
    {
        Instance* jointInst = nullptr;
        Instance* part0     = nullptr;
        Instance* part1     = nullptr;
        std::shared_ptr<SunvoltumPhysics::Joint> joint;
    };

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

        std::unordered_map<uintptr_t, PhysicsBody>        bodies;
        std::vector<uintptr_t>                            bodyOrder;
        std::unordered_map<uintptr_t, BodySubscriptions>  bodyTokens;

        PropertyToken            gravityToken;
        ChildAddedToken          workspaceChildToken;
        std::vector<ChildAddedToken> containerTokens;

        std::unordered_set<uintptr_t> pendingTeleports;

        std::unordered_map<uintptr_t, JointEntry>         joints;
        std::vector<uintptr_t>                            jointOrder;
        std::unordered_map<uintptr_t, JointSubscriptions> jointTokens;

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
        void SyncJointsDrive();
        void SyncJointsRead();

        struct HumanoidEntry
        {
            Instance* humanoidInst = nullptr;
            Instance* hrpInst      = nullptr;
            float     facingYaw    = 0.0f;
        };

        std::unordered_map<uintptr_t, HumanoidEntry> humanoids;

        void RegisterHumanoid(Instance* humanoidInst);
        void SyncHumanoidTurn(float dt);
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

        if (!m_impl->world.Init(m_impl->manager, GravityToEngine(gravityStuds)))
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
                        m_impl->world.SetGravity(GravityToEngine(studs));
                        std::cout << "[PhysicsBridge] Gravity -> " << studs << " studs/s^2\n";
                    });
            }
        }

        m_impl->SubscribeWorkspace();

        m_impl->initialized = true;
        std::cout << "[PhysicsBridge] Init OK with SunvoltumPhysics\n";
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

        // Удаляем Joints из SunvoltumPhysics::World
        for (auto& [key, entry] : m_impl->joints)
        {
            if (entry.joint)
            {
                m_impl->world.GetWorld().RemoveJoint(entry.joint);
                entry.joint.reset();
            }
        }
        m_impl->joints.clear();
        m_impl->jointOrder.clear();

        for (auto& [key, body] : m_impl->bodies)
            body.Shutdown(&m_impl->world);
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
    // Step
    // -----------------------------------------------------------------------
    void PhysicsBridge::Step(float dt)
    {
        if (!m_impl->initialized) return;

        m_impl->SyncIn();
        m_impl->SyncHumanoidTurn(dt);
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

        if (cls == Classes::CLASS_HUMANOID)
        {
            RegisterHumanoid(inst);
            return;
        }

        for (auto& child : inst->GetChildren())
            RegisterBodyRecursive(child.get());
    }

    void PhysicsBridge::Impl::SubscribeWorkspace()
    {
        Instance* ws = dataModel->FindByName("Workspace");
        if (!ws)
        {
            std::cerr << "[PhysicsBridge] Workspace not found in DataModel\n";
            return;
        }

        workspaceChildToken = ws->SubscribeChildAdded(
            [this](Instance& newChild)
            {
                std::cout << "[PhysicsBridge] Workspace childAdded: "
                          << newChild.GetName() << "\n";
                RegisterBodyRecursive(&newChild);

                const int8_t cls = newChild.GetClassId();
                if (cls == Classes::CLASS_MODEL || cls == Classes::CLASS_FOLDER)
                {
                    auto token = newChild.SubscribeChildAdded(
                        [this](Instance& innerChild)
                        {
                            std::cout << "[PhysicsBridge] Container childAdded: "
                                      << innerChild.GetName() << "\n";
                            RegisterBodyRecursive(&innerChild);
                        });
                    containerTokens.push_back(std::move(token));
                }
            });

        for (auto& child : ws->GetChildren())
        {
            const int8_t cls = child->GetClassId();
            if (cls == Classes::CLASS_MODEL || cls == Classes::CLASS_FOLDER)
            {
                auto token = child->SubscribeChildAdded(
                    [this](Instance& innerChild)
                    {
                        std::cout << "[PhysicsBridge] Pre-existing container childAdded: "
                                  << innerChild.GetName() << "\n";
                        RegisterBodyRecursive(&innerChild);
                    });
                containerTokens.push_back(std::move(token));
            }
        }

        std::cout << "[PhysicsBridge] SubscribeWorkspace: ws.children="
                  << ws->GetChildren().size() << "\n";

        RegisterBodyRecursive(ws);
    }

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
        if (!body.Init(&world, cf, size, shape, anchored, canCollide, false))
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

        subs.anchored = PropertyManager::Get().Subscribe(
            inst, Classes::BasePart::Anchored,
            [this, key](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Bool) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                bool nowAnchored = val.Value.AsBool;
                if (nowAnchored != it->second.IsAnchored())
                    it->second.SetAnchored(nowAnchored, &world);
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
            [this, key](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Shape) return;
                auto it = bodies.find(key);
                if (it == bodies.end()) return;
                it->second.Reshape(val.Value.AsShape);
            });
    }

    // -----------------------------------------------------------------------
    // RegisterJoint
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
                CFrame c0 = kIdentity, c1 = kIdentity;

                auto* c0prop = inst->GetProperty(Classes::JointInstance::C0);
                if (c0prop && c0prop->Type == PropertyType::CFrame)
                    c0 = c0prop->Value.AsCFrame;
                auto* c1prop = inst->GetProperty(Classes::JointInstance::C1);
                if (c1prop && c1prop->Type == PropertyType::CFrame)
                    c1 = c1prop->Value.AsCFrame;

                entry.joint = CreateJointConstraint(
                    classId,
                    b0it->second.GetRigidBody(), c0,
                    b1it->second.GetRigidBody(), c1
                );

                if (entry.joint)
                {
                    world.GetWorld().AddJoint(entry.joint);
                    std::cout << "[PhysicsBridge] RegisterJoint: Joint created for "
                              << inst->GetName() << "\n";
                }
            }
        }

        joints.emplace(key, std::move(entry));
        jointOrder.push_back(key);

        SubscribeJoint(inst);
    }

    // -----------------------------------------------------------------------
    // SubscribeJoint
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

                if (it->second.joint)
                {
                    world.GetWorld().RemoveJoint(it->second.joint);
                    it->second.joint.reset();
                }

                it->second.part1 = (val.Type == PropertyType::InstanceRef)
                                   ? val.Value.AsInstanceRef : nullptr;

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
                    CFrame c0 = kIdentity, c1 = kIdentity;

                    auto* c0prop = jointInst->GetProperty(Classes::JointInstance::C0);
                    if (c0prop && c0prop->Type == PropertyType::CFrame)
                        c0 = c0prop->Value.AsCFrame;
                    auto* c1prop = jointInst->GetProperty(Classes::JointInstance::C1);
                    if (c1prop && c1prop->Type == PropertyType::CFrame)
                        c1 = c1prop->Value.AsCFrame;

                    it->second.joint = CreateJointConstraint(
                        jointInst->GetClassId(),
                        b0it->second.GetRigidBody(), c0,
                        b1it->second.GetRigidBody(), c1
                    );

                    if (it->second.joint)
                    {
                        world.GetWorld().AddJoint(it->second.joint);
                    }

                    std::cout << "[PhysicsBridge] SubscribeJoint Part1 set: "
                              << newPart1->GetName()
                              << " joint=" << (it->second.joint ? "ok" : "fail") << "\n";
                }
            });

        subs.c0 = PropertyManager::Get().Subscribe(
            inst, Classes::JointInstance::C0,
            [this, key](const PropertyValue& val)
            {
                if (val.Type != PropertyType::CFrame) return;
                auto it = joints.find(key);
                if (it == joints.end() || !it->second.joint) return;
                it->second.joint->SetLocalFrameA(ToPhysicsTransform(val.Value.AsCFrame));
            });

        subs.c1 = PropertyManager::Get().Subscribe(
            inst, Classes::JointInstance::C1,
            [this, key](const PropertyValue& val)
            {
                if (val.Type != PropertyType::CFrame) return;
                auto it = joints.find(key);
                if (it == joints.end() || !it->second.joint) return;
                it->second.joint->SetLocalFrameB(ToPhysicsTransform(val.Value.AsCFrame));
            });

        subs.enabled = PropertyManager::Get().Subscribe(
            inst, Classes::JointInstance::Enabled,
            [this, key](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Bool) return;
                auto it = joints.find(key);
                if (it == joints.end() || !it->second.joint) return;
                it->second.joint->SetEnabled(val.Value.AsBool);
            });
    }

    // -----------------------------------------------------------------------
    // SyncJointsDrive
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

            auto motor = std::dynamic_pointer_cast<SunvoltumPhysics::MotorJoint>(it->second.joint);
            if (!motor) continue;

            auto* enProp = inst->GetProperty(JointInstance::Enabled);
            if (enProp && enProp->Type == PropertyType::Bool && !enProp->Value.AsBool)
            {
                motor->SetEnabled(false);
                continue;
            }
            motor->SetEnabled(true);

            auto* desiredProp = inst->GetProperty(Motor6D::DesiredAngle);
            auto* maxVelProp  = inst->GetProperty(Motor6D::MaxVelocity);
            if (!desiredProp || !maxVelProp) continue;

            float desiredAngle = (desiredProp->Type == PropertyType::Float)
                                 ? desiredProp->Value.AsFloat : 0.0f;
            float maxVelocity  = (maxVelProp->Type == PropertyType::Float)
                                 ? maxVelProp->Value.AsFloat : 0.0f;

            motor->SetDesiredAngle(desiredAngle);
            motor->SetMaxVelocity(maxVelocity);
        }
    }

    // -----------------------------------------------------------------------
    // SyncJointsRead
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

            auto motor = std::dynamic_pointer_cast<SunvoltumPhysics::MotorJoint>(it->second.joint);
            if (!motor) continue;

            float currentAngle = motor->GetCurrentAngle();
            inst->SetProperty(Motor6D::CurrentAngle, PropertyValue::Float(currentAngle));
        }
    }

    // -----------------------------------------------------------------------
    // SyncIn
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
    // SyncOut
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

        SunvoltumPhysics::Vector3 pOrigin = ToPhysicsVec3(origin);
        SunvoltumPhysics::Vector3 pDir    = ToPhysicsVec3(direction);

        const SunvoltumPhysics::RigidBody* ignoreBody = nullptr;
        if (ignoreInst)
        {
            uintptr_t key = reinterpret_cast<uintptr_t>(ignoreInst);
            auto it = m_impl->bodies.find(key);
            if (it != m_impl->bodies.end() && it->second.IsInitialized())
                ignoreBody = it->second.GetRigidBody().get();
        }

        SunvoltumPhysics::RaycastHit hit;
        if (m_impl->world.GetWorld().Raycast(pOrigin, pDir, maxDist, hit, ignoreBody))
        {
            result.Hit      = true;
            result.Distance = hit.distance;
            result.Position = FromPhysicsVec3(hit.point);
            result.Normal   = FromPhysicsVec3(hit.normal);
        }

        return result;
    }

    // -----------------------------------------------------------------------
    // Humanoid methods
    // -----------------------------------------------------------------------
    void PhysicsBridge::Impl::RegisterHumanoid(Instance* humanoidInst)
    {
        if (!humanoidInst) return;

        uintptr_t key = reinterpret_cast<uintptr_t>(humanoidInst);
        if (humanoids.count(key)) return;

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

        if (hrpInst)
        {
            auto* cfProp = hrpInst->GetProperty(Classes::BasePart::CFrame);
            if (cfProp && cfProp->Type == PropertyType::CFrame)
            {
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

    void PhysicsBridge::Impl::SyncHumanoidTurn(float dt)
    {
        using H  = Classes::Humanoid;
        using BP = Classes::BasePart;

        constexpr float TURN_SPEED = 14.0f;

        for (auto& [key, entry] : humanoids)
        {
            Instance* humanoid = entry.humanoidInst;
            Instance* hrp      = entry.hrpInst;
            if (!humanoid || !hrp) continue;

            auto* healthProp = humanoid->GetProperty(H::Health);
            if (healthProp && healthProp->Type == PropertyType::Float &&
                healthProp->Value.AsFloat <= 0.0f)
                continue;

            auto* mdProp = humanoid->GetProperty(H::MoveDirection);
            if (!mdProp || mdProp->Type != PropertyType::Vector3) continue;
            const Vector3& md = mdProp->Value.AsVector3;

            float moveLen = std::sqrt(md.X * md.X + md.Z * md.Z);
            if (moveLen < 1e-4f) continue;

            float targetYaw = std::atan2(md.X, md.Z);

            float diff = targetYaw - entry.facingYaw;
            while (diff >  3.14159265f) diff -= 2.0f * 3.14159265f;
            while (diff < -3.14159265f) diff += 2.0f * 3.14159265f;

            float step = TURN_SPEED * dt;
            if (std::abs(diff) <= step)
                entry.facingYaw = targetYaw;
            else
                entry.facingYaw += (diff > 0.0f ? step : -step);

            auto* cfProp = hrp->GetProperty(BP::CFrame);
            if (!cfProp || cfProp->Type != PropertyType::CFrame) continue;

            CFrame newCF(
                cfProp->Value.AsCFrame.Position,
                Matrix3x3::FromEuler(0.0f, entry.facingYaw, 0.0f)
            );

            hrp->SetProperty(BP::CFrame, PropertyValue::CFrame(newCF), false, true);

            uintptr_t hrpKey = reinterpret_cast<uintptr_t>(hrp);
            auto bodyIt = bodies.find(hrpKey);
            if (bodyIt != bodies.end() && bodyIt->second.IsInitialized())
                bodyIt->second.SetCFrame(newCF);

            humanoid->SetProperty(H::FacingYaw,
                PropertyValue::Float(entry.facingYaw));
        }
    }

} // namespace Sunvoltum
