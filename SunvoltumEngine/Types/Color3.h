#pragma once

namespace Sunvoltum {

    struct Color3
    {
        float R = 0.0f;
        float G = 0.0f;
        float B = 0.0f;

        Color3() = default;
        Color3(float r, float g, float b) : R(r), G(g), B(b) {}
    };

} // namespace Sunvoltum
