#pragma once

#include "Math/Vector3.h"

namespace BrightBox3D::Math
{
    struct Vector4
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;

        constexpr Vector4() = default;
        constexpr Vector4(float xValue, float yValue, float zValue, float wValue) : x(xValue), y(yValue), z(zValue), w(wValue) {}
        constexpr Vector4(const Vector3& vector, float wValue) : x(vector.x), y(vector.y), z(vector.z), w(wValue) {}
        explicit constexpr Vector4(float value) : x(value), y(value), z(value), w(value) {}

        constexpr Vector4 operator+(const Vector4& other) const { return { x + other.x, y + other.y, z + other.z, w + other.w }; }
        constexpr Vector4 operator-(const Vector4& other) const { return { x - other.x, y - other.y, z - other.z, w - other.w }; }
        constexpr Vector4 operator*(const Vector4& other) const { return { x * other.x, y * other.y, z * other.z, w * other.w }; }
        constexpr Vector4 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar, w * scalar }; }
        constexpr Vector4 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar, w / scalar }; }
        constexpr bool operator==(const Vector4& other) const { return x == other.x && y == other.y && z == other.z && w == other.w; }
        constexpr bool operator!=(const Vector4& other) const { return !(*this == other); }
        float& operator[](int index) { return (&x)[index]; }
        const float& operator[](int index) const { return (&x)[index]; }

        constexpr Vector3 XYZ() const { return { x, y, z }; }
        static constexpr float Dot(const Vector4& a, const Vector4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
        static Vector4 Lerp(const Vector4& a, const Vector4& b, float t) { return a + (b - a) * t; }
    };
}
