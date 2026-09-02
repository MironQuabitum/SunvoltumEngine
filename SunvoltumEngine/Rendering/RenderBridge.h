#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../LibSunvoltum.h"
#include "LibSunvoltumRender.h"
#include "../DataModel/InstanceParent.h"   // ChildAddedToken
#include "../DataModel/PropertyManager.h"
#include "../Types/CFrame.h"
#include "../Types/Vector3.h"

namespace MeturmRender {
    class Renderer;
    class Window;
    class Texture;
    namespace Objects {
        class Camera;
        class SunLight;
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
    class SunvoltumInput;

    // -------------------------------------------------------------------------
    // SceneEntry — данные одного ShapePart в рендер-сцене.
    // -------------------------------------------------------------------------
    struct SceneEntry
    {
        std::unique_ptr<MeturmRender::Objects::MeshObject> mesh;

        CFrame  cachedCFrame;
        Vector3 cachedSize         = { 1.0f, 1.0f, 1.0f };
        float   cachedTransparency = 0.0f;

        bool dirtyCFrame       = true;
        bool dirtySize         = true;
        bool dirtyTransparency = true;

        // true = Anchored=false: физика двигает объект каждый тик
        bool isDynamic = false;

        PropertyToken tokenCFrame;
        PropertyToken tokenSize;
        PropertyToken tokenTransparency;
        PropertyToken tokenAnchored;
    };

    class LibSunvoltumRender RenderBridge
    {
    public:
        RenderBridge();
        ~RenderBridge();

        RenderBridge(const RenderBridge&)            = delete;
        RenderBridge& operator=(const RenderBridge&) = delete;

        bool Init(Engine& engine, int width = 1280, int height = 720,
                  const char* title = "Sunvoltum");

        void Shutdown();
        void Frame(float deltaTime);
        bool PollEvents();
        bool IsInitialized() const;

        MeturmRender::Window& GetWindow();
        DataModel* GetDataModel() const;
        void SetCursorPosition(float x, float y);

        // Утилиты окна — не требуют включения MeturmRender/MeturmFrame в клиентском коде
        int  GetWindowWidth()  const;
        int  GetWindowHeight() const;
        void UpdateInput();
        void SetInputSource(SunvoltumInput& target);

    private:
        void SyncCamera();
        void SyncLighting();
        void SyncScene();
        void InitSkyBox();

        void SubscribeLighting();
        void SubscribeCamera();
        void RecalcLighting();

        // Подписаться на ChildAdded Workspace — регистрирует ShapePart'ы по событию.
        // Также регистрирует ShapePart'ы уже существующие на момент вызова Init.
        void SubscribeWorkspace();

        // Рекурсивно регистрирует ShapePart'ы в контейнере (Model, Folder, Workspace)
        void RegisterInstanceRecursive(Instance* inst);

        // Создаёт SceneEntry для ShapePart и добавляет в m_scene / m_sceneOrder.
        void RegisterSceneObject(Instance* inst, uintptr_t key);

        Engine*                                          m_engine    = nullptr;
        DataModel*                                       m_dataModel = nullptr;
        std::unique_ptr<MeturmRender::Window>            m_window;
        std::unique_ptr<MeturmRender::Renderer>          m_renderer;
        std::unique_ptr<MeturmRender::Objects::Camera>   m_camera;
        std::unique_ptr<MeturmRender::Objects::SunLight> m_sunLight;
        MeturmRender::Objects::SkyBox*                   m_skyBox      = nullptr;
        MeturmRender::Objects::SkyBox*                   m_skyBoxNight = nullptr;
        MeturmRender::Objects::Cursor*                   m_cursor      = nullptr;

        std::unique_ptr<MeturmRender::Objects::MeshObject> m_sunMesh;
        std::unique_ptr<MeturmRender::Objects::MeshObject> m_moonMesh;

        // --- Сцена ---
        // m_scene       — SceneEntry per ShapePart (keyed by Instance*)
        // m_sceneOrder  — порядок добавления; SyncScene итерирует именно его
        std::unordered_map<uintptr_t, SceneEntry> m_scene;
        std::vector<uintptr_t>                    m_sceneOrder;

        // Токен подписки на ChildAdded Workspace — живёт до Shutdown
        ChildAddedToken m_workspaceChildToken;

        // Токены ChildAdded для вложенных контейнеров (Model, Folder)
        std::vector<ChildAddedToken> m_containerTokens;

        // --- Кэши текстур ---
        std::unordered_map<uintptr_t, std::unique_ptr<MeturmRender::Texture>> m_decalTextureCache;
        std::unordered_map<uintptr_t, std::string>                             m_decalPathCache;
        std::unordered_map<uintptr_t, std::unique_ptr<MeturmRender::Texture>> m_surfaceTextureCache;
        std::unordered_set<uintptr_t> m_decalSyncedParts;
        std::unordered_set<uintptr_t> m_surfaceSyncedParts;

        // --- Кэш Lighting ---
        Instance* m_lightingInst     = nullptr;
        float     m_clockTime        = 14.0f;
        float     m_latitude         = 45.0f;
        float     m_brightness       = 1.0f;
        bool      m_needLightingSync = true;

        PropertyToken m_lightClockToken;
        PropertyToken m_lightLatToken;
        PropertyToken m_lightBrightToken;

        // --- Кэш Camera ---
        Instance* m_cameraInst     = nullptr;
        bool      m_needCameraSync = true;

        PropertyToken m_camCFrameToken;
        PropertyToken m_camFovToken;

        bool m_initialized = false;
    };

} // namespace Sunvoltum

#pragma warning(pop)
