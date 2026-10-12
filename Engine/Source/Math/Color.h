#pragma once

#include "Math/Vector4.h"

namespace BrightBox3D::Math
{
    struct Color
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;

        constexpr Color() = default;
        constexpr Color(float red, float green, float blue, float alpha = 1.0f) : r(red), g(green), b(blue), a(alpha) {}

        constexpr Color operator*(float scalar) const { return { r * scalar, g * scalar, b * scalar, a * scalar }; }
        constexpr Color operator*(const Color& other) const { return { r * other.r, g * other.g, b * other.b, a * other.a }; }
        constexpr Color operator+(const Color& other) const { return { r + other.r, g + other.g, b + other.b, a + other.a }; }
        constexpr bool operator==(const Color& other) const { return r == other.r && g == other.g && b == other.b && a == other.a; }
        constexpr bool operator!=(const Color& other) const { return !(*this == other); }

        constexpr Vector4 ToVector4() const { return { r, g, b, a }; }
        Color ToLinear() const;
        Color ToSrgb() const;
        unsigned int ToRgba8() const;
        static Color FromRgba8(unsigned int packed);
        static Color Lerp(const Color& a, const Color& b, float t);

        static constexpr Color White() { return { 1.0f, 1.0f, 1.0f, 1.0f }; }
        static constexpr Color Black() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
        static constexpr Color Clear() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }
        static constexpr Color Red() { return { 1.0f, 0.0f, 0.0f, 1.0f }; }
        static constexpr Color Green() { return { 0.0f, 1.0f, 0.0f, 1.0f }; }
        static constexpr Color Blue() { return { 0.0f, 0.0f, 1.0f, 1.0f }; }
    };
}
