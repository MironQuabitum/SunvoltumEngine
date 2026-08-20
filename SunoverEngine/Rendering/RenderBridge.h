#pragma once

#include <memory>

#include "../LibSunover.h"

namespace MeturmRender {
    class Renderer;
    class Window;
    namespace Objects {
        class Camera;
        class SunLight;
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

        Engine*                                          m_engine    = nullptr;
        DataModel*                                       m_dataModel = nullptr;
        std::unique_ptr<MeturmRender::Window>            m_window;
        std::unique_ptr<MeturmRender::Renderer>          m_renderer;
        std::unique_ptr<MeturmRender::Objects::Camera>   m_camera;
        std::unique_ptr<MeturmRender::Objects::SunLight> m_sunLight;

        bool m_initialized = false;
    };

} // namespace Sunover

#pragma warning(pop)
