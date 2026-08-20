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
        SyncScene();
        m_renderer->RenderSunLight(*m_sunLight);
        m_renderer->EndFrame();
    }

} // namespace Sunover
