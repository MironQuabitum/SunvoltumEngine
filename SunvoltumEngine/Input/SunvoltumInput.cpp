#include "SunvoltumInput.h"
#include <SunvoltumManager/Input/Input.h>
#include <SunvoltumManager/Input/InputCodes.h>

namespace Sunvoltum {

    static SunvoltumManager::KeyCode ToManagerKey(KeyCode key)
    {
        switch (key)
        {
            case KeyCode::A: return SunvoltumManager::KeyCode::A;
            case KeyCode::B: return SunvoltumManager::KeyCode::B;
            case KeyCode::C: return SunvoltumManager::KeyCode::C;
            case KeyCode::D: return SunvoltumManager::KeyCode::D;
            case KeyCode::E: return SunvoltumManager::KeyCode::E;
            case KeyCode::F: return SunvoltumManager::KeyCode::F;
            case KeyCode::G: return SunvoltumManager::KeyCode::G;
            case KeyCode::H: return SunvoltumManager::KeyCode::H;
            case KeyCode::I: return SunvoltumManager::KeyCode::I;
            case KeyCode::J: return SunvoltumManager::KeyCode::J;
            case KeyCode::K: return SunvoltumManager::KeyCode::K;
            case KeyCode::L: return SunvoltumManager::KeyCode::L;
            case KeyCode::M: return SunvoltumManager::KeyCode::M;
            case KeyCode::N: return SunvoltumManager::KeyCode::N;
            case KeyCode::O: return SunvoltumManager::KeyCode::O;
            case KeyCode::P: return SunvoltumManager::KeyCode::P;
            case KeyCode::Q: return SunvoltumManager::KeyCode::Q;
            case KeyCode::R: return SunvoltumManager::KeyCode::R;
            case KeyCode::S: return SunvoltumManager::KeyCode::S;
            case KeyCode::T: return SunvoltumManager::KeyCode::T;
            case KeyCode::U: return SunvoltumManager::KeyCode::U;
            case KeyCode::V: return SunvoltumManager::KeyCode::V;
            case KeyCode::W: return SunvoltumManager::KeyCode::W;
            case KeyCode::X: return SunvoltumManager::KeyCode::X;
            case KeyCode::Y: return SunvoltumManager::KeyCode::Y;
            case KeyCode::Z: return SunvoltumManager::KeyCode::Z;

            case KeyCode::D0: return SunvoltumManager::KeyCode::Num0;
            case KeyCode::D1: return SunvoltumManager::KeyCode::Num1;
            case KeyCode::D2: return SunvoltumManager::KeyCode::Num2;
            case KeyCode::D3: return SunvoltumManager::KeyCode::Num3;
            case KeyCode::D4: return SunvoltumManager::KeyCode::Num4;
            case KeyCode::D5: return SunvoltumManager::KeyCode::Num5;
            case KeyCode::D6: return SunvoltumManager::KeyCode::Num6;
            case KeyCode::D7: return SunvoltumManager::KeyCode::Num7;
            case KeyCode::D8: return SunvoltumManager::KeyCode::Num8;
            case KeyCode::D9: return SunvoltumManager::KeyCode::Num9;

            case KeyCode::Escape:       return SunvoltumManager::KeyCode::Escape;
            case KeyCode::Enter:        return SunvoltumManager::KeyCode::Enter;
            case KeyCode::Space:        return SunvoltumManager::KeyCode::Space;
            case KeyCode::Tab:          return SunvoltumManager::KeyCode::Tab;
            case KeyCode::Backspace:    return SunvoltumManager::KeyCode::Backspace;

            case KeyCode::Shift:
            case KeyCode::LeftShift:    return SunvoltumManager::KeyCode::LeftShift;
            case KeyCode::RightShift:   return SunvoltumManager::KeyCode::RightShift;
            case KeyCode::Control:
            case KeyCode::LeftControl:  return SunvoltumManager::KeyCode::LeftControl;
            case KeyCode::RightControl: return SunvoltumManager::KeyCode::RightControl;
            case KeyCode::Alt:
            case KeyCode::LeftAlt:      return SunvoltumManager::KeyCode::LeftAlt;
            case KeyCode::RightAlt:     return SunvoltumManager::KeyCode::RightAlt;

            case KeyCode::Left:         return SunvoltumManager::KeyCode::Left;
            case KeyCode::Right:        return SunvoltumManager::KeyCode::Right;
            case KeyCode::Up:           return SunvoltumManager::KeyCode::Up;
            case KeyCode::Down:         return SunvoltumManager::KeyCode::Down;

            case KeyCode::F1:  return SunvoltumManager::KeyCode::F1;
            case KeyCode::F2:  return SunvoltumManager::KeyCode::F2;
            case KeyCode::F3:  return SunvoltumManager::KeyCode::F3;
            case KeyCode::F4:  return SunvoltumManager::KeyCode::F4;
            case KeyCode::F5:  return SunvoltumManager::KeyCode::F5;
            case KeyCode::F6:  return SunvoltumManager::KeyCode::F6;
            case KeyCode::F7:  return SunvoltumManager::KeyCode::F7;
            case KeyCode::F8:  return SunvoltumManager::KeyCode::F8;
            case KeyCode::F9:  return SunvoltumManager::KeyCode::F9;
            case KeyCode::F10: return SunvoltumManager::KeyCode::F10;
            case KeyCode::F11: return SunvoltumManager::KeyCode::F11;
            case KeyCode::F12: return SunvoltumManager::KeyCode::F12;

            default: return SunvoltumManager::KeyCode::Unknown;
        }
    }

    static SunvoltumManager::MouseButton ToManagerMouse(MouseButton btn)
    {
        switch (btn)
        {
            case MouseButton::Left:   return SunvoltumManager::MouseButton::Left;
            case MouseButton::Right:  return SunvoltumManager::MouseButton::Right;
            case MouseButton::Middle: return SunvoltumManager::MouseButton::Middle;
            default:                  return SunvoltumManager::MouseButton::Left;
        }
    }

    void SunvoltumInput::SetSource(SunvoltumManager::Input* input) { m_source = input; }
    bool SunvoltumInput::IsValid() const { return m_source != nullptr; }

    bool SunvoltumInput::IsKeyDown(KeyCode key) const
    {
        if (!m_source) return false;
        if (key == KeyCode::Shift)
            return m_source->IsKeyDown(SunvoltumManager::KeyCode::LeftShift) ||
                   m_source->IsKeyDown(SunvoltumManager::KeyCode::RightShift);
        if (key == KeyCode::Control)
            return m_source->IsKeyDown(SunvoltumManager::KeyCode::LeftControl) ||
                   m_source->IsKeyDown(SunvoltumManager::KeyCode::RightControl);
        if (key == KeyCode::Alt)
            return m_source->IsKeyDown(SunvoltumManager::KeyCode::LeftAlt) ||
                   m_source->IsKeyDown(SunvoltumManager::KeyCode::RightAlt);
        return m_source->IsKeyDown(ToManagerKey(key));
    }

    bool SunvoltumInput::IsKeyPressed(KeyCode key) const
    {
        if (!m_source) return false;
        if (key == KeyCode::Shift)
            return m_source->IsKeyPressed(SunvoltumManager::KeyCode::LeftShift) ||
                   m_source->IsKeyPressed(SunvoltumManager::KeyCode::RightShift);
        if (key == KeyCode::Control)
            return m_source->IsKeyPressed(SunvoltumManager::KeyCode::LeftControl) ||
                   m_source->IsKeyPressed(SunvoltumManager::KeyCode::RightControl);
        if (key == KeyCode::Alt)
            return m_source->IsKeyPressed(SunvoltumManager::KeyCode::LeftAlt) ||
                   m_source->IsKeyPressed(SunvoltumManager::KeyCode::RightAlt);
        return m_source->IsKeyPressed(ToManagerKey(key));
    }

    bool SunvoltumInput::IsKeyReleased(KeyCode key) const
    {
        if (!m_source) return false;
        if (key == KeyCode::Shift)
            return m_source->IsKeyReleased(SunvoltumManager::KeyCode::LeftShift) ||
                   m_source->IsKeyReleased(SunvoltumManager::KeyCode::RightShift);
        if (key == KeyCode::Control)
            return m_source->IsKeyReleased(SunvoltumManager::KeyCode::LeftControl) ||
                   m_source->IsKeyReleased(SunvoltumManager::KeyCode::RightControl);
        if (key == KeyCode::Alt)
            return m_source->IsKeyReleased(SunvoltumManager::KeyCode::LeftAlt) ||
                   m_source->IsKeyReleased(SunvoltumManager::KeyCode::RightAlt);
        return m_source->IsKeyReleased(ToManagerKey(key));
    }

    bool SunvoltumInput::IsMouseDown    (MouseButton btn) const { return m_source && m_source->IsMouseDown    (ToManagerMouse(btn)); }
    bool SunvoltumInput::IsMousePressed (MouseButton btn) const { return m_source && m_source->IsMousePressed (ToManagerMouse(btn)); }
    bool SunvoltumInput::IsMouseReleased(MouseButton btn) const { return m_source && m_source->IsMouseReleased(ToManagerMouse(btn)); }

    int SunvoltumInput::GetMouseX()      const { return m_source ? m_source->GetMouseX()      : 0; }
    int SunvoltumInput::GetMouseY()      const { return m_source ? m_source->GetMouseY()      : 0; }
    int SunvoltumInput::GetMouseDeltaX() const { return m_source ? m_source->GetMouseDeltaX() : 0; }
    int SunvoltumInput::GetMouseDeltaY() const { return m_source ? m_source->GetMouseDeltaY() : 0; }
    int SunvoltumInput::GetMouseWheel()  const { return m_source ? m_source->GetMouseWheel()  : 0; }

    void SunvoltumInput::SetSystemCursorVisible(bool v) { if (m_source) m_source->SetSystemCursorVisible(v); }
    bool SunvoltumInput::IsSystemCursorVisible() const  { return m_source ? m_source->IsSystemCursorVisible() : true; }

    void SunvoltumInput::SetEngineCursorVisible(bool v) { if (m_source) m_source->SetEngineCursorVisible(v); }
    bool SunvoltumInput::IsEngineCursorVisible() const  { return m_source ? m_source->IsEngineCursorVisible() : true; }

    int  SunvoltumInput::GetRawMouseDeltaX()     const  { return m_source ? m_source->GetRawMouseDeltaX() : 0; }
    int  SunvoltumInput::GetRawMouseDeltaY()     const  { return m_source ? m_source->GetRawMouseDeltaY() : 0; }

    void SunvoltumInput::SetMouseLocked(bool v)      { if (m_source) m_source->SetMouseLocked(v); }
    bool SunvoltumInput::IsMouseLocked() const       { return m_source ? m_source->IsMouseLocked() : false; }
    void SunvoltumInput::ResetMouseDelta()           { if (m_source) m_source->ResetMouseDelta(); }
    void SunvoltumInput::SetMousePosition(int x, int y) { if (m_source) m_source->SetMousePosition(x, y); }

} // namespace Sunvoltum
