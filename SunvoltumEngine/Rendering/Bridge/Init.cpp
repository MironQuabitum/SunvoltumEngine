#include "BridgeCommon.h"
#include <iostream>

namespace Sunvoltum {

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

        m_camera    = std::make_unique<MeturmRender::Objects::Camera>();
        m_sunLight  = std::make_unique<MeturmRender::Objects::SunLight>();

        float aspect = static_cast<float>(width) / static_cast<float>(height);
        m_camera->SetPerspective(70.0f, aspect, 0.1f, 1000.0f);

        m_initialized = true;

        // Подключаем внутренний объект ввода к окну
        SetInputSource(m_inputObj);

        InitSkyBox();

        // Курсор — загружаем текстуру из PlatformContent
        {
            MeturmRender::Texture cursorTex;
            std::ifstream f("PlatformContent/textures/cursor/ArrowFarCursor.dds",
                            std::ios::binary);
            if (f.is_open())
                cursorTex.LoadTexture(MeturmRender::RenderType::OpenGL, f);

            m_cursor = new MeturmRender::Objects::Cursor(cursorTex);
            m_cursor->SetSize(64.0f, 64.0f);
            m_cursor->SetPosition(0.0f, 0.0f);
            m_cursor->SetOpacity(1.0f);
        }

        // Меш солнца — плоский квад (billboard), лицом по -Z в локальном пространстве.
        // Ориентация поворачивается к камере каждый кадр в SyncLighting.
        // Не добавляется в DataModel, не участвует в физике, не отбрасывает тени.
        {
            // Единичный квад в плоскости XY, центр в (0,0,0), нормаль смотрит по -Z
            // чтобы лицевая сторона Decal была видна со стороны камеры.
            //
            //  (-0.5, +0.5, 0)  ---  (+0.5, +0.5, 0)
            //        |                      |
            //  (-0.5, -0.5, 0)  ---  (+0.5, -0.5, 0)
            //
            const Sunvoltum::Color3 white = { 1.0f, 0.98f, 0.7f };
            const Sunvoltum::Vector3 normFront = { 0.0f, 0.0f,  1.0f }; // +Z = Front для DecalFace
            const Sunvoltum::Vector3 normBack  = { 0.0f, 0.0f, -1.0f }; // обратная сторона

            std::vector<Sunvoltum::Vertex> verts = {
                // лицевая сторона (нормаль +Z)
                { { -0.5f,  0.5f, 0.0f }, normFront, { 0.0f, 0.0f }, white },
                { {  0.5f,  0.5f, 0.0f }, normFront, { 1.0f, 0.0f }, white },
                { {  0.5f, -0.5f, 0.0f }, normFront, { 1.0f, 1.0f }, white },
                { { -0.5f, -0.5f, 0.0f }, normFront, { 0.0f, 1.0f }, white },
                // обратная сторона (нормаль -Z) — отдельные вершины для правильного dot в декали
                { { -0.5f,  0.5f, 0.0f }, normBack, { 0.0f, 0.0f }, white },
                { {  0.5f,  0.5f, 0.0f }, normBack, { 1.0f, 0.0f }, white },
                { {  0.5f, -0.5f, 0.0f }, normBack, { 1.0f, 1.0f }, white },
                { { -0.5f, -0.5f, 0.0f }, normBack, { 0.0f, 1.0f }, white },
            };
            std::vector<uint32_t> idx = {
                0, 1, 2,  0, 2, 3,  // лицевая
                4, 6, 5,  4, 7, 6   // обратная (winding перевёрнут)
            };

            Sunvoltum::Mesh quadMesh(verts, idx);
            auto renderMesh = ToRenderMesh(quadMesh);
            m_sunMesh = std::make_unique<MeturmRender::Objects::MeshObject>(
                std::move(renderMesh));
            m_sunMesh->SetCastShadows(false);
            // Меш полностью прозрачный — вся картинка идёт через декаль.
            // OGLRenderer при opacity=0 пропускает геометрию но рисует декали.
            m_sunMesh->SetTransparency(1.0f);

            // Декаль солнца из PlatformContent
            {
                std::ifstream sunTexStream(
                    "PlatformContent/textures/sky/sun.png", std::ios::binary);
                if (sunTexStream.is_open())
                {
                    std::cout << "[SunMesh] sun.png stream opened OK" << std::endl;
                    auto sunTex = std::make_unique<MeturmRender::Texture>();
                    bool loaded = sunTex->LoadTexture(MeturmRender::RenderType::OpenGL, sunTexStream);
                    std::cout << "[SunMesh] Texture loaded: " << loaded
                              << " IsLoaded: " << sunTex->IsLoaded() << std::endl;

                    if (sunTex->IsLoaded())
                    {
                        MeturmRender::Decal sunDecal(*sunTex,
                            MeturmRender::Enum::DecalFace::Back);
                        sunDecal.SetIgnoreLighting(true);
                        m_sunMesh->AddDecal(sunDecal);
                        std::cout << "[SunMesh] Decal added, decal count: "
                                  << m_sunMesh->GetDecals().size() << std::endl;
                    }
                }
                else
                {
                    std::cout << "[SunMesh] ERROR: failed to open sun.png" << std::endl;
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
            m_moonMesh = std::make_unique<MeturmRender::Objects::MeshObject>(
                std::move(renderMesh));
            m_moonMesh->SetCastShadows(false);
            m_moonMesh->SetTransparency(1.0f);

            // Декаль луны
            {
                std::ifstream moonTexStream(
                    "PlatformContent/textures/sky/moon.png", std::ios::binary);
                if (moonTexStream.is_open())
                {
                    std::cout << "[MoonMesh] moon.jpg stream opened OK" << std::endl;
                    auto moonTex = std::make_unique<MeturmRender::Texture>();
                    bool loaded = moonTex->LoadTexture(MeturmRender::RenderType::OpenGL, moonTexStream);
                    std::cout << "[MoonMesh] Texture loaded: " << loaded
                              << " IsLoaded: " << moonTex->IsLoaded() << std::endl;

                    if (moonTex->IsLoaded())
                    {
                        MeturmRender::Decal moonDecal(*moonTex,
                            MeturmRender::Enum::DecalFace::Back);
                        moonDecal.SetIgnoreLighting(true);
                        m_moonMesh->AddDecal(moonDecal);
                        std::cout << "[MoonMesh] Decal added, decal count: "
                                  << m_moonMesh->GetDecals().size() << std::endl;
                    }
                }
                else
                {
                    std::cout << "[MoonMesh] ERROR: failed to open moon.jpg" << std::endl;
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

        // Отписываемся от ChildAdded Workspace
        m_workspaceChildToken.Disconnect();

        // Уничтожаем SceneEntry (токены отписываются, MeshObject освобождает GPU-буферы)
        m_scene.clear();
        m_sceneOrder.clear();
        m_decalTextureCache.clear();
        m_surfaceTextureCache.clear();
        m_decalSyncedParts.clear();
        m_surfaceSyncedParts.clear();

        m_renderer->Shutdown();
        m_window->Destroy();

        delete m_skyBox;
        m_skyBox = nullptr;

        delete m_skyBoxNight;
        m_skyBoxNight = nullptr;

        delete m_cursor;
        m_cursor = nullptr;

        m_initialized = false;
    }

    bool RenderBridge::IsInitialized() const { return m_initialized; }

    MeturmRender::Window& RenderBridge::GetWindow() { return *m_window; }

    DataModel* RenderBridge::GetDataModel() const { return m_dataModel; }

    void RenderBridge::SetCursorPosition(float x, float y)
    {
        if (m_cursor) m_cursor->SetPosition(x, y);
    }

} // namespace Sunvoltum
