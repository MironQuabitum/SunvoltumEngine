#pragma once

#include <memory>
#include "../LibSunover.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunover {

    class Engine;
    class Instance;

    // SoundEngine — система звука на основе OpenAL + libvorbis.
    //
    // Правила позиционирования:
    //   - Sound дочерний для ShapePart  → 3D источник: затухание по расстоянию
    //     и панорамирование относительно позиции камеры (слушателя).
    //   - Sound дочерний для Workspace  → ambient: звук без позиции,
    //     слышно одинаково везде.
    //
    // Жизненный цикл:
    //   Init()    — вызывается один раз после заполнения DataModel.
    //   Tick(dt)  — вызывается каждый кадр; обновляет позиции 3D источников
    //               и синхронизирует Playing/Volume/Looped с OpenAL.
    //   Shutdown() — освобождает ресурсы OpenAL.
    class LibSunover SoundEngine
    {
    public:
        SoundEngine();
        ~SoundEngine();

        SoundEngine(const SoundEngine&)            = delete;
        SoundEngine& operator=(const SoundEngine&) = delete;

        // Инициализировать OpenAL, пройти по DataModel и зарегистрировать
        // все Sound объекты. Вызывается при первом Tick если ещё не инициализирован.
        bool Init(Engine& engine);

        // Обновить состояние всех звуков за один кадр:
        //   - синхронизировать Playing, Volume, Looped из DataModel в OpenAL
        //   - обновить позицию 3D источников из CFrame родительского ShapePart
        //   - обновить позицию слушателя из CFrame активной камеры
        void Tick(float dt);

        // Освободить все OpenAL ресурсы
        void Shutdown();

        bool IsInitialized() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace Sunover

#pragma warning(pop)
