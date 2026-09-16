#pragma once

#include "../LibSunvoltum.h"
#include "LibSunvoltumRender.h"
#include "KeyCode.h"
#include "MouseButton.h"
#include "../Runtime/IInputSource.h"

// SunvoltumManager::Input скрыт — клиент не тянет SunvoltumManager заголовки
namespace SunvoltumManager { class Input; }

namespace Sunvoltum {

    class LibSunvoltumRender SunvoltumInput : public IInputSource
    {
    public:
        SunvoltumInput() = default;

        // Вызывается из RenderBridge — устанавливает источник ввода
        void SetSource(SunvoltumManager::Input* input);

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

        void SetSystemCursorVisible(bool visible)  override;
        bool IsSystemCursorVisible()         const override;
        void SetEngineCursorVisible(bool visible)  override;
        bool IsEngineCursorVisible()         const override;

        int  GetRawMouseDeltaX()             const override;
        int  GetRawMouseDeltaY()             const override;

        void SetMouseLocked(bool locked)     override;
        bool IsMouseLocked()           const override;
        void ResetMouseDelta()               override;
        void SetMousePosition(int x, int y)  override;

    private:
        SunvoltumManager::Input* m_source = nullptr;
    };

} // namespace Sunvoltum
