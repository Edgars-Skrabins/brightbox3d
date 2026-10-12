#pragma once

#include "Math/MathUtils.h"

namespace BrightBox3D::Math
{
    struct Vector2
    {
        float x = 0.0f;
        float y = 0.0f;

        constexpr Vector2() = default;
        constexpr Vector2(float xValue, float yValue) : x(xValue), y(yValue) {}
        explicit constexpr Vector2(float value) : x(value), y(value) {}

        constexpr Vector2 operator+(const Vector2& other) const { return { x + other.x, y + other.y }; }
        constexpr Vector2 operator-(const Vector2& other) const { return { x - other.x, y - other.y }; }
        constexpr Vector2 operator*(const Vector2& other) const { return { x * other.x, y * other.y }; }
        constexpr Vector2 operator/(const Vector2& other) const { return { x / other.x, y / other.y }; }
        constexpr Vector2 operator*(float scalar) const { return { x * scalar, y * scalar }; }
        constexpr Vector2 operator/(float scalar) const { return { x / scalar, y / scalar }; }
        constexpr Vector2 operator-() const { return { -x, -y }; }
        Vector2& operator+=(const Vector2& other) { x += other.x; y += other.y; return *this; }
        Vector2& operator-=(const Vector2& other) { x -= other.x; y -= other.y; return *this; }
        Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
        constexpr bool operator==(const Vector2& other) const { return x == other.x && y == other.y; }
        constexpr bool operator!=(const Vector2& other) const { return !(*this == other); }

        float Length() const { return std::sqrt(x * x + y * y); }
        constexpr float LengthSquared() const { return x * x + y * y; }

        Vector2 Normalized() const
        {
            float length = Length();
            return length > Epsilon ? *this / length : Vector2();
        }

        static constexpr float Dot(const Vector2& a, const Vector2& b) { return a.x * b.x + a.y * b.y; }
        static Vector2 Lerp(const Vector2& a, const Vector2& b, float t) { return a + (b - a) * t; }
        static Vector2 Min(const Vector2& a, const Vector2& b) { return { std::min(a.x, b.x), std::min(a.y, b.y) }; }
        static Vector2 Max(const Vector2& a, const Vector2& b) { return { std::max(a.x, b.x), std::max(a.y, b.y) }; }
    };

    inline constexpr Vector2 operator*(float scalar, const Vector2& vector)
    {
        return vector * scalar;
    }
}
