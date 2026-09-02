#pragma once

#include "../LibSunvoltum.h"
#include "LibSunvoltumRender.h"
#include "KeyCode.h"
#include "MouseButton.h"
#include "../Runtime/IInputSource.h"

// MeturmFrame::Input скрыт — клиент не тянет MeturmFrame заголовки
namespace MeturmFrame { class Input; }

namespace Sunvoltum {

    class LibSunvoltumRender SunvoltumInput : public IInputSource
    {
    public:
        SunvoltumInput() = default;

        // Вызывается из RenderBridge — устанавливает источник ввода
        void SetSource(MeturmFrame::Input* input);

        bool IsValid() const;

        // IInputSource
        bool IsKeyDown    (KeyCode key)     const override;
        bool IsKeyPressed (KeyCode key)     const override;
        bool IsKeyReleased(KeyCode key)     const override;

        bool IsMouseDown    (MouseButton btn) const override;
        bool IsMousePressed (MouseButton btn) const override;
        bool IsMouseReleased(MouseButton btn) const override;

        int  GetMouseX()      const override;
        int  GetMouseY()      const override;
        int  GetMouseDeltaX() const override;
        int  GetMouseDeltaY() const override;
        int  GetMouseWheel()  const override;

        void SetCursorVisible(bool visible)  override;
        bool IsCursorVisible()         const override;
        void SetMouseLocked(bool locked)     override;
        bool IsMouseLocked()           const override;
        void ResetMouseDelta()               override;
        void SetMousePosition(int x, int y)  override;

    private:
        MeturmFrame::Input* m_source = nullptr;
    };

} // namespace Sunvoltum
