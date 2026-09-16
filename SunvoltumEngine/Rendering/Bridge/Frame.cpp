#include "BridgeCommon.h"

namespace Sunvoltum {

    bool RenderBridge::PollEvents()
    {
        if (!m_initialized) return false;

        m_window->PollEvents();

        if (m_window->WasResized())
        {
            int w = m_window->GetWidth();
            int h = m_window->GetHeight();
            m_renderer->Resize(w, h);
            float aspect = static_cast<float>(w) / static_cast<float>(h);
            m_camera->SetPerspective(m_camera->GetFov(), aspect,
                                     m_camera->GetNearPlane(), m_camera->GetFarPlane());
            m_window->ClearResizedFlag();
        }

        return !m_window->ShouldClose();
    }

    void RenderBridge::Frame(float deltaTime)
    {
        if (!m_initialized) return;

        m_totalTime += deltaTime;

        SyncCamera();
        SyncLighting();

        m_renderer->BeginFrame();

        // Дневной скайбокс рендерится первым.
        // Ночной рендерится поверх — когда его alpha=1 он перекрывает дневной.
        if (m_skyBox)      m_renderer->RenderSkyBox(*m_skyBox,      *m_camera);
        if (m_skyBoxNight) m_renderer->RenderSkyBox(*m_skyBoxNight, *m_camera);

        // Меш солнца рендерится после скайбоксов и до SyncScene.
        if (m_sunMesh)  m_renderer->RenderObject(*m_sunMesh,  *m_camera);
        // Меш луны — аналогично, противоположная сторона неба.
        if (m_moonMesh) m_renderer->RenderObject(*m_moonMesh, *m_camera);

        SyncScene(deltaTime);

        // 2D Курсор движка поверх сцены (позиция задаётся программно, видимость не зависит от системного курсора)
        if (m_cursor && m_window)
        {
            auto& input = m_window->GetInput();
            if (input.IsEngineCursorVisible() && m_cursor->IsVisible())
            {
                m_renderer->RenderCursor(*m_cursor);
            }
        }

        m_renderer->EndFrame();
    }

} // namespace Sunvoltum
