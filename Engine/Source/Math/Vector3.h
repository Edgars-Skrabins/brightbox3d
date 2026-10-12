#pragma once

#include "Math/MathUtils.h"

namespace BrightBox3D::Math
{
    struct Vector3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        constexpr Vector3() = default;
        constexpr Vector3(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue) {}
        explicit constexpr Vector3(float value) : x(value), y(value), z(value) {}

        constexpr Vector3 operator+(const Vector3& other) const { return { x + other.x, y + other.y, z + other.z }; }
        constexpr Vector3 operator-(const Vector3& other) const { return { x - other.x, y - other.y, z - other.z }; }
        constexpr Vector3 operator*(const Vector3& other) const { return { x * other.x, y * other.y, z * other.z }; }
        constexpr Vector3 operator/(const Vector3& other) const { return { x / other.x, y / other.y, z / other.z }; }
        constexpr Vector3 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
        constexpr Vector3 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar }; }
        constexpr Vector3 operator-() const { return { -x, -y, -z }; }
        Vector3& operator+=(const Vector3& other) { x += other.x; y += other.y; z += other.z; return *this; }
        Vector3& operator-=(const Vector3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
        Vector3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
        Vector3& operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }
        constexpr bool operator==(const Vector3& other) const { return x == other.x && y == other.y && z == other.z; }
        constexpr bool operator!=(const Vector3& other) const { return !(*this == other); }
        float& operator[](int index) { return (&x)[index]; }
        const float& operator[](int index) const { return (&x)[index]; }

        float Length() const { return std::sqrt(x * x + y * y + z * z); }
        constexpr float LengthSquared() const { return x * x + y * y + z * z; }

        Vector3 Normalized() const
        {
            float length = Length();
            return length > Epsilon ? *this / length : Vector3();
        }

        static constexpr float Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

        static constexpr Vector3 Cross(const Vector3& a, const Vector3& b)
        {
            return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
        }

        static float Distance(const Vector3& a, const Vector3& b) { return (a - b).Length(); }
        static Vector3 Lerp(const Vector3& a, const Vector3& b, float t) { return a + (b - a) * t; }
        static Vector3 Min(const Vector3& a, const Vector3& b) { return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) }; }
        static Vector3 Max(const Vector3& a, const Vector3& b) { return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) }; }

        static constexpr Vector3 Zero() { return { 0.0f, 0.0f, 0.0f }; }
        static constexpr Vector3 One() { return { 1.0f, 1.0f, 1.0f }; }
        static constexpr Vector3 Up() { return { 0.0f, 1.0f, 0.0f }; }
        static constexpr Vector3 Down() { return { 0.0f, -1.0f, 0.0f }; }
        static constexpr Vector3 Right() { return { 1.0f, 0.0f, 0.0f }; }
        static constexpr Vector3 Left() { return { -1.0f, 0.0f, 0.0f }; }
        static constexpr Vector3 Forward() { return { 0.0f, 0.0f, 1.0f }; }
        static constexpr Vector3 Back() { return { 0.0f, 0.0f, -1.0f }; }
    };

    inline constexpr Vector3 operator*(float scalar, const Vector3& vector)
    {
        return vector * scalar;
    }
}
