#pragma once

#include <ostream>

namespace Sunvoltum {

    struct Vector3
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;

        Vector3() = default;
        Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

        Vector3 operator+(const Vector3& o) const { return Vector3(X + o.X, Y + o.Y, Z + o.Z); }
        Vector3 operator-(const Vector3& o) const { return Vector3(X - o.X, Y - o.Y, Z - o.Z); }
        Vector3 operator*(float s)          const { return Vector3(X * s,   Y * s,   Z * s);   }

        static Vector3 Zero()    { return Vector3(0.0f, 0.0f,  0.0f); }
        static Vector3 One()     { return Vector3(1.0f, 1.0f,  1.0f); }
        static Vector3 Up()      { return Vector3(0.0f, 1.0f,  0.0f); }
        static Vector3 Forward() { return Vector3(0.0f, 0.0f, -1.0f); }
        static Vector3 Right()   { return Vector3(1.0f, 0.0f,  0.0f); }
    };

    inline std::ostream& operator<<(std::ostream& os, const Vector3& v)
    {
        return os << '(' << v.X << ", " << v.Y << ", " << v.Z << ')';
    }

} // namespace Sunvoltum
