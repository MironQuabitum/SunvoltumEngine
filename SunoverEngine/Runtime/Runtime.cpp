#include "Runtime.h"
#include "../Rendering/RenderBridge.h"
#include "../Core/Engine.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceClasses/CurrentCamera.h"
#include "../DataModel/PropertyValue.h"
#include "../Types/CFrame.h"
#include "../Types/Matrix3x3.h"
#include "../Types/Vector3.h"
#include "../Input/MouseButton.h"
#include <MeturmRender/Core/Window.h>
#include <cmath>

namespace Sunover {

    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration  = std::chrono::duration<float>;

    Runtime::Runtime() = default;

    void Runtime::SetRenderBridge(RenderBridge* bridge)
    {
        m_renderBridge = bridge;

        // Подключаем Input источник из окна RenderBridge
        if (bridge && bridge->IsInitialized())
            Input.SetSource(&bridge->GetWindow().GetInput());
    }

    void Runtime::SetEngine(Engine* engine)
    {
        m_engine = engine;
    }

    void Runtime::Start()
    {
        // Если SetRenderBridge вызван до Init окна — подключаем Input здесь
        if (m_renderBridge && m_renderBridge->IsInitialized() && !Input.IsValid())
            Input.SetSource(&m_renderBridge->GetWindow().GetInput());

        m_running = true;
        RunLoop();
    }

    void Runtime::Stop()
    {
        m_running = false;
    }

    // ---------------------------------------------------------------------------
    // Follow-камера — орбитальное вращение вокруг CameraSubject
    // ---------------------------------------------------------------------------
    void Runtime::UpdateFollowCamera()
    {
        if (!m_renderBridge) return;
        DataModel* dm = m_renderBridge->GetDataModel();
        if (!dm) return;

        // Найти CurrentCamera в DataModel
        Instance* camInst = nullptr;
        for (auto& child : dm->GetChildren())
        {
            if (child->GetClassId() == Classes::CurrentCamera::ClassId)
            {
                camInst = child.get();
                break;
            }
        }
        if (!camInst) return;

        // Проверяем режим
        auto* modeProp = camInst->GetProperty(Classes::CurrentCamera::CameraMode);
        if (!modeProp || modeProp->Type != PropertyType::CameraType) return;
        if (modeProp->Value.AsCameraType != CameraType::Follow) return;

        // Получаем CameraSubject
        auto* subjectProp = camInst->GetProperty(Classes::CurrentCamera::CameraSubject);
        if (!subjectProp || subjectProp->Type != PropertyType::InstanceRef) return;
        Instance* subject = subjectProp->Value.AsInstanceRef;
        if (!subject) return;

        // Позиция субъекта
        static constexpr PropertyId SHAPE_CFRAME_ID = 0;
        auto* subjCFProp = subject->GetProperty(SHAPE_CFRAME_ID);
        Vector3 target(0.0f, 0.0f, 0.0f);
        if (subjCFProp && subjCFProp->Type == PropertyType::CFrame)
            target = subjCFProp->Value.AsCFrame.Position;

        // Читаем пределы зума из свойств камеры, иначе fallback-дефолты
        float radiusMin = FOLLOW_RADIUS_MIN_DEFAULT;
        float radiusMax = FOLLOW_RADIUS_MAX_DEFAULT;
        auto* minProp = camInst->GetProperty(Classes::CurrentCamera::MinZoomDistance);
        if (minProp && minProp->Type == PropertyType::Number)
            radiusMin = static_cast<float>(minProp->Value.AsNumber);
        auto* maxProp = camInst->GetProperty(Classes::CurrentCamera::MaxZoomDistance);
        if (maxProp && maxProp->Type == PropertyType::Number)
            radiusMax = static_cast<float>(maxProp->Value.AsNumber);
        // Защита от некорректных значений
        if (radiusMin < 0.0f)           radiusMin = 0.0f;
        if (radiusMax < radiusMin)      radiusMax = radiusMin;
        // Держим текущий радиус в актуальных пределах при смене свойств
        if (m_followRadius < radiusMin) m_followRadius = radiusMin;
        if (m_followRadius > radiusMax) m_followRadius = radiusMax;

        // ПКМ нажата: запоминаем позицию курсора движка ДО лока, потом лочим
        if (Input.IsMousePressed(MouseButton::Right))
        {
            m_preLockMouseX = Input.GetMouseX();
            m_preLockMouseY = Input.GetMouseY();
            Input.SetMouseLocked(true);
            Input.ResetMouseDelta();
        }

        // ПКМ отпущена: разлочиваем и телепортируем системный курсор
        // обратно туда, где был курсор движка в момент нажатия
        if (Input.IsMouseReleased(MouseButton::Right))
        {
            Input.SetMouseLocked(false);
            // SetMouseLocked(false) внутри MeturmFrame автоматически вызывает
            // SetCursorVisible(true) — сразу скрываем обратно, движок рисует свой курсор
            Input.SetCursorVisible(false);
            Input.SetMousePosition(m_preLockMouseX, m_preLockMouseY);
        }

        // Пока мышь залочена — обновляем углы орбиты
        if (Input.IsMouseLocked())
        {
            m_followYaw   += static_cast<float>(Input.GetMouseDeltaX()) * FOLLOW_SENS;
            m_followPitch += static_cast<float>(Input.GetMouseDeltaY()) * FOLLOW_SENS;
            if (m_followPitch > FOLLOW_PITCH_MAX) m_followPitch = FOLLOW_PITCH_MAX;
            if (m_followPitch < FOLLOW_PITCH_MIN) m_followPitch = FOLLOW_PITCH_MIN;
            Input.ResetMouseDelta();
        }

        // Колесо мыши — плавный зум с учётом Min/MaxZoomDistance
        int wheel = Input.GetMouseWheel();
        if (wheel != 0)
        {
            // WM_MOUSEWHEEL даёт delta кратную 120 за один клик колёса.
            // Делим на 120 чтобы получить количество кликов, умножаем на скорость.
            float delta = (static_cast<float>(wheel) / 120.0f) * FOLLOW_WHEEL_SPEED;
            m_followRadius -= delta;
            if (m_followRadius < radiusMin) m_followRadius = radiusMin;
            if (m_followRadius > radiusMax) m_followRadius = radiusMax;
        }

        // --- First-person при radius == 0 ---
        // Мышь лочится автоматически (без ПКМ), курсор движка идёт в центр экрана.
        if (m_followRadius <= 0.0f)
        {
            // Входим в first-person — лочим мышь автоматически если ещё не залочена.
            // Если пользователь уже держит ПКМ — мышь и так залочена, просто ставим флаг.
            if (!m_firstPersonLocked)
            {
                int cx = m_renderBridge->GetWindow().GetWidth()  / 2;
                int cy = m_renderBridge->GetWindow().GetHeight() / 2;
                if (!Input.IsMouseLocked())
                {
                    Input.SetMousePosition(cx, cy); // курсор движка в центр
                    Input.SetMouseLocked(true);
                    Input.ResetMouseDelta();
                }
                // Выставляем позицию курсора движка (2D спрайт) сразу —
                // Frame.cpp не двигает его пока мышь залочена, поэтому без этого
                // курсор оставался бы на старом месте до следующего кадра.
                m_renderBridge->SetCursorPosition(
                    static_cast<float>(cx),
                    static_cast<float>(cy)
                );
                m_firstPersonLocked = true;
            }

            // Читаем дельту и крутим камеру
            m_followYaw   += static_cast<float>(Input.GetMouseDeltaX()) * FOLLOW_SENS;
            m_followPitch += static_cast<float>(Input.GetMouseDeltaY()) * FOLLOW_SENS;
            if (m_followPitch > FOLLOW_PITCH_MAX) m_followPitch = FOLLOW_PITCH_MAX;
            if (m_followPitch < FOLLOW_PITCH_MIN) m_followPitch = FOLLOW_PITCH_MIN;
            Input.ResetMouseDelta();

            Matrix3x3 rot = Matrix3x3::FromEuler(m_followPitch, m_followYaw, 0.0f);
            camInst->SetProperty(Classes::CurrentCamera::CFrame,
                PropertyValue::CFrame(Sunover::CFrame(target, rot)));
            return;
        }

        // Вышли из first-person (зум назад) — снимаем автолок если мы его ставили
        if (m_firstPersonLocked)
        {
            Input.SetMouseLocked(false);
            Input.SetCursorVisible(false);
            // Телепортируем системный курсор в центр (где был курсор движка)
            int cx = m_renderBridge->GetWindow().GetWidth()  / 2;
            int cy = m_renderBridge->GetWindow().GetHeight() / 2;
            Input.SetMousePosition(cx, cy);
            m_firstPersonLocked = false;
        }

        // --- Обычная орбита ---
        Matrix3x3 rot    = Matrix3x3::FromEuler(m_followPitch, m_followYaw, 0.0f);
        Vector3   offset = rot * Vector3(0.0f, 0.0f, -m_followRadius);
        Vector3   camPos = target + offset;

        camInst->SetProperty(Classes::CurrentCamera::CFrame,
            PropertyValue::CFrame(Sunover::CFrame(camPos, rot)));
    }

    void Runtime::RunLoop()
    {
        TimePoint previousTime = Clock::now();
        float     accumulator  = 0.0f;

        while (m_running)
        {
            if (m_renderBridge && m_renderBridge->IsInitialized())
            {
                // Input::Update() — сбрасывает per-frame дельты до PollEvents
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

            // RenderStepped вызывается ДО рендера, чтобы логика (в т.ч. Follow-камера)
            // успела обновить CFrame перед тем как SyncCamera() прочитает его.
            if (RenderStepped) RenderStepped(deltaTime);

            // Follow-камера обновляется движком после пользовательского RenderStepped,
            // чтобы пользователь мог изменить CameraSubject/CameraMode в своём колбэке.
            UpdateFollowCamera();

            if (m_renderBridge && m_renderBridge->IsInitialized())
                m_renderBridge->Frame(deltaTime);

            accumulator += deltaTime;
            while (accumulator >= FIXED_TIMESTEP)
            {
                if (PreSimulation)  PreSimulation(FIXED_TIMESTEP);
                // Физический тик движка — между Pre и PostSimulation
                if (m_engine) m_engine->PhysicsTick(FIXED_TIMESTEP);
                if (PostSimulation) PostSimulation(FIXED_TIMESTEP);
                accumulator -= FIXED_TIMESTEP;
            }

            if (Heartbeat) Heartbeat(deltaTime);
        }
    }

} // namespace Sunover
