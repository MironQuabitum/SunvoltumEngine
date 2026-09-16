#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../LibSunvoltum.h"
#include "LibSunvoltumRender.h"
#include "../DataModel/InstanceParent.h"   // ChildAddedToken
#include "../DataModel/Instance.h"         // InstanceNetId
#include "../DataModel/PropertyManager.h"
#include "../Types/CFrame.h"
#include "../Types/Vector3.h"
#include "../Types/Color3.h"
#include "../Types/Shape.h"
#include "../Runtime/IRenderBridge.h"
#include "../Input/SunvoltumInput.h"

namespace SunvoltumManager {
    class IPlatformWindow;
}

namespace SunvoltumRender {
    class Renderer;
    class Texture;
    namespace Objects {
        class Camera;
        class DirectionalLight;
        class SkyBox;
        class MeshObject;
        class Cursor;
    }
}

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunvoltum {

    class Engine;
    class DataModel;
    class Instance;

    // -------------------------------------------------------------------------
    // SceneEntry — данные одного ShapePart в рендер-сцене.
    // -------------------------------------------------------------------------
    struct SceneEntry
    {
        std::unique_ptr<SunvoltumRender::Objects::MeshObject> mesh;

        CFrame  cachedCFrame;
        Vector3 cachedSize         = { 1.0f, 1.0f, 1.0f };
        float   cachedTransparency = 0.0f;
        Color3  cachedColor        = { 1.0f, 1.0f, 1.0f };
        Shape   cachedShape        = Shape::Block;

        bool dirtyCFrame       = true;
        bool dirtySize         = true;
        bool dirtyTransparency = true;
        bool dirtyColor        = false;
        bool dirtyShape        = false;

        // true = Anchored=false: физика двигает объект каждый тик
        bool isDynamic = false;

        // -----------------------------------------------------------------------
        // Интерполяция для network-controlled объектов (Anchored=false, не owned).
        //
        // Когда сервер/владелец шлёт CFrame через PropertyUpdate (~30 Hz),
        // мы не применяем его напрямую, а буферизируем два снапшота и плавно
        // интерполируем между ними за время interpPeriod.
        //
        // isNetworkControlled = true  → позиция приходит по сети, нужна интерп.
        //                               (Anchored=false, управляется чужой физикой)
        // isNetworkControlled = false → позиция от локальной физики (owned или сервер).
        // -----------------------------------------------------------------------
        bool isNetworkControlled = false;

        // Два снапшота: от → до
        CFrame interpFrom;
        CFrame interpTo;

        // Сколько времени прошло с момента получения последнего снапшота
        float interpTimer  = 0.0f;

        // Ожидаемый период между пакетами (инициализируется из RATE_PHYSICS_MS ~33ms)
        // Подстраивается под реальный интервал получения пакетов.
        float interpPeriod = 0.033f;

        // Время получения предпоследнего снапшота — для подстройки interpPeriod
        float lastSnapshotTime = 0.0f;

        PropertyToken tokenCFrame;
        PropertyToken tokenSize;
        PropertyToken tokenTransparency;
        PropertyToken tokenAnchored;
        PropertyToken tokenColor;
        PropertyToken tokenShape;
    };

    class LibSunvoltumRender RenderBridge : public IRenderBridge
    {
    public:
        RenderBridge();
        ~RenderBridge();

        RenderBridge(const RenderBridge&)            = delete;
        RenderBridge& operator=(const RenderBridge&) = delete;

        bool Init(Engine& engine, int width = 1280, int height = 720,
                  const char* title = "Sunvoltum");

        void Shutdown();

        // IRenderBridge
        bool       IsInitialized()   const override;
        void       UpdateInput()           override;
        bool       PollEvents()            override;
        void       Frame(float deltaTime)  override;
        DataModel* GetDataModel()    const override;
        int        GetWindowWidth()  const override;
        int        GetWindowHeight() const override;
        void       SetCursorPosition(float x, float y) override;
        void       SetEngineCursorPosition(float x, float y) override;

        SunvoltumManager::IPlatformWindow* GetWindow();
        void SetInputSource(SunvoltumInput& target);

        // Доступ к объекту ввода (привязывается при Init)
        SunvoltumInput& GetInput();

    private:
        void SyncCamera();
        void SyncLighting();
        void SyncScene(float deltaTime);
        void InitSkyBox();

        void SubscribeLighting();
        void SubscribeCamera();
        void RecalcLighting();

        // Обновляет позиции billboard-мешей солнца и луны относительно текущей
        // позиции камеры. Вызывается каждый кадр — независимо от m_needLightingSync —
        // чтобы билборды следовали за камерой даже при отсутствии сетевых обновлений
        // (высокий пинг / потеря пакетов).
        void UpdateCelestialBillboards();

        // Подписаться на ChildAdded Workspace — регистрирует ShapePart'ы по событию.
        // Также регистрирует ShapePart'ы уже существующие на момент вызова Init.
        void SubscribeWorkspace();

        // Рекурсивно регистрирует ShapePart'ы в контейнере (Model, Folder, Workspace)
        void RegisterInstanceRecursive(Instance* inst);

        // Создаёт SceneEntry для ShapePart и добавляет в m_scene / m_sceneOrder.
        void RegisterSceneObject(Instance* inst, uintptr_t key);

        Engine*                                                 m_engine    = nullptr;
        DataModel*                                              m_dataModel = nullptr;
        std::unique_ptr<SunvoltumManager::IPlatformWindow>     m_window;
        std::unique_ptr<SunvoltumRender::Renderer>             m_renderer;
        std::unique_ptr<SunvoltumRender::Objects::Camera>       m_camera;
        std::unique_ptr<SunvoltumRender::Objects::DirectionalLight> m_sunLight;
        std::unique_ptr<SunvoltumRender::Objects::SkyBox>       m_skyBox;
        std::unique_ptr<SunvoltumRender::Objects::SkyBox>       m_skyBoxNight;

        std::unique_ptr<SunvoltumRender::Objects::MeshObject>   m_sunMesh;
        std::unique_ptr<SunvoltumRender::Objects::MeshObject>   m_moonMesh;
        std::unique_ptr<SunvoltumRender::Objects::Cursor>       m_cursor;

        // --- Сцена ---
        // m_scene       — SceneEntry per ShapePart (keyed by Instance*)
        // m_sceneOrder  — порядок добавления; SyncScene итерирует именно его
        std::unordered_map<uintptr_t, SceneEntry> m_scene;
        std::vector<uintptr_t>                    m_sceneOrder;

        // Накопленное время рендера (секунды) — используется в интерполяции
        float m_totalTime = 0.0f;

        // Токен подписки на ChildAdded Workspace — живёт до Shutdown
        ChildAddedToken m_workspaceChildToken;

        // Токены ChildAdded для вложенных контейнеров (Model, Folder)
        std::vector<ChildAddedToken> m_containerTokens;

        // --- Кэши текстур ---
        std::unordered_map<uintptr_t, std::unique_ptr<SunvoltumRender::Texture>> m_decalTextureCache;
        std::unordered_map<uintptr_t, std::string>                                m_decalPathCache;
        std::unordered_map<uintptr_t, std::unique_ptr<SunvoltumRender::Texture>> m_surfaceTextureCache;
        std::unordered_set<uintptr_t> m_decalSyncedParts;
        std::unordered_set<uintptr_t> m_surfaceSyncedParts;

        // --- Кэш Lighting ---
        Instance* m_lightingInst     = nullptr;
        float     m_clockTime        = 14.0f;
        float     m_latitude         = 45.0f;
        float     m_brightness       = 1.0f;
        bool      m_needLightingSync = true;

        // Кэш солнечных углов — обновляются в RecalcLighting(),
        // используются в UpdateCelestialBillboards() каждый кадр.
        float     m_sunAltitude      = 0.0f;
        float     m_sunAzimuth       = 0.0f;
        float     m_moonAltitude     = 0.0f;
        float     m_moonAzimuth      = 0.0f;

        PropertyToken m_lightClockToken;
        PropertyToken m_lightLatToken;
        PropertyToken m_lightBrightToken;

        // --- Кэш Camera ---
        Instance* m_cameraInst     = nullptr;
        bool      m_needCameraSync = true;

        PropertyToken m_camCFrameToken;
        PropertyToken m_camFovToken;

        bool m_initialized = false;

        // Ввод — инициализируется в Init, возвращается через GetInput()
        SunvoltumInput m_inputObj;
    };

} // namespace Sunvoltum

#pragma warning(pop)
