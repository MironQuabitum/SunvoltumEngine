#include "BridgeCommon.h"
#include <iostream>

namespace Sunvoltum {

    RenderBridge::RenderBridge()  = default;
    RenderBridge::~RenderBridge() { Shutdown(); }

    bool RenderBridge::Init(Engine& engine, int width, int height, const char* title)
    {
        m_engine    = &engine;
        m_dataModel = &engine.DataModel;

        SunvoltumManager::WindowConfig config;
        config.width = width;
        config.height = height;
        config.title = title;
        config.fullscreen = false;

        m_window = SunvoltumManager::WindowManager::Create();
        if (!m_window || !m_window->Init(config))
            return false;

        m_renderer = std::make_unique<SunvoltumRender::Renderer>();

        if (!m_renderer->Init(SunvoltumRender::RenderType::Direct3D9, *m_window))
            return false;

        m_camera   = std::make_unique<SunvoltumRender::Objects::Camera>();
        m_sunLight = std::make_unique<SunvoltumRender::Objects::DirectionalLight>();

        float aspect = static_cast<float>(width) / static_cast<float>(height);
        m_camera->SetPerspective(70.0f, aspect, 0.1f, 1000.0f);

        m_initialized = true;

        // Подключаем внутренний объект ввода к окну
        SetInputSource(m_inputObj);

        InitSkyBox();

        // 2D курсор
        {
            SunvoltumRender::Texture cursorTex;
            std::ifstream cursorStream("PlatformContent/textures/Cursor/ArrowFarCursor.png", std::ios::binary);
            if (cursorStream.is_open())
                cursorTex.LoadStream(cursorStream);
            if (!cursorTex.IsLoaded())
            {
                std::ifstream cursorDds("PlatformContent/textures/Cursor/ArrowFarCursor.dds", std::ios::binary);
                if (cursorDds.is_open())
                    cursorTex.LoadStream(cursorDds);
            }

            if (cursorTex.IsLoaded())
            {
                m_cursor = std::make_unique<SunvoltumRender::Objects::Cursor>(cursorTex);
                m_cursor->SetSize(64.0f, 64.0f);
                m_cursor->SetPosition(static_cast<float>(width) * 0.5f, static_cast<float>(height) * 0.5f);
                m_cursor->SetOpacity(1.0f);
            }
        }

        // Меш солнца — плоский квад (billboard), лицом по -Z в локальном пространстве.
        // Ориентация поворачивается к камере каждый кадр в SyncLighting.
        // Не добавляется в DataModel, не участвует в физике, не отбрасывает тени.
        {
            const Sunvoltum::Color3 white = { 1.0f, 0.98f, 0.7f };
            const Sunvoltum::Vector3 normFront = { 0.0f, 0.0f,  1.0f }; // +Z = Front для DecalFace
            const Sunvoltum::Vector3 normBack  = { 0.0f, 0.0f, -1.0f }; // обратная сторона

            std::vector<Sunvoltum::Vertex> verts = {
                // лицевая сторона (нормаль +Z)
                { { -0.5f,  0.5f, 0.0f }, normFront, { 0.0f, 0.0f }, white },
                { {  0.5f,  0.5f, 0.0f }, normFront, { 1.0f, 0.0f }, white },
                { {  0.5f, -0.5f, 0.0f }, normFront, { 1.0f, 1.0f }, white },
                { { -0.5f, -0.5f, 0.0f }, normFront, { 0.0f, 1.0f }, white },
                // обратная сторона (нормаль -Z)
                { { -0.5f,  0.5f, 0.0f }, normBack,  { 0.0f, 0.0f }, white },
                { {  0.5f,  0.5f, 0.0f }, normBack,  { 1.0f, 0.0f }, white },
                { {  0.5f, -0.5f, 0.0f }, normBack,  { 1.0f, 1.0f }, white },
                { { -0.5f, -0.5f, 0.0f }, normBack,  { 0.0f, 1.0f }, white },
            };
            std::vector<uint32_t> idx = {
                0, 1, 2,  0, 2, 3,  // лицевая
                4, 6, 5,  4, 7, 6   // обратная
            };

            Sunvoltum::Mesh quadMesh(verts, idx);
            auto renderMesh = ToRenderMesh(quadMesh);
            m_sunMesh = std::make_unique<SunvoltumRender::Objects::MeshObject>(
                std::move(renderMesh));
            m_sunMesh->SetTransparency(1.0f);

            // Декаль солнца из PlatformContent
            {
                std::ifstream sunTexStream(
                    "PlatformContent/textures/sky/sun.png", std::ios::binary);
                if (sunTexStream.is_open())
                {
                    auto sunTex = std::make_unique<SunvoltumRender::Texture>();
                    if (sunTex->LoadStream(sunTexStream) && sunTex->IsLoaded())
                    {
                        SunvoltumRender::Objects::Decal sunDecal(*sunTex,
                            SunvoltumRender::Enum::DecalFace::Back);
                        sunDecal.SetIgnoreLighting(true);
                        m_sunMesh->AddDecal(sunDecal);
                    }
                }
            }
        }

        // Меш луны — аналогичный billboard-квад, белый оттенок.
        {
            const Sunvoltum::Color3 white = { 1.0f, 1.0f, 1.0f };
            const Sunvoltum::Vector3 normFront = { 0.0f, 0.0f,  1.0f };
            const Sunvoltum::Vector3 normBack  = { 0.0f, 0.0f, -1.0f };

            std::vector<Sunvoltum::Vertex> verts = {
                { { -0.5f,  0.5f, 0.0f }, normFront, { 0.0f, 0.0f }, white },
                { {  0.5f,  0.5f, 0.0f }, normFront, { 1.0f, 0.0f }, white },
                { {  0.5f, -0.5f, 0.0f }, normFront, { 1.0f, 1.0f }, white },
                { { -0.5f, -0.5f, 0.0f }, normFront, { 0.0f, 1.0f }, white },
                { { -0.5f,  0.5f, 0.0f }, normBack,  { 0.0f, 0.0f }, white },
                { {  0.5f,  0.5f, 0.0f }, normBack,  { 1.0f, 0.0f }, white },
                { {  0.5f, -0.5f, 0.0f }, normBack,  { 1.0f, 1.0f }, white },
                { { -0.5f, -0.5f, 0.0f }, normBack,  { 0.0f, 1.0f }, white },
            };
            std::vector<uint32_t> idx = {
                0, 1, 2,  0, 2, 3,
                4, 6, 5,  4, 7, 6
            };

            Sunvoltum::Mesh quadMesh(verts, idx);
            auto renderMesh = ToRenderMesh(quadMesh);
            m_moonMesh = std::make_unique<SunvoltumRender::Objects::MeshObject>(
                std::move(renderMesh));
            m_moonMesh->SetTransparency(1.0f);

            // Декаль луны
            {
                std::ifstream moonTexStream(
                    "PlatformContent/textures/sky/moon.png", std::ios::binary);
                if (moonTexStream.is_open())
                {
                    auto moonTex = std::make_unique<SunvoltumRender::Texture>();
                    if (moonTex->LoadStream(moonTexStream) && moonTex->IsLoaded())
                    {
                        SunvoltumRender::Objects::Decal moonDecal(*moonTex,
                            SunvoltumRender::Enum::DecalFace::Back);
                        moonDecal.SetIgnoreLighting(true);
                        m_moonMesh->AddDecal(moonDecal);
                    }
                }
            }
        }

        // Подписываемся на Lighting, Camera и Workspace (ChildAdded)
        SubscribeLighting();
        SubscribeCamera();
        SubscribeWorkspace();

        return true;
    }

    void RenderBridge::Shutdown()
    {
        if (!m_initialized) return;

        // Уничтожаем меши солнца и луны до Shutdown() рендерера
        m_sunMesh.reset();
        m_moonMesh.reset();
        m_cursor.reset();

        // Отписываемся от ChildAdded Workspace
        m_workspaceChildToken.Disconnect();

        // Уничтожаем SceneEntry
        m_scene.clear();
        m_sceneOrder.clear();
        m_decalTextureCache.clear();
        m_surfaceTextureCache.clear();
        m_decalSyncedParts.clear();
        m_surfaceSyncedParts.clear();

        if (m_renderer)
            m_renderer->Shutdown();
        if (m_window)
            m_window->Destroy();

        m_skyBox.reset();
        m_skyBoxNight.reset();

        m_initialized = false;
    }

    bool RenderBridge::IsInitialized() const { return m_initialized; }

    SunvoltumManager::IPlatformWindow* RenderBridge::GetWindow() { return m_window.get(); }

    DataModel* RenderBridge::GetDataModel() const { return m_dataModel; }

    void RenderBridge::SetCursorPosition(float x, float y)
    {
        if (m_window)
            m_window->GetInput().SetMousePosition(static_cast<int>(x), static_cast<int>(y));
    }

    void RenderBridge::SetEngineCursorPosition(float x, float y)
    {
        if (m_cursor)
            m_cursor->SetPosition(x, y);
    }

} // namespace Sunvoltum
