#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace SunvoltumPhysics {

    constexpr float PI = 3.14159265358979323846f;
    constexpr float EPSILON = 1e-6f;

    struct Vector3 {
        float x{0.0f}, y{0.0f}, z{0.0f};

        constexpr Vector3() = default;
        constexpr Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

        Vector3 operator+(const Vector3& r) const { return {x + r.x, y + r.y, z + r.z}; }
        Vector3 operator-(const Vector3& r) const { return {x - r.x, y - r.y, z - r.z}; }
        Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
        Vector3 operator/(float s) const { float inv = 1.0f / s; return {x * inv, y * inv, z * inv}; }
        Vector3 operator-() const { return {-x, -y, -z}; }

        Vector3& operator+=(const Vector3& r) { x += r.x; y += r.y; z += r.z; return *this; }
        Vector3& operator-=(const Vector3& r) { x -= r.x; y -= r.y; z -= r.z; return *this; }
        Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
        Vector3& operator/=(float s) { float inv = 1.0f / s; x *= inv; y *= inv; z *= inv; return *this; }

        float Dot(const Vector3& r) const { return x * r.x + y * r.y + z * r.z; }
        Vector3 Cross(const Vector3& r) const {
            return {
                y * r.z - z * r.y,
                z * r.x - x * r.z,
                x * r.y - y * r.x
            };
        }

        float LengthSquared() const { return Dot(*this); }
        float Length() const { return std::sqrt(LengthSquared()); }

        Vector3 Normalized() const {
            float lenSq = LengthSquared();
            if (lenSq > EPSILON * EPSILON) {
                float invLen = 1.0f / std::sqrt(lenSq);
                return *this * invLen;
            }
            return {0.0f, 0.0f, 0.0f};
        }

        static const Vector3 Zero;
        static const Vector3 UnitX;
        static const Vector3 UnitY;
        static const Vector3 UnitZ;
    };

    inline Vector3 operator*(float s, const Vector3& v) { return v * s; }

    struct Quaternion {
        float x{0.0f}, y{0.0f}, z{0.0f}, w{1.0f};

        constexpr Quaternion() = default;
        constexpr Quaternion(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

        Quaternion operator*(const Quaternion& r) const {
            return {
                w * r.x + x * r.w + y * r.z - z * r.y,
                w * r.y - x * r.z + y * r.w + z * r.x,
                w * r.z + x * r.y - y * r.x + z * r.w,
                w * r.w - x * r.x - y * r.y - z * r.z
            };
        }

        Vector3 Rotate(const Vector3& v) const {
            Vector3 qv(x, y, z);
            Vector3 t = 2.0f * qv.Cross(v);
            return v + w * t + qv.Cross(t);
        }

        Quaternion Normalized() const {
            float lenSq = x * x + y * y + z * z + w * w;
            if (lenSq > EPSILON * EPSILON) {
                float inv = 1.0f / std::sqrt(lenSq);
                return {x * inv, y * inv, z * inv, w * inv};
            }
            return {0.0f, 0.0f, 0.0f, 1.0f};
        }

        Quaternion Conjugate() const {
            return {-x, -y, -z, w};
        }

        Quaternion Inversed() const {
            float lenSq = x * x + y * y + z * z + w * w;
            if (lenSq > EPSILON * EPSILON) {
                float inv = 1.0f / lenSq;
                return {-x * inv, -y * inv, -z * inv, w * inv};
            }
            return {0.0f, 0.0f, 0.0f, 1.0f};
        }

        static Quaternion FromAxisAngle(const Vector3& axis, float angleRadians) {
            float half = angleRadians * 0.5f;
            float s = std::sin(half);
            Vector3 na = axis.Normalized();
            return {na.x * s, na.y * s, na.z * s, std::cos(half)};
        }

        float Dot(const Quaternion& r) const {
            return x * r.x + y * r.y + z * r.z + w * r.w;
        }

        static Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t) {
            t = std::clamp(t, 0.0f, 1.0f);
            float cosTheta = q0.Dot(q1);
            Quaternion target = q1;

            if (cosTheta < 0.0f) {
                target = Quaternion(-q1.x, -q1.y, -q1.z, -q1.w);
                cosTheta = -cosTheta;
            }

            if (cosTheta > 0.9995f) {
                // Линейная интерполяция при очень близких углах (избегаем деления на ноль)
                Quaternion res(
                    q0.x + t * (target.x - q0.x),
                    q0.y + t * (target.y - q0.y),
                    q0.z + t * (target.z - q0.z),
                    q0.w + t * (target.w - q0.w)
                );
                return res.Normalized();
            }

            float theta = std::acos(cosTheta);
            float sinTheta = std::sin(theta);
            float w0 = std::sin((1.0f - t) * theta) / sinTheta;
            float w1 = std::sin(t * theta) / sinTheta;

            return Quaternion(
                w0 * q0.x + w1 * target.x,
                w0 * q0.y + w1 * target.y,
                w0 * q0.z + w1 * target.z,
                w0 * q0.w + w1 * target.w
            ).Normalized();
        }
    };

    struct Matrix3x3 {
        float m[3][3]{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f}
        };

        Matrix3x3() = default;

        Vector3 operator*(const Vector3& v) const {
            return {
                m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
                m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
            };
        }

        Matrix3x3 operator+(const Matrix3x3& r) const {
            Matrix3x3 out;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    out.m[i][j] = m[i][j] + r.m[i][j];
                }
            }
            return out;
        }

        Matrix3x3 operator-(const Matrix3x3& r) const {
            Matrix3x3 out;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    out.m[i][j] = m[i][j] - r.m[i][j];
                }
            }
            return out;
        }

        Matrix3x3 operator*(const Matrix3x3& r) const {
            Matrix3x3 out;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    out.m[i][j] = m[i][0] * r.m[0][j] + m[i][1] * r.m[1][j] + m[i][2] * r.m[2][j];
                }
            }
            return out;
        }

        Matrix3x3 Transposed() const {
            Matrix3x3 out;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    out.m[i][j] = m[j][i];
                }
            }
            return out;
        }

        Matrix3x3 Inversed() const {
            float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                      - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                      + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

            if (std::abs(det) < EPSILON * EPSILON) {
                return Matrix3x3();
            }

            float invDet = 1.0f / det;
            Matrix3x3 res;
            res.m[0][0] =  (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
            res.m[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * invDet;
            res.m[0][2] =  (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;

            res.m[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * invDet;
            res.m[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
            res.m[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * invDet;

            res.m[2][0] =  (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
            res.m[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * invDet;
            res.m[2][2] =  (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;
            return res;
        }

        static Matrix3x3 SkewSymmetric(const Vector3& v) {
            Matrix3x3 res;
            res.m[0][0] =  0.0f; res.m[0][1] = -v.z;  res.m[0][2] =  v.y;
            res.m[1][0] =  v.z;  res.m[1][1] =  0.0f; res.m[1][2] = -v.x;
            res.m[2][0] = -v.y;  res.m[2][1] =  v.x;  res.m[2][2] =  0.0f;
            return res;
        }

        static Matrix3x3 FromQuaternion(const Quaternion& q) {
            Matrix3x3 res;
            float xx = q.x * q.x;
            float yy = q.y * q.y;
            float zz = q.z * q.z;
            float xy = q.x * q.y;
            float xz = q.x * q.z;
            float yz = q.y * q.z;
            float wx = q.w * q.x;
            float wy = q.w * q.y;
            float wz = q.w * q.z;

            res.m[0][0] = 1.0f - 2.0f * (yy + zz);
            res.m[0][1] = 2.0f * (xy - wz);
            res.m[0][2] = 2.0f * (xz + wy);

            res.m[1][0] = 2.0f * (xy + wz);
            res.m[1][1] = 1.0f - 2.0f * (xx + zz);
            res.m[1][2] = 2.0f * (yz - wx);

            res.m[2][0] = 2.0f * (xz - wy);
            res.m[2][1] = 2.0f * (yz + wx);
            res.m[2][2] = 1.0f - 2.0f * (xx + yy);

            return res;
        }
    };

    struct Transform {
        Vector3 position{0.0f, 0.0f, 0.0f};
        Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};

        Transform() = default;
        Transform(const Vector3& pos, const Quaternion& rot = Quaternion(0.0f, 0.0f, 0.0f, 1.0f)) : position(pos), rotation(rot) {}

        Vector3 TransformPoint(const Vector3& point) const {
            return position + rotation.Rotate(point);
        }

        Vector3 TransformVector(const Vector3& vec) const {
            return rotation.Rotate(vec);
        }
    };

} // namespace SunvoltumPhysics
