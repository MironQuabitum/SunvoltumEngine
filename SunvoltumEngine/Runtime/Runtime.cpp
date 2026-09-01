#include "Runtime.h"
#include "../Rendering/RenderBridge.h"
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
#include <MeturmRender/Core/Window.h>
#include <cmath>

namespace Sunvoltum {

    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration  = std::chrono::duration<float>;

    Runtime::Runtime() = default;

    void Runtime::SetRenderBridge(RenderBridge* bridge)
    {
        m_renderBridge = bridge;
        if (bridge && bridge->IsInitialized())
            Input.SetSource(&bridge->GetWindow().GetInput());
    }

    void Runtime::SetEngine(Engine* engine)
    {
        m_engine = engine;
    }

    // -------------------------------------------------------------------------
    // InitFollowCamera — вызывается из Start() после того как DataModel заполнен.
    // Находит CurrentCamera, кэширует указатели и подписывается на редко
    // меняющиеся свойства: CameraMode, CameraSubject, MinZoom, MaxZoom.
    //
    // CFrame субъекта намеренно не кэшируется — он меняется каждый тик физики,
    // поэтому в UpdateFollowCamera() читается напрямую через GetProperty().
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

        // --- CameraMode ---
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

        // --- CameraSubject ---
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

        // --- MinZoomDistance ---
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

        // --- MaxZoomDistance ---
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

        // Начальный клэмп радиуса
        if (m_radiusMin < 0.0f) m_radiusMin = 0.0f;
        if (m_radiusMax < m_radiusMin) m_radiusMax = m_radiusMin;
        if (m_followRadius < m_radiusMin) m_followRadius = m_radiusMin;
        if (m_followRadius > m_radiusMax) m_followRadius = m_radiusMax;
    }

    void Runtime::Start()
    {
        if (m_renderBridge && m_renderBridge->IsInitialized() && !Input.IsValid())
            Input.SetSource(&m_renderBridge->GetWindow().GetInput());

        // Подписываемся на свойства камеры после заполнения DataModel
        InitFollowCamera();

        m_running = true;
        RunLoop();
    }

    void Runtime::Stop()
    {
        m_running = false;
    }

    // -------------------------------------------------------------------------
    // UpdateFollowCamera — теперь читает только закэшированные значения.
    // GetProperty вызывается только для CFrame субъекта (меняется каждый тик).
    // -------------------------------------------------------------------------
    void Runtime::UpdateFollowCamera()
    {
        if (!m_renderBridge || !m_camInst) return;
        if (m_cameraMode != CameraType::Follow) return;
        if (!m_subject) return;

        // CFrame субъекта — меняется каждый тик физики, читаем напрямую
        static constexpr PropertyId SHAPE_CFRAME_ID = 0;
        Vector3 target(0.0f, 0.0f, 0.0f);
        auto* subjCF = m_subject->GetProperty(SHAPE_CFRAME_ID);
        if (subjCF && subjCF->Type == PropertyType::CFrame)
            target = subjCF->Value.AsCFrame.Position;

        // Пределы зума уже в m_radiusMin / m_radiusMax (обновляются по коллбэку)

        // ПКМ нажата
        if (Input.IsMousePressed(MouseButton::Right))
        {
            m_preLockMouseX = Input.GetMouseX();
            m_preLockMouseY = Input.GetMouseY();
            Input.SetMouseLocked(true);
            Input.ResetMouseDelta();
        }

        // ПКМ отпущена
        if (Input.IsMouseReleased(MouseButton::Right))
        {
            Input.SetMouseLocked(false);
            Input.SetCursorVisible(false);
            Input.SetMousePosition(m_preLockMouseX, m_preLockMouseY);
        }

        // Вращение орбиты
        if (Input.IsMouseLocked())
        {
            m_followYaw   += static_cast<float>(Input.GetMouseDeltaX()) * FOLLOW_SENS;
            m_followPitch += static_cast<float>(Input.GetMouseDeltaY()) * FOLLOW_SENS;
            if (m_followPitch > FOLLOW_PITCH_MAX) m_followPitch = FOLLOW_PITCH_MAX;
            if (m_followPitch < FOLLOW_PITCH_MIN) m_followPitch = FOLLOW_PITCH_MIN;
            Input.ResetMouseDelta();
        }

        // Колесо мыши — зум
        int wheel = Input.GetMouseWheel();
        if (wheel != 0)
        {
            float delta = (static_cast<float>(wheel) / 120.0f) * FOLLOW_WHEEL_SPEED;
            m_followRadius -= delta;
            if (m_followRadius < m_radiusMin) m_followRadius = m_radiusMin;
            if (m_followRadius > m_radiusMax) m_followRadius = m_radiusMax;
        }

        // --- First-person (radius == 0) ---
        if (m_followRadius <= 0.0f)
        {
            if (!m_firstPersonLocked)
            {
                int cx = m_renderBridge->GetWindow().GetWidth()  / 2;
                int cy = m_renderBridge->GetWindow().GetHeight() / 2;
                if (!Input.IsMouseLocked())
                {
                    Input.SetMousePosition(cx, cy);
                    Input.SetMouseLocked(true);
                    Input.ResetMouseDelta();
                }
                m_renderBridge->SetCursorPosition(
                    static_cast<float>(cx), static_cast<float>(cy));
                m_firstPersonLocked = true;
            }

            m_followYaw   += static_cast<float>(Input.GetMouseDeltaX()) * FOLLOW_SENS;
            m_followPitch += static_cast<float>(Input.GetMouseDeltaY()) * FOLLOW_SENS;
            if (m_followPitch > FOLLOW_PITCH_MAX) m_followPitch = FOLLOW_PITCH_MAX;
            if (m_followPitch < FOLLOW_PITCH_MIN) m_followPitch = FOLLOW_PITCH_MIN;
            Input.ResetMouseDelta();

            Matrix3x3 rot = Matrix3x3::FromEuler(m_followPitch, m_followYaw, 0.0f);
            m_camInst->SetProperty(Classes::CurrentCamera::CFrame,
                PropertyValue::CFrame(Sunvoltum::CFrame(target, rot)));
            return;
        }

        // Выход из first-person
        if (m_firstPersonLocked)
        {
            Input.SetMouseLocked(false);
            Input.SetCursorVisible(false);
            int cx = m_renderBridge->GetWindow().GetWidth()  / 2;
            int cy = m_renderBridge->GetWindow().GetHeight() / 2;
            Input.SetMousePosition(cx, cy);
            m_firstPersonLocked = false;
        }

        // --- Обычная орбита ---
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
                m_renderBridge->GetWindow().GetInput().Update();
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
