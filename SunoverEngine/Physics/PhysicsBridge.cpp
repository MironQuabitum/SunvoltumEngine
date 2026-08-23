#include "PhysicsBridge.h"
#include "PhysicsManager.h"
#include "PhysicsWorld.h"
#include "PhysicsBody.h"
#include "../Core/Engine.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceClasses/ShapePart.h"
#include "../DataModel/InstanceClasses/Workspace.h"
#include "../DataModel/PropertyValue.h"
#include <unordered_map>
#include <cmath>
#include <iostream>

namespace Sunover {

    // Единица длины движка — stud.
    // 1 stud = 0.05 м  →  коэффициент пересчёта studs/s² → m/s²
    // Пример: Workspace.Gravity = 196.2 studs/s²
    //         196.2 * 0.05 = 9.81 m/s²  (стандартная Земля)
    static constexpr float STUDS_TO_METERS = 0.05f;

    // Конвертирует значение Workspace.Gravity (studs/s², всегда положительное)
    // в вектор гравитации PhysX (m/s², направленный вниз).
    static float GravityToPhysX(float studsPerSecSq)
    {
        return -std::abs(studsPerSecSq) * STUDS_TO_METERS;
    }

    // -----------------------------------------------------------------------
    // pImpl — все PhysX-зависимые поля здесь
    // -----------------------------------------------------------------------

    struct PhysicsBridge::Impl
    {
        Engine*    engine    = nullptr;
        DataModel* dataModel = nullptr;

        PhysicsManager manager;
        PhysicsWorld   world;

        // Кэш тел: ключ — адрес Instance (как в RenderBridge)
        std::unordered_map<uintptr_t, PhysicsBody> bodies;

        // Последнее известное значение Workspace.Gravity в studs/s².
        // Используется для детекции изменений без лишних вызовов SetGravity.
        float lastGravityStuds = -1.0f; // -1 = "не инициализировано"

        bool initialized = false;

        // Проверить не изменилась ли гравитация в Workspace и обновить PhysX-сцену.
        void SyncGravity();

        // --- Синхронизация DataModel → PhysX (до симуляции) ---
        void SyncIn();

        // --- Синхронизация PhysX → DataModel (после симуляции) ---
        void SyncOut();
    };

    // -----------------------------------------------------------------------
    // Ctor / Dtor — определяем здесь, где Impl полностью объявлен
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

        // --- PhysicsManager ---
        if (!m_impl->manager.Init(0))
            return false;

        // --- Гравитация из Workspace ---
        float gravityStuds = 196.2f; // дефолт: 196.2 studs/s² = 9.81 m/s²
        {
            Instance* ws = m_impl->dataModel->FindByName("Workspace");
            if (ws)
            {
                auto* prop = ws->GetProperty(Classes::Workspace::Gravity);
                if (prop && prop->Type == PropertyType::Float)
                    gravityStuds = std::abs(prop->Value.AsFloat);
            }
        }
        m_impl->lastGravityStuds = gravityStuds;

        // --- PhysicsWorld ---
        if (!m_impl->world.Init(m_impl->manager, GravityToPhysX(gravityStuds)))
        {
            m_impl->manager.Shutdown();
            return false;
        }

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

        // Сначала удаляем все тела из сцены
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
    // Step: SyncIn → simulate → SyncOut
    // -----------------------------------------------------------------------

    void PhysicsBridge::Step(float dt)
    {
        if (!m_impl->initialized) return;

        m_impl->SyncGravity();
        m_impl->SyncIn();
        m_impl->world.Step(dt);
        m_impl->SyncOut();
    }

    // -----------------------------------------------------------------------
    // SyncGravity — проверяем Workspace.Gravity каждый тик
    // Если значение изменилось — обновляем PxScene без пересоздания мира.
    // -----------------------------------------------------------------------

    void PhysicsBridge::Impl::SyncGravity()
    {
        Instance* ws = dataModel->FindByName("Workspace");
        if (!ws) return;

        auto* prop = ws->GetProperty(Classes::Workspace::Gravity);
        if (!prop || prop->Type != PropertyType::Float) return;

        float newGravityStuds = std::abs(prop->Value.AsFloat);

        // Сравниваем с небольшим epsilon чтобы не дёргать PhysX на каждый кадр
        if (std::abs(newGravityStuds - lastGravityStuds) > 0.001f)
        {
            lastGravityStuds = newGravityStuds;
            world.SetGravity(GravityToPhysX(newGravityStuds));
            std::cout << "[PhysicsBridge] Gravity updated: "
                      << newGravityStuds << " studs/s² = "
                      << (newGravityStuds * STUDS_TO_METERS) << " m/s²\n";
        }
    }

    // -----------------------------------------------------------------------
    // SyncIn — DataModel → PhysX
    // Проходим по всем Instance в DataModel. Для каждого ShapePart:
    //   - если тела ещё нет → создаём PhysicsBody
    //   - если тело есть и оно anchored → обновляем позицию (телепорт)
    // -----------------------------------------------------------------------

    void PhysicsBridge::Impl::SyncIn()
    {
        px::PxPhysics* physics = manager.GetPhysics();
        px::PxScene*   scene   = world.GetScene();

        for (auto& inst : dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::ShapePart::ClassId) continue;

            uintptr_t key = reinterpret_cast<uintptr_t>(inst.get());

            // --- Считываем свойства ---
            CFrame  cf      = {};
            Vector3 size    = { 1.0f, 1.0f, 1.0f };
            Shape   shape   = Shape::Block;
            bool    anchored = false;

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

            bool canCollide = true;
            auto* canCollideProp = inst->GetProperty(Classes::ShapePart::CanCollide);
            if (canCollideProp && canCollideProp->Type == PropertyType::Bool)
                canCollide = canCollideProp->Value.AsBool;

            auto it = bodies.find(key);

            if (it == bodies.end())
            {
                // --- Тела ещё нет — создаём ---
                PhysicsBody body;
                if (!body.Init(physics, scene, cf, size, shape, anchored, canCollide))
                    continue;

                // Если в DataModel уже заданы начальные скорости — передаём в PhysX
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
            }
            else if (anchored)
            {
                // Anchored тело: если DataModel изменил CFrame → телепортируем
                it->second.SetCFrame(cf);
            }
            else
            {
                // Dynamic тело: читаем velocity из DataModel → PhysX каждый тик.
                // Это позволяет SetProperty(PosVelocity/RotVelocity, ...) работать
                // как прямая команда (ShapePart::ApplyImpulse пишет сюда).
                // SyncOut перезапишет их обратно после симуляции — цикл замкнут.
                auto* posVelProp = inst->GetProperty(Classes::ShapePart::PosVelocity);
                if (posVelProp && posVelProp->Type == PropertyType::Vector3)
                    it->second.SetLinearVelocity(posVelProp->Value.AsVector3);

                auto* rotVelProp = inst->GetProperty(Classes::ShapePart::RotVelocity);
                if (rotVelProp && rotVelProp->Type == PropertyType::Vector3)
                    it->second.SetAngularVelocity(rotVelProp->Value.AsVector3);
            }

            // CanCollide можно менять на лету для существующего тела
            if (it != bodies.end())
                it->second.SetCanCollide(canCollide);
        }
    }

    // -----------------------------------------------------------------------
    // SyncOut — PhysX → DataModel
    // Читаем результаты симуляции и пишем CFrame + Velocity обратно
    // в DataModel. Только для dynamic (не Anchored) тел.
    // -----------------------------------------------------------------------

    void PhysicsBridge::Impl::SyncOut()
    {
        for (auto& inst : dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::ShapePart::ClassId) continue;

            uintptr_t key = reinterpret_cast<uintptr_t>(inst.get());
            auto it = bodies.find(key);
            if (it == bodies.end()) continue;

            PhysicsBody& body = it->second;
            if (!body.IsInitialized() || body.IsAnchored()) continue;

            // Позиция и ориентация
            inst->SetProperty(
                Classes::ShapePart::CFrame,
                PropertyValue::CFrame(body.GetCFrame())
            );

            // Линейная скорость
            inst->SetProperty(
                Classes::ShapePart::PosVelocity,
                PropertyValue::Vector3(body.GetLinearVelocity())
            );

            // Угловая скорость
            inst->SetProperty(
                Classes::ShapePart::RotVelocity,
                PropertyValue::Vector3(body.GetAngularVelocity())
            );
        }
    }

    // -----------------------------------------------------------------------
    // ApplyImpulse — применить мгновенный импульс к телу
    // force передаётся в studs/s², конвертируется в м/с² через STUDS_TO_METERS.
    // Используем addImpulse (а не addForce) — он применяется мгновенно,
    // не зависит от dt шага симуляции.
    // -----------------------------------------------------------------------

    void PhysicsBridge::ApplyImpulse(Instance& inst, const Vector3& force)
    {
        if (!m_impl || !m_impl->initialized) return;

        uintptr_t key = reinterpret_cast<uintptr_t>(&inst);
        auto it = m_impl->bodies.find(key);
        if (it == m_impl->bodies.end()) return;

        PhysicsBody& body = it->second;
        if (!body.IsInitialized() || body.IsAnchored()) return;

        // Конвертируем studs → метры и передаём как импульс в PhysX
        const float scale = STUDS_TO_METERS;
        body.SetLinearVelocity(Vector3(
            body.GetLinearVelocity().X + force.X * scale,
            body.GetLinearVelocity().Y + force.Y * scale,
            body.GetLinearVelocity().Z + force.Z * scale
        ));
    }

} // namespace Sunover
