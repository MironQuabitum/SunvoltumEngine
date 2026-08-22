#include "BridgeCommon.h"

namespace Sunover {

    RenderBridge::RenderBridge()  = default;
    RenderBridge::~RenderBridge() { Shutdown(); }

    bool RenderBridge::Init(Engine& engine, int width, int height, const char* title)
    {
        m_engine    = &engine;
        m_dataModel = &engine.DataModel;

        m_window = std::make_unique<MeturmRender::Window>();
        m_window->SetResolution(width, height);
        m_window->SetTitle(title);

        if (!m_window->Init())
            return false;

        m_renderer = std::make_unique<MeturmRender::Renderer>();

        if (!m_renderer->Init(MeturmRender::RenderType::OpenGL, *m_window))
            return false;

        m_camera   = std::make_unique<MeturmRender::Objects::Camera>();
        m_sunLight = std::make_unique<MeturmRender::Objects::SunLight>();

        float aspect = static_cast<float>(width) / static_cast<float>(height);
        m_camera->SetPerspective(70.0f, aspect, 0.1f, 1000.0f);

        m_initialized = true;

        InitSkyBox();

        return true;
    }

    void RenderBridge::Shutdown()
    {
        if (!m_initialized) return;

        // Уничтожаем кэш мешей до Shutdown() рендерера —
        // MeshObject должен освободить GPU-буферы пока контекст ещё жив.
        m_meshCache.clear();
        m_decalTextureCache.clear();
        m_surfaceTextureCache.clear();
        m_decalSyncedParts.clear();
        m_surfaceSyncedParts.clear();

        m_renderer->Shutdown();
        m_window->Destroy();

        delete m_skyBox;
        m_skyBox = nullptr;

        m_initialized = false;
    }

    bool RenderBridge::IsInitialized() const { return m_initialized; }

} // namespace Sunover
