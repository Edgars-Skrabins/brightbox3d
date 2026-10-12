#include "Math/Color.h"

namespace BrightBox3D::Math
{
    namespace
    {
        float SrgbToLinear(float value)
        {
            return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        float LinearToSrgb(float value)
        {
            return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        }

        unsigned int ToByte(float value)
        {
            return static_cast<unsigned int>(Clamp01(value) * 255.0f + 0.5f);
        }
    }

    Color Color::ToLinear() const
    {
        return { SrgbToLinear(r), SrgbToLinear(g), SrgbToLinear(b), a };
    }

    Color Color::ToSrgb() const
    {
        return { LinearToSrgb(r), LinearToSrgb(g), LinearToSrgb(b), a };
    }

    unsigned int Color::ToRgba8() const
    {
        return ToByte(r) | (ToByte(g) << 8) | (ToByte(b) << 16) | (ToByte(a) << 24);
    }

    Color Color::FromRgba8(unsigned int packed)
    {
        return { (packed & 0xFF) / 255.0f, ((packed >> 8) & 0xFF) / 255.0f, ((packed >> 16) & 0xFF) / 255.0f, ((packed >> 24) & 0xFF) / 255.0f };
    }

    Color Color::Lerp(const Color& a, const Color& b, float t)
    {
        return { Math::Lerp(a.r, b.r, t), Math::Lerp(a.g, b.g, t), Math::Lerp(a.b, b.b, t), Math::Lerp(a.a, b.a, t) };
    }
}
