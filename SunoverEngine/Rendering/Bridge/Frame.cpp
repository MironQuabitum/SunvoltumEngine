#include "BridgeCommon.h"

namespace Sunover {

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
            m_camera->SetPerspective(m_camera->GetFOV(), aspect,
                                     m_camera->GetNearZ(), m_camera->GetFarZ());
            m_window->ClearResizedFlag();
        }

        return !m_window->ShouldClose();
    }

    void RenderBridge::Frame(float deltaTime)
    {
        if (!m_initialized) return;

        SyncCamera();
        SyncLighting();

        m_renderer->BeginFrame();

        // Ночной скайбокс рендерится первым — он фоновый слой.
        // Дневной рендерится поверх него и блендится через transparency.
        if (m_skyBoxNight) m_renderer->RenderSkyBox(*m_skyBoxNight, *m_camera);
        if (m_skyBox)      m_renderer->RenderSkyBox(*m_skyBox,      *m_camera);

        // Меш солнца рендерится до SyncScene — тени от объектов DataModel
        // не затрагивают его (CastShadows=false выставлен при создании).
        if (m_sunMesh) m_renderer->RenderObject(*m_sunMesh, *m_camera);

        SyncScene();
        m_renderer->RenderSunLight(*m_sunLight);

        // Курсор — двигается за мышью когда не залочен
        if (m_cursor)
        {
            auto& input = m_window->GetInput();
            if (!input.IsMouseLocked())
            {
                m_cursor->SetPosition(
                    static_cast<float>(input.GetMouseX()),
                    static_cast<float>(input.GetMouseY())
                );
            }
            m_renderer->RenderCursor(*m_cursor);
        }

        m_renderer->EndFrame();
    }

} // namespace Sunover
