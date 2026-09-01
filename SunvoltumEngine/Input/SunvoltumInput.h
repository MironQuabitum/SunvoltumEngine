#pragma once

#include "../LibSunvoltum.h"
#include "KeyCode.h"
#include "MouseButton.h"

// MeturmFrame::Input скрыт — клиент не тянет MeturmFrame заголовки
namespace MeturmFrame { class Input; }

namespace Sunvoltum {

    class LibSunvoltum SunvoltumInput
    {
    public:
        SunvoltumInput() = default;

        // Вызывается из Runtime — устанавливает источник ввода из RenderBridge
        void SetSource(MeturmFrame::Input* input);

        bool IsValid() const;

        // --- Клавиатура ---
        bool IsKeyDown    (KeyCode key) const;
        bool IsKeyPressed (KeyCode key) const;
        bool IsKeyReleased(KeyCode key) const;

        // --- Мышь ---
        bool IsMouseDown    (MouseButton btn) const;
        bool IsMousePressed (MouseButton btn) const;
        bool IsMouseReleased(MouseButton btn) const;

        int GetMouseX()      const;
        int GetMouseY()      const;
        int GetMouseDeltaX() const;
        int GetMouseDeltaY() const;
        int GetMouseWheel()  const;

        void SetCursorVisible(bool visible);
        bool IsCursorVisible() const;

        void SetMouseLocked(bool locked);
        bool IsMouseLocked()   const;
        void ResetMouseDelta();
        void SetMousePosition(int x, int y);

    private:
        MeturmFrame::Input* m_source = nullptr;
    };

} // namespace Sunvoltum
