#pragma once

#include "../LibSunvoltum.h"
#include "../Input/KeyCode.h"
#include "../Input/MouseButton.h"

namespace Sunvoltum {

    // -------------------------------------------------------------------------
    // IInputSource — абстракция ввода для Runtime.
    // Runtime не знает про SunvoltumInput или MeturmFrame::Input —
    // только этот интерфейс. Реализуется в LibSunvoltumRender.
    // -------------------------------------------------------------------------
    class LibSunvoltum IInputSource
    {
    public:
        virtual ~IInputSource() = default;

        // Клавиатура
        virtual bool IsKeyDown    (KeyCode key) const = 0;
        virtual bool IsKeyPressed (KeyCode key) const = 0;
        virtual bool IsKeyReleased(KeyCode key) const = 0;

        // Мышь — кнопки
        virtual bool IsMouseDown    (MouseButton btn) const = 0;
        virtual bool IsMousePressed (MouseButton btn) const = 0;
        virtual bool IsMouseReleased(MouseButton btn) const = 0;

        // Мышь — позиция и дельта
        virtual int GetMouseX()      const = 0;
        virtual int GetMouseY()      const = 0;
        virtual int GetMouseDeltaX() const = 0;
        virtual int GetMouseDeltaY() const = 0;
        virtual int GetMouseWheel()  const = 0;

        // Захват / видимость курсоров
        virtual void SetSystemCursorVisible(bool visible) = 0;
        virtual bool IsSystemCursorVisible() const = 0;

        virtual void SetEngineCursorVisible(bool visible) = 0;
        virtual bool IsEngineCursorVisible() const = 0;

        // Совместимость
        virtual void SetCursorVisible(bool visible) { SetEngineCursorVisible(visible); }
        virtual bool IsCursorVisible() const { return IsEngineCursorVisible(); }

        virtual int  GetRawMouseDeltaX() const = 0;
        virtual int  GetRawMouseDeltaY() const = 0;

        virtual void SetMouseLocked(bool locked) = 0;
        virtual bool IsMouseLocked() const = 0;
        virtual void ResetMouseDelta() = 0;
        virtual void SetMousePosition(int x, int y) = 0;
    };

} // namespace Sunvoltum
