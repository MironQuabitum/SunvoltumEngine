#include "BridgeCommon.h"
#include "../../Input/SunvoltumInput.h"

namespace Sunvoltum {

    int RenderBridge::GetWindowWidth() const
    {
        return m_initialized ? m_window->GetWidth() : 0;
    }

    int RenderBridge::GetWindowHeight() const
    {
        return m_initialized ? m_window->GetHeight() : 0;
    }

    void RenderBridge::UpdateInput()
    {
        if (m_initialized)
            m_window->GetInput().Update();
    }

    void RenderBridge::SetInputSource(SunvoltumInput& target)
    {
        if (m_initialized)
            target.SetSource(&m_window->GetInput());
    }

} // namespace Sunvoltum
