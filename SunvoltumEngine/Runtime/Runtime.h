#pragma once

#include <functional>
#include <chrono>

#include "../LibSunvoltum.h"
#include "../DataModel/PropertyManager.h"
#include "../Types/CameraType.h"
#include "IRenderBridge.h"
#include "IInputSource.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunvoltum {

    class Engine;
    class Instance;

    class LibSunvoltum Runtime
    {
    public:
        Runtime();
        ~Runtime();

        Runtime(const Runtime&)            = delete;
        Runtime& operator=(const Runtime&) = delete;

        // Передать рендер-бридж (необязательно — сервер не вызывает)
        void SetRenderBridge(IRenderBridge* bridge);

        // Передать источник ввода (необязательно — сервер не вызывает)
        void SetInputSource(IInputSource* input);

        void SetEngine(Engine* engine);

        // Запрещает Runtime восстанавливать видимость курсора при отпускании ПКМ.
        // Установить true когда приложение само управляет курсором (Mouse2D и т.д.).
        // По умолчанию false — поведение как раньше.
        void SetKeepCursorHidden(bool keep);

        void Start();
        void Stop();

        float GetCameraYaw()  const { return m_followYaw;        }
        float GetCameraZoom() const { return m_followRadius;      }
        bool  IsFirstPerson() const { return m_followRadius <= 0.0f; }

        std::function<void(float deltaTime)>  RenderStepped;
        std::function<void(float fixedDelta)> PreSimulation;
        std::function<void(float fixedDelta)> PostSimulation;
        std::function<void(float deltaTime)>  Heartbeat;

    private:
        bool           m_running      = false;
        IRenderBridge* m_renderBridge = nullptr;
        IInputSource*  m_input        = nullptr;
        Engine*        m_engine       = nullptr;

        // -----------------------------------------------------------
        // Follow-камера
        // -----------------------------------------------------------
        float m_followYaw    =  0.0f;
        float m_followPitch  =  0.3f;
        float m_followRadius = 15.0f;

        bool m_isRmbHeld         = false;
        int  m_lockedMouseX       = 0;
        int  m_lockedMouseY       = 0;

        static constexpr float FOLLOW_SENS        = 0.002618f;
        static constexpr float FOLLOW_PITCH_MIN   = -1.48f;
        static constexpr float FOLLOW_PITCH_MAX   =  1.48f;
        static constexpr float FOLLOW_WHEEL_SPEED =  2.0f;
        static constexpr float FOLLOW_RADIUS_MIN_DEFAULT =  0.0f;
        static constexpr float FOLLOW_RADIUS_MAX_DEFAULT = 60.0f;

        // -----------------------------------------------------------
        // Кэш Follow-камеры
        // -----------------------------------------------------------
        Instance*  m_camInst    = nullptr;
        Instance*  m_subject    = nullptr;

        CameraType m_cameraMode = CameraType::Follow;
        float      m_radiusMin  = FOLLOW_RADIUS_MIN_DEFAULT;
        float      m_radiusMax  = FOLLOW_RADIUS_MAX_DEFAULT;

        PropertyToken m_camModeToken;
        PropertyToken m_camSubjectToken;
        PropertyToken m_camMinZoomToken;
        PropertyToken m_camMaxZoomToken;

        bool m_isFirstPerson = false;

        void InitFollowCamera();
        void UpdateFollowCamera();
        void UpdateSubjectTransparency(float radius, float step);

        static constexpr float FIXED_TIMESTEP = 1.0f / 240.0f;

        void RunLoop();
    };

} // namespace Sunvoltum

#pragma warning(pop)
