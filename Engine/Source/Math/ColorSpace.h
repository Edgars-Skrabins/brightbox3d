#pragma once

#include <cmath>

namespace BrightBox3D::Math
{
    inline constexpr float SrgbLinearThreshold = 0.04045f;
    inline constexpr float LinearSrgbThreshold = 0.0031308f;
    inline constexpr float SrgbLinearSlope = 12.92f;
    inline constexpr float SrgbOffset = 0.055f;
    inline constexpr float SrgbScale = 1.055f;
    inline constexpr float SrgbGamma = 2.4f;

    inline float SrgbToLinear(float value)
    {
        return value <= SrgbLinearThreshold ? value / SrgbLinearSlope : std::pow((value + SrgbOffset) / SrgbScale, SrgbGamma);
    }

    inline float LinearToSrgb(float value)
    {
        return value <= LinearSrgbThreshold ? value * SrgbLinearSlope : SrgbScale * std::pow(value, 1.0f / SrgbGamma) - SrgbOffset;
    }
}
