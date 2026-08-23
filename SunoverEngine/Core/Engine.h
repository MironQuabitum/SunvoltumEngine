#pragma once

#include "../LibSunover.h"
#include "../Types/EngineMode.h"
#include "../DataModel/DataModel.h"
#include "../Physics/PhysicsBridge.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunover {

    class LibSunover Engine
    {
    public:
        Engine();
        ~Engine();

        // Engine некопируемый — DataModel содержит unique_ptr
        Engine(const Engine&)            = delete;
        Engine& operator=(const Engine&) = delete;

        // Move разрешён
        Engine(Engine&&)            = default;
        Engine& operator=(Engine&&) = default;

        /// Инициализация движка в указанном режиме
        void Init(EngineMode mode);

        /// Физический тик — вызывать с фиксированным шагом (1/60 с)
        void PhysicsTick(float fixedDelta);

        /// Основной тик — вызывать каждый кадр
        void Tick(float deltaTime);

        /// Завершение работы и освобождение ресурсов
        void Shutdown();

        /// Текущий режим работы движка
        EngineMode GetMode() const;

        /// Корень объектной иерархии
        DataModel DataModel;

        /// Физический мост — доступен для прямого использования если нужно
        PhysicsBridge Physics;

    private:
        EngineMode m_mode        = EngineMode::Standalone;
        bool       m_initialized = false;
    };

} // namespace Sunover

#pragma warning(pop)
