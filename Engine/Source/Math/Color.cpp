#include "Math/Color.h"

#include "Math/ColorSpace.h"

namespace BrightBox3D::Math
{
    namespace
    {
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
