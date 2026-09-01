#include "SunvoltumInput.h"
#include <MeturmFrame/Interface/Input.h>
#include <MeturmFrame/Input/KeyCode.h>
#include <MeturmFrame/Input/MouseButton.h>

namespace Sunvoltum {

    // Sunvoltum::KeyCode значения совпадают с MeturmFrame::KeyCode — прямой каст
    static MeturmFrame::KeyCode ToMF(KeyCode key)
    {
        return static_cast<MeturmFrame::KeyCode>(key);
    }

    static MeturmFrame::MouseButton ToMF(MouseButton btn)
    {
        return static_cast<MeturmFrame::MouseButton>(btn);
    }

    void SunvoltumInput::SetSource(MeturmFrame::Input* input) { m_source = input; }
    bool SunvoltumInput::IsValid() const { return m_source != nullptr; }

    bool SunvoltumInput::IsKeyDown    (KeyCode key) const { return m_source && m_source->IsKeyDown    (ToMF(key)); }
    bool SunvoltumInput::IsKeyPressed (KeyCode key) const { return m_source && m_source->IsKeyPressed (ToMF(key)); }
    bool SunvoltumInput::IsKeyReleased(KeyCode key) const { return m_source && m_source->IsKeyReleased(ToMF(key)); }

    bool SunvoltumInput::IsMouseDown    (MouseButton btn) const { return m_source && m_source->IsMouseDown    (ToMF(btn)); }
    bool SunvoltumInput::IsMousePressed (MouseButton btn) const { return m_source && m_source->IsMousePressed (ToMF(btn)); }
    bool SunvoltumInput::IsMouseReleased(MouseButton btn) const { return m_source && m_source->IsMouseReleased(ToMF(btn)); }

    int SunvoltumInput::GetMouseX()      const { return m_source ? m_source->GetMouseX()          : 0; }
    int SunvoltumInput::GetMouseY()      const { return m_source ? m_source->GetMouseY()          : 0; }
    int SunvoltumInput::GetMouseDeltaX() const { return m_source ? m_source->GetMouseDeltaX()     : 0; }
    int SunvoltumInput::GetMouseDeltaY() const { return m_source ? m_source->GetMouseDeltaY()     : 0; }
    int SunvoltumInput::GetMouseWheel()  const { return m_source ? m_source->GetMouseWheelDelta() : 0; }

    void SunvoltumInput::SetCursorVisible(bool v)    { if (m_source) m_source->SetCursorVisible(v); }
    bool SunvoltumInput::IsCursorVisible() const     { return m_source ? m_source->IsCursorVisible() : true; }

    void SunvoltumInput::SetMouseLocked(bool v)      { if (m_source) m_source->SetMouseLocked(v); }
    bool SunvoltumInput::IsMouseLocked() const       { return m_source ? m_source->IsMouseLocked() : false; }
    void SunvoltumInput::ResetMouseDelta()           { if (m_source) m_source->ResetMouseDelta(); }
    void SunvoltumInput::SetMousePosition(int x, int y) { if (m_source) m_source->SetMousePosition(x, y); }

} // namespace Sunvoltum
