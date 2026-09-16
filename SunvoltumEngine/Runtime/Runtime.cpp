#include "Runtime.h"
#include "../Core/Engine.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceClasses/CurrentCamera.h"
#include "../DataModel/InstanceClasses/BasePart.h"
#include "../DataModel/InstanceClasses/Humanoid.h"
#include "../DataModel/InstanceClasses/Decal.h"
#include "../DataModel/PropertyValue.h"
#include "../DataModel/PropertyManager.h"
#include "../Types/CFrame.h"
#include "../Types/Matrix3x3.h"
#include "../Types/Vector3.h"
#include "../Types/CameraType.h"
#include "../Input/MouseButton.h"
#include <cmath>

namespace Sunvoltum {

    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration  = std::chrono::duration<float>;

    Runtime::Runtime()  = default;
    Runtime::~Runtime() = default;

    void Runtime::SetRenderBridge(IRenderBridge* bridge)
    {
        m_renderBridge = bridge;
    }

    void Runtime::SetInputSource(IInputSource* input)
    {
        m_input = input;
    }

    void Runtime::SetEngine(Engine* engine)
    {
        m_engine = engine;
    }

    // -------------------------------------------------------------------------
    // InitFollowCamera — находит CurrentCamera в DataModel и подписывается
    // на редко меняющиеся свойства.
    // -------------------------------------------------------------------------
    void Runtime::InitFollowCamera()
    {
        DataModel* dm = m_renderBridge ? m_renderBridge->GetDataModel() : nullptr;
        if (!dm) return;

        for (auto& child : dm->GetChildren())
        {
            if (child->GetClassId() != Classes::CurrentCamera::ClassId) continue;
            m_camInst = child.get();
            break;
        }
        if (!m_camInst) return;

        auto& pm = PropertyManager::Get();

        // CameraMode
        {
            auto* p = m_camInst->GetProperty(Classes::CurrentCamera::CameraMode);
            if (p && p->Type == PropertyType::CameraType)
                m_cameraMode = p->Value.AsCameraType;
        }
        m_camModeToken = pm.Subscribe(m_camInst, Classes::CurrentCamera::CameraMode,
            [this](const PropertyValue& val)
            {
                if (val.Type == PropertyType::CameraType)
                    m_cameraMode = val.Value.AsCameraType;
            });

        // CameraSubject
        {
            auto* p = m_camInst->GetProperty(Classes::CurrentCamera::CameraSubject);
            if (p && p->Type == PropertyType::InstanceRef)
                m_subject = p->Value.AsInstanceRef;
        }
        m_camSubjectToken = pm.Subscribe(m_camInst, Classes::CurrentCamera::CameraSubject,
            [this](const PropertyValue& val)
            {
                if (val.Type == PropertyType::InstanceRef)
                    m_subject = val.Value.AsInstanceRef;
            });

        // MinZoomDistance
        {
            auto* p = m_camInst->GetProperty(Classes::CurrentCamera::MinZoomDistance);
            if (p && p->Type == PropertyType::Number)
                m_radiusMin = static_cast<float>(p->Value.AsNumber);
        }
        m_camMinZoomToken = pm.Subscribe(m_camInst, Classes::CurrentCamera::MinZoomDistance,
            [this](const PropertyValue& val)
            {
                if (val.Type == PropertyType::Number)
                {
                    m_radiusMin = static_cast<float>(val.Value.AsNumber);
                    if (m_radiusMin < 0.0f) m_radiusMin = 0.0f;
                    if (m_radiusMax < m_radiusMin) m_radiusMax = m_radiusMin;
                    if (m_followRadius < m_radiusMin) m_followRadius = m_radiusMin;
                    if (m_followRadius > m_radiusMax) m_followRadius = m_radiusMax;
                }
            });

        // MaxZoomDistance
        {
            auto* p = m_camInst->GetProperty(Classes::CurrentCamera::MaxZoomDistance);
            if (p && p->Type == PropertyType::Number)
                m_radiusMax = static_cast<float>(p->Value.AsNumber);
        }
        m_camMaxZoomToken = pm.Subscribe(m_camInst, Classes::CurrentCamera::MaxZoomDistance,
            [this](const PropertyValue& val)
            {
                if (val.Type == PropertyType::Number)
                {
                    m_radiusMax = static_cast<float>(val.Value.AsNumber);
                    if (m_radiusMax < m_radiusMin) m_radiusMax = m_radiusMin;
                    if (m_followRadius > m_radiusMax) m_followRadius = m_radiusMax;
                }
            });

        if (m_radiusMin < 0.0f) m_radiusMin = 0.0f;
        if (m_radiusMax < m_radiusMin) m_radiusMax = m_radiusMin;
        if (m_followRadius < m_radiusMin) m_followRadius = m_radiusMin;
        if (m_followRadius > m_radiusMax) m_followRadius = m_radiusMax;
    }

    void Runtime::Start()
    {
        InitFollowCamera();
        m_running = true;
        RunLoop();
    }

    void Runtime::Stop()
    {
        m_running = false;
    }

    // -------------------------------------------------------------------------
    // UpdateFollowCamera — только если есть рендер-бридж и ввод.
    // Вся логика через IInputSource и IRenderBridge — никаких конкретных типов.
    // -------------------------------------------------------------------------
    void Runtime::UpdateFollowCamera()
    {
        if (!m_renderBridge || !m_input) return;

        IInputSource& in = *m_input;

        // Если режим камеры Scriptable — Runtime не управляет CFrame камеры.
        // Но поддерживаем позиционирование курсора движка по системной мыши.
        if (m_cameraMode != CameraType::Follow)
        {
            if (in.IsMouseReleased(MouseButton::Right))
                m_isRmbHeld = false;
            m_renderBridge->SetEngineCursorPosition(static_cast<float>(in.GetMouseX()), static_cast<float>(in.GetMouseY()));
            return;
        }

        // Колесо мыши — зум (считываем ДО сброса дельт)
        int wheel = in.GetMouseWheel();
        if (wheel != 0)
        {
            float delta = (static_cast<float>(wheel) / 120.0f) * FOLLOW_WHEEL_SPEED;
            m_followRadius -= delta;
            if (m_followRadius < m_radiusMin) m_followRadius = m_radiusMin;
            if (m_followRadius > m_radiusMax) m_followRadius = m_radiusMax;
        }

        bool wasFirstPerson = m_isFirstPerson;
        m_isFirstPerson = (m_followRadius <= 0.0f);

        // При переходе в режим первого лица центрируем курсоры
        if (m_isFirstPerson && !wasFirstPerson)
        {
            int cx = m_renderBridge->GetWindowWidth() / 2;
            int cy = m_renderBridge->GetWindowHeight() / 2;
            m_lockedMouseX = cx;
            m_lockedMouseY = cy;
            in.SetMousePosition(cx, cy);
            m_renderBridge->SetEngineCursorPosition(static_cast<float>(cx), static_cast<float>(cy));
            in.ResetMouseDelta();
        }

        // При первом нажатии ПКМ (если не от первого лица) запоминаем позицию мыши
        if (in.IsMousePressed(MouseButton::Right))
        {
            m_isRmbHeld = true;
            if (!m_isFirstPerson)
            {
                m_lockedMouseX = in.GetMouseX();
                m_lockedMouseY = in.GetMouseY();
                m_renderBridge->SetEngineCursorPosition(static_cast<float>(m_lockedMouseX), static_cast<float>(m_lockedMouseY));
            }
        }

        // При отпускании ПКМ возвращаем курсор на место (только если не первое лицо)
        if (in.IsMouseReleased(MouseButton::Right))
        {
            m_isRmbHeld = false;
            if (!m_isFirstPerson)
            {
                in.SetMousePosition(m_lockedMouseX, m_lockedMouseY);
            }
        }

        // Вращение камеры:
        // В режиме первого лица вращение свободно (всегда), либо при зажатой ПКМ в обычном режиме
        if (m_isFirstPerson || m_isRmbHeld)
        {
            int dx = in.GetRawMouseDeltaX();
            int dy = in.GetRawMouseDeltaY();
            if (dx == 0 && dy == 0)
            {
                dx = in.GetMouseDeltaX();
                dy = in.GetMouseDeltaY();
            }

            m_followYaw   += static_cast<float>(dx) * FOLLOW_SENS;
            m_followPitch += static_cast<float>(dy) * FOLLOW_SENS;
            if (m_followPitch > FOLLOW_PITCH_MAX) m_followPitch = FOLLOW_PITCH_MAX;
            if (m_followPitch < FOLLOW_PITCH_MIN) m_followPitch = FOLLOW_PITCH_MIN;

            if (m_isFirstPerson)
            {
                int cx = m_renderBridge->GetWindowWidth() / 2;
                int cy = m_renderBridge->GetWindowHeight() / 2;
                in.SetMousePosition(cx, cy);
                m_renderBridge->SetEngineCursorPosition(static_cast<float>(cx), static_cast<float>(cy));
            }
            else
            {
                // Системный курсор всегда возвращаем в сохранённую точку
                in.SetMousePosition(m_lockedMouseX, m_lockedMouseY);
                // Курсор движка остаётся зафиксированным на этих координатах и виден
                m_renderBridge->SetEngineCursorPosition(static_cast<float>(m_lockedMouseX), static_cast<float>(m_lockedMouseY));
            }
        }
        else
        {
            // В обычном режиме обновляем позицию курсора движка по текущей мыши
            m_renderBridge->SetEngineCursorPosition(static_cast<float>(in.GetMouseX()), static_cast<float>(in.GetMouseY()));
        }

        in.ResetMouseDelta();

        if (!m_camInst || !m_subject) return;

        // Обновляем прозрачность модели (если субъект - Humanoid)
        UpdateSubjectTransparency(m_followRadius, FOLLOW_WHEEL_SPEED);

        Vector3 target(0.0f, 0.0f, 0.0f);

        // Если CameraSubject — это Humanoid, позицию берем из сопутствующего Head (или HumanoidRootPart как fallback)
        if (m_subject->GetClassId() == Classes::Humanoid::ClassId)
        {
            InstanceParent* parent = m_subject->GetParent();
            Instance* head = parent ? parent->FindByName("Head") : nullptr;
            Instance* hrp  = parent ? parent->FindByName("HumanoidRootPart") : nullptr;
            if (head)
            {
                auto* cf = head->GetProperty(Classes::BasePart::CFrame);
                if (cf && cf->Type == PropertyType::CFrame)
                    target = cf->Value.AsCFrame.Position;
            }
            else if (hrp)
            {
                auto* cf = hrp->GetProperty(Classes::BasePart::CFrame);
                if (cf && cf->Type == PropertyType::CFrame)
                {
                    target = cf->Value.AsCFrame.Position;
                    target.Y += 1.5f;
                }
            }
        }
        else
        {
            static constexpr PropertyId SHAPE_CFRAME_ID = 0;
            auto* subjCF = m_subject->GetProperty(SHAPE_CFRAME_ID);
            if (subjCF && subjCF->Type == PropertyType::CFrame)
                target = subjCF->Value.AsCFrame.Position;
        }

        // First-person (radius <= 0) — камера привязана к голове
        if (m_followRadius <= 0.0f)
        {
            Matrix3x3 rot = Matrix3x3::FromEuler(m_followPitch, m_followYaw, 0.0f);
            m_camInst->SetProperty(Classes::CurrentCamera::CFrame,
                PropertyValue::CFrame(Sunvoltum::CFrame(target, rot)));
            return;
        }

        // Обычная орбита
        Matrix3x3 rot    = Matrix3x3::FromEuler(m_followPitch, m_followYaw, 0.0f);
        Vector3   offset = rot * Vector3(0.0f, 0.0f, -m_followRadius);
        Vector3   camPos = target + offset;

        m_camInst->SetProperty(Classes::CurrentCamera::CFrame,
            PropertyValue::CFrame(Sunvoltum::CFrame(camPos, rot)));
    }

    void Runtime::UpdateSubjectTransparency(float radius, float /*step*/)
    {
        if (!m_subject || m_subject->GetClassId() != Classes::Humanoid::ClassId)
            return;

        InstanceParent* parent = m_subject->GetParent();
        if (!parent) return;

        // По условию:
        // Если радиус > 2.0 — персонаж полностью непрозрачен (0.0).
        // Если радиус <= 2.0 — появляется легкая прозрачность, и чем меньше radius, тем сильнее transparency.
        // При полном приближении (radius <= 0.0) — transparency = 1.0.
        float targetTransparency = 0.0f;
        if (radius <= 0.0f)
        {
            targetTransparency = 1.0f;
        }
        else if (radius <= 2.0f)
        {
            // При radius == 2.0f: 0.1f (совсем немного прозрачности), при radius -> 0: стремится к 1.0f
            float factor = (2.0f - radius) / 2.0f; // от 0.0 (при radius=2) до 1.0 (при radius=0)
            targetTransparency = 0.1f + 0.9f * factor;
        }

        for (auto& child : parent->GetChildren())
        {
            // Изменяем прозрачность только у визуальных частей тела (не HumanoidRootPart, который уже скрыт)
            if (child->GetName() == "HumanoidRootPart")
                continue;

            if (Classes::IsBasePart(child->GetClassId()))
            {
                auto* transProp = child->GetProperty(Classes::BasePart::Transparency);
                if (transProp && transProp->Type == PropertyType::Float)
                {
                    if (std::abs(transProp->Value.AsFloat - targetTransparency) > 0.001f)
                    {
                        child->SetProperty(Classes::BasePart::Transparency, PropertyValue::Float(targetTransparency));
                    }
                }

                // Также обновляем прозрачность у всех Decal внутри части (например Decal "face" на Head)
                for (auto& partChild : child->GetChildren())
                {
                    if (partChild->GetClassId() == Classes::Decal::ClassId)
                    {
                        auto* decalTrans = partChild->GetProperty(Classes::Decal::Transparency);
                        if (decalTrans && decalTrans->Type == PropertyType::Float)
                        {
                            if (std::abs(decalTrans->Value.AsFloat - targetTransparency) > 0.001f)
                            {
                                partChild->SetProperty(Classes::Decal::Transparency, PropertyValue::Float(targetTransparency));
                            }
                        }
                    }
                }
            }
            else if (child->GetClassId() == Classes::Decal::ClassId)
            {
                auto* decalTrans = child->GetProperty(Classes::Decal::Transparency);
                if (decalTrans && decalTrans->Type == PropertyType::Float)
                {
                    if (std::abs(decalTrans->Value.AsFloat - targetTransparency) > 0.001f)
                    {
                        child->SetProperty(Classes::Decal::Transparency, PropertyValue::Float(targetTransparency));
                    }
                }
            }
        }
    }

    void Runtime::RunLoop()
    {
        TimePoint previousTime = Clock::now();
        float     accumulator  = 0.0f;

        while (m_running)
        {
            if (m_renderBridge && m_renderBridge->IsInitialized())
            {
                m_renderBridge->UpdateInput();
                if (!m_renderBridge->PollEvents())
                {
                    m_running = false;
                    break;
                }
            }

            TimePoint currentTime = Clock::now();
            float     deltaTime   = Duration(currentTime - previousTime).count();
            previousTime = currentTime;
            if (deltaTime > 0.25f) deltaTime = 0.25f;

            if (RenderStepped) RenderStepped(deltaTime);

            UpdateFollowCamera();

            if (m_renderBridge && m_renderBridge->IsInitialized())
                m_renderBridge->Frame(deltaTime);

            accumulator += deltaTime;
            while (accumulator >= FIXED_TIMESTEP)
            {
                if (PreSimulation)  PreSimulation(FIXED_TIMESTEP);
                if (m_engine) m_engine->PhysicsTick(FIXED_TIMESTEP);
                if (PostSimulation) PostSimulation(FIXED_TIMESTEP);
                accumulator -= FIXED_TIMESTEP;
            }

            if (Heartbeat) Heartbeat(deltaTime);
        }
    }

} // namespace Sunvoltum
