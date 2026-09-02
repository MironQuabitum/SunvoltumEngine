#include "Runtime.h"
#include "../Core/Engine.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceClasses/CurrentCamera.h"
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
        if (!m_camInst) return;
        if (m_cameraMode != CameraType::Follow) return;
        if (!m_subject) return;

        static constexpr PropertyId SHAPE_CFRAME_ID = 0;
        Vector3 target(0.0f, 0.0f, 0.0f);
        auto* subjCF = m_subject->GetProperty(SHAPE_CFRAME_ID);
        if (subjCF && subjCF->Type == PropertyType::CFrame)
            target = subjCF->Value.AsCFrame.Position;

        IInputSource& in = *m_input;

        // ПКМ нажата
        if (in.IsMousePressed(MouseButton::Right))
        {
            m_preLockMouseX = in.GetMouseX();
            m_preLockMouseY = in.GetMouseY();
            in.SetMouseLocked(true);
            in.ResetMouseDelta();
        }

        // ПКМ отпущена
        if (in.IsMouseReleased(MouseButton::Right))
        {
            in.SetMouseLocked(false);
            in.SetCursorVisible(false);
            in.SetMousePosition(m_preLockMouseX, m_preLockMouseY);
        }

        // Вращение орбиты
        if (in.IsMouseLocked())
        {
            m_followYaw   += static_cast<float>(in.GetMouseDeltaX()) * FOLLOW_SENS;
            m_followPitch += static_cast<float>(in.GetMouseDeltaY()) * FOLLOW_SENS;
            if (m_followPitch > FOLLOW_PITCH_MAX) m_followPitch = FOLLOW_PITCH_MAX;
            if (m_followPitch < FOLLOW_PITCH_MIN) m_followPitch = FOLLOW_PITCH_MIN;
            in.ResetMouseDelta();
        }

        // Колесо мыши — зум
        int wheel = in.GetMouseWheel();
        if (wheel != 0)
        {
            float delta = (static_cast<float>(wheel) / 120.0f) * FOLLOW_WHEEL_SPEED;
            m_followRadius -= delta;
            if (m_followRadius < m_radiusMin) m_followRadius = m_radiusMin;
            if (m_followRadius > m_radiusMax) m_followRadius = m_radiusMax;
        }

        // First-person (radius == 0)
        if (m_followRadius <= 0.0f)
        {
            if (!m_firstPersonLocked)
            {
                int cx = m_renderBridge->GetWindowWidth()  / 2;
                int cy = m_renderBridge->GetWindowHeight() / 2;
                if (!in.IsMouseLocked())
                {
                    in.SetMousePosition(cx, cy);
                    in.SetMouseLocked(true);
                    in.ResetMouseDelta();
                }
                m_renderBridge->SetCursorPosition(
                    static_cast<float>(cx), static_cast<float>(cy));
                m_firstPersonLocked = true;
            }

            m_followYaw   += static_cast<float>(in.GetMouseDeltaX()) * FOLLOW_SENS;
            m_followPitch += static_cast<float>(in.GetMouseDeltaY()) * FOLLOW_SENS;
            if (m_followPitch > FOLLOW_PITCH_MAX) m_followPitch = FOLLOW_PITCH_MAX;
            if (m_followPitch < FOLLOW_PITCH_MIN) m_followPitch = FOLLOW_PITCH_MIN;
            in.ResetMouseDelta();

            Matrix3x3 rot = Matrix3x3::FromEuler(m_followPitch, m_followYaw, 0.0f);
            m_camInst->SetProperty(Classes::CurrentCamera::CFrame,
                PropertyValue::CFrame(Sunvoltum::CFrame(target, rot)));
            return;
        }

        // Выход из first-person
        if (m_firstPersonLocked)
        {
            in.SetMouseLocked(false);
            in.SetCursorVisible(false);
            int cx = m_renderBridge->GetWindowWidth()  / 2;
            int cy = m_renderBridge->GetWindowHeight() / 2;
            in.SetMousePosition(cx, cy);
            m_firstPersonLocked = false;
        }

        // Обычная орбита
        Matrix3x3 rot    = Matrix3x3::FromEuler(m_followPitch, m_followYaw, 0.0f);
        Vector3   offset = rot * Vector3(0.0f, 0.0f, -m_followRadius);
        Vector3   camPos = target + offset;

        m_camInst->SetProperty(Classes::CurrentCamera::CFrame,
            PropertyValue::CFrame(Sunvoltum::CFrame(camPos, rot)));
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
