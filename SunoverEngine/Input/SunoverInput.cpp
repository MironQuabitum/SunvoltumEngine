#include "SunoverInput.h"
#include <MeturmFrame/Interface/Input.h>
#include <MeturmFrame/Input/KeyCode.h>
#include <MeturmFrame/Input/MouseButton.h>

namespace Sunover {

    // Sunover::KeyCode значения совпадают с MeturmFrame::KeyCode — прямой каст
    static MeturmFrame::KeyCode ToMF(KeyCode key)
    {
        return static_cast<MeturmFrame::KeyCode>(key);
    }

    static MeturmFrame::MouseButton ToMF(MouseButton btn)
    {
        return static_cast<MeturmFrame::MouseButton>(btn);
    }

    void SunoverInput::SetSource(MeturmFrame::Input* input) { m_source = input; }
    bool SunoverInput::IsValid() const { return m_source != nullptr; }

    bool SunoverInput::IsKeyDown    (KeyCode key) const { return m_source && m_source->IsKeyDown    (ToMF(key)); }
    bool SunoverInput::IsKeyPressed (KeyCode key) const { return m_source && m_source->IsKeyPressed (ToMF(key)); }
    bool SunoverInput::IsKeyReleased(KeyCode key) const { return m_source && m_source->IsKeyReleased(ToMF(key)); }

    bool SunoverInput::IsMouseDown    (MouseButton btn) const { return m_source && m_source->IsMouseDown    (ToMF(btn)); }
    bool SunoverInput::IsMousePressed (MouseButton btn) const { return m_source && m_source->IsMousePressed (ToMF(btn)); }
    bool SunoverInput::IsMouseReleased(MouseButton btn) const { return m_source && m_source->IsMouseReleased(ToMF(btn)); }

    int SunoverInput::GetMouseX()      const { return m_source ? m_source->GetMouseX()          : 0; }
    int SunoverInput::GetMouseY()      const { return m_source ? m_source->GetMouseY()          : 0; }
    int SunoverInput::GetMouseDeltaX() const { return m_source ? m_source->GetMouseDeltaX()     : 0; }
    int SunoverInput::GetMouseDeltaY() const { return m_source ? m_source->GetMouseDeltaY()     : 0; }
    int SunoverInput::GetMouseWheel()  const { return m_source ? m_source->GetMouseWheelDelta() : 0; }

    void SunoverInput::SetCursorVisible(bool v)    { if (m_source) m_source->SetCursorVisible(v); }
    bool SunoverInput::IsCursorVisible() const     { return m_source ? m_source->IsCursorVisible() : true; }

    void SunoverInput::SetMouseLocked(bool v)      { if (m_source) m_source->SetMouseLocked(v); }
    bool SunoverInput::IsMouseLocked() const       { return m_source ? m_source->IsMouseLocked() : false; }
    void SunoverInput::ResetMouseDelta()           { if (m_source) m_source->ResetMouseDelta(); }
    void SunoverInput::SetMousePosition(int x, int y) { if (m_source) m_source->SetMousePosition(x, y); }

} // namespace Sunover
