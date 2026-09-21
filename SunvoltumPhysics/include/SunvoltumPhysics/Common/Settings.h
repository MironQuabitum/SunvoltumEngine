#pragma once

#include <cstdint>

namespace SunvoltumPhysics {

    struct Settings {
        // Сила гравитации по умолчанию (м/с^2)
        static constexpr float DEFAULT_GRAVITY_Y = -9.81f;

        // Число итераций PGS солвера для импульсов
        static constexpr uint32_t VELOCITY_ITERATIONS = 10;
        
        // Коэффициент стабилизации Баумгарте (0.1 - 0.3)
        static constexpr float BAUMGARTE_FACTOR = 0.2f;

        // Допустимое проникновение без коррекции положения (slop)
        static constexpr float PENETRATION_SLOP = 0.005f;

        // Максимальная скорость коррекции Баумгарте
        static constexpr float MAX_PENETRATION_CORRECTION = 5.0f;

        // Порог скорости для перехода в спящий режим
        static constexpr float SLEEP_LINEAR_VELOCITY_SQ = 0.01f * 0.01f;
        static constexpr float SLEEP_ANGULAR_VELOCITY_SQ = 0.01f * 0.01f;
        static constexpr float TIME_TO_SLEEP = 0.5f; // секунды неподвижности
    };

} // namespace SunvoltumPhysics
