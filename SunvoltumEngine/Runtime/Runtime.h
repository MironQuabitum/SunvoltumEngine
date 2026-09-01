#pragma once

#include <functional>
#include <chrono>
#include <vector>

#include "../LibSunvoltum.h"
#include "../Input/SunvoltumInput.h"
#include "../DataModel/PropertyManager.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunvoltum {

    class RenderBridge;
    class Engine;
    class Instance;

    class LibSunvoltum Runtime
    {
    public:
        Runtime();
        ~Runtime() = default;

        Runtime(const Runtime&)            = delete;
        Runtime& operator=(const Runtime&) = delete;

        void SetRenderBridge(RenderBridge* bridge);
        void SetEngine(Engine* engine);

        void Start();
        void Stop();

        float GetCameraYaw() const { return m_followYaw; }

        SunvoltumInput Input;

        std::function<void(float deltaTime)>  RenderStepped;
        std::function<void(float fixedDelta)> PreSimulation;
        std::function<void(float fixedDelta)> PostSimulation;
        std::function<void(float deltaTime)>  Heartbeat;

    private:
        bool          m_running      = false;
        RenderBridge* m_renderBridge = nullptr;
        Engine*       m_engine       = nullptr;

        // -----------------------------------------------------------
        // Follow-камера — орбитальное состояние
        // -----------------------------------------------------------
        float m_followYaw    =  0.0f;
        float m_followPitch  =  0.3f;
        float m_followRadius = 15.0f;

        int  m_preLockMouseX     = 0;
        int  m_preLockMouseY     = 0;
        bool m_firstPersonLocked = false;

        static constexpr float FOLLOW_SENS        = 0.002618f;
        static constexpr float FOLLOW_PITCH_MIN   = -1.48f;
        static constexpr float FOLLOW_PITCH_MAX   =  1.48f;
        static constexpr float FOLLOW_WHEEL_SPEED =  2.0f;
        static constexpr float FOLLOW_RADIUS_MIN_DEFAULT =  0.0f;
        static constexpr float FOLLOW_RADIUS_MAX_DEFAULT = 60.0f;

        // -----------------------------------------------------------
        // Кэш Follow-камеры — обновляется через PropertyManager
        // -----------------------------------------------------------
        Instance*   m_camInst    = nullptr;  // CurrentCamera Instance
        Instance*   m_subject    = nullptr;  // CameraSubject Instance

        CameraType  m_cameraMode = CameraType::Follow;
        float       m_radiusMin  = FOLLOW_RADIUS_MIN_DEFAULT;
        float       m_radiusMax  = FOLLOW_RADIUS_MAX_DEFAULT;

        // Токены подписок Follow-камеры
        PropertyToken m_camModeToken;
        PropertyToken m_camSubjectToken;
        PropertyToken m_camMinZoomToken;
        PropertyToken m_camMaxZoomToken;

        // Вызывается из Start() — находит камеру в DataModel и подписывается
        void InitFollowCamera();

        void UpdateFollowCamera();

        static constexpr float FIXED_TIMESTEP = 1.0f / 240.0f;

        void RunLoop();
    };

} // namespace Sunvoltum

#pragma warning(pop)
