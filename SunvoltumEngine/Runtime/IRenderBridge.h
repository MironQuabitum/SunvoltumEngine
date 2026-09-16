#pragma once

#include "../LibSunvoltum.h"

namespace Sunvoltum {

    class DataModel;

    // -------------------------------------------------------------------------
    // IRenderBridge — абстракция рендер-моста для Runtime.
    // Runtime не знает про конкретный RenderBridge, MeturmRender или окно —
    // только этот интерфейс. Реализуется в LibSunvoltumRender.
    // -------------------------------------------------------------------------
    class LibSunvoltum IRenderBridge
    {
    public:
        virtual ~IRenderBridge() = default;

        // Инициализирован ли бридж (окно создано, рендерер готов)
        virtual bool IsInitialized() const = 0;

        // Обновить состояние ввода (опрос MeturmFrame)
        virtual void UpdateInput() = 0;

        // Обработать оконные события. Возвращает false если окно закрыто.
        virtual bool PollEvents() = 0;

        // Отрисовать кадр
        virtual void Frame(float deltaTime) = 0;

        // Получить DataModel привязанный к бриджу
        virtual DataModel* GetDataModel() const = 0;

        // Размеры окна
        virtual int GetWindowWidth()  const = 0;
        virtual int GetWindowHeight() const = 0;

        // Переместить системный курсор в экранные координаты
        virtual void SetCursorPosition(float x, float y) = 0;

        // Задать позицию курсора движка
        virtual void SetEngineCursorPosition(float x, float y) = 0;
    };

} // namespace Sunvoltum
