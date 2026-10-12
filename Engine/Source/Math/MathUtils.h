#pragma once

#include <algorithm>
#include <cmath>

namespace BrightBox3D::Math
{
    inline constexpr float Pi = 3.14159265358979323846f;
    inline constexpr float TwoPi = Pi * 2.0f;
    inline constexpr float HalfPi = Pi * 0.5f;
    inline constexpr float DegreesToRadians = Pi / 180.0f;
    inline constexpr float RadiansToDegrees = 180.0f / Pi;
    inline constexpr float Epsilon = 1.0e-6f;

    inline float Clamp(float value, float minimum, float maximum)
    {
        return std::min(std::max(value, minimum), maximum);
    }

    inline float Clamp01(float value)
    {
        return Clamp(value, 0.0f, 1.0f);
    }

    inline float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    inline float InverseLerp(float a, float b, float value)
    {
        return std::abs(b - a) < Epsilon ? 0.0f : (value - a) / (b - a);
    }

    inline bool NearlyEqual(float a, float b, float tolerance = 1.0e-4f)
    {
        return std::abs(a - b) <= tolerance;
    }

    inline float Repeat(float value, float length)
    {
        return Clamp(value - std::floor(value / length) * length, 0.0f, length);
    }

    inline float SmoothStep(float edge0, float edge1, float value)
    {
        float t = Clamp01((value - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }
}
