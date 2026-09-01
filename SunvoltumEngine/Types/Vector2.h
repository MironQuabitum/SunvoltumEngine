#pragma once

namespace Sunvoltum {

    struct Vector2
    {
        float X = 0.0f;
        float Y = 0.0f;

        Vector2() = default;
        Vector2(float x, float y) : X(x), Y(y) {}

        Vector2 operator+(const Vector2& o) const { return Vector2(X + o.X, Y + o.Y); }
        Vector2 operator-(const Vector2& o) const { return Vector2(X - o.X, Y - o.Y); }
        Vector2 operator*(float s)          const { return Vector2(X * s,   Y * s);   }

        static Vector2 Zero() { return Vector2(0.0f, 0.0f); }
        static Vector2 One()  { return Vector2(1.0f, 1.0f); }
    };

} // namespace Sunvoltum
