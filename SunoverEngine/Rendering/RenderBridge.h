#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "../LibSunover.h"

namespace MeturmRender {
    class Renderer;
    class Window;
    class Texture;
    namespace Objects {
        class Camera;
        class SunLight;
        class SkyBox;
        class MeshObject;
    }
}

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunover {

    class Engine;
    class DataModel;

    class LibSunover RenderBridge
    {
    public:
        RenderBridge();
        // Определён в Init.cpp где SkyBox полностью определён
        ~RenderBridge();

        RenderBridge(const RenderBridge&)            = delete;
        RenderBridge& operator=(const RenderBridge&) = delete;

        // Инициализация окна и рендерера.
        // Вызывать после Engine::Init() до Runtime::Start().
        bool Init(Engine& engine, int width = 1280, int height = 720,
                  const char* title = "Sunover");

        void Shutdown();

        // Вызывается каждый кадр из Runtime
        void Frame(float deltaTime);

        // Обработка событий окна. Возвращает false если окно закрыто.
        bool PollEvents();

        bool IsInitialized() const;

    private:
        void SyncCamera();
        void SyncLighting();
        void SyncScene();
        void InitSkyBox();

        Engine*                                          m_engine    = nullptr;
        DataModel*                                       m_dataModel = nullptr;
        std::unique_ptr<MeturmRender::Window>            m_window;
        std::unique_ptr<MeturmRender::Renderer>          m_renderer;
        std::unique_ptr<MeturmRender::Objects::Camera>   m_camera;
        std::unique_ptr<MeturmRender::Objects::SunLight> m_sunLight;
        // Сырой указатель — удаляется вручную в Shutdown() где SkyBox полностью определён
        MeturmRender::Objects::SkyBox*                   m_skyBox    = nullptr;

        // Кэш мешей: ключ — адрес экземпляра Instance.
        // MeshObject хранится как persistent объект, GPU-буфер создаётся один раз.
        std::unordered_map<uintptr_t,
            std::unique_ptr<MeturmRender::Objects::MeshObject>> m_meshCache;

        // Кэш загруженных текстур декалей: ключ — адрес Instance декали.
        // Текстура загружается один раз при первом обнаружении, затем переиспользуется.
        std::unordered_map<uintptr_t,
            std::unique_ptr<MeturmRender::Texture>> m_decalTextureCache;

        // Кэш загруженных текстур TextureSurface: ключ — адрес Instance поверхности.
        std::unordered_map<uintptr_t,
            std::unique_ptr<MeturmRender::Texture>> m_surfaceTextureCache;

        // Множество Instance-ключей у которых декали уже добавлены в MeshObject.
        // Декаль добавляется один раз — повторный Add не нужен пока сцена не меняется.
        std::unordered_set<uintptr_t> m_decalSyncedParts;

        // Множество Instance-ключей у которых TextureSurface уже добавлены в MeshObject.
        std::unordered_set<uintptr_t> m_surfaceSyncedParts;

        bool m_initialized = false;
    };

} // namespace Sunover

#pragma warning(pop)
