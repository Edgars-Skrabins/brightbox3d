#include "Rendering/MipGenerator.h"

#include "Math/ColorSpace.h"
#include "Math/Vector3.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace BrightBox3D::Rendering::MipGenerator
{
    namespace
    {
        constexpr int ChannelCount = 4;
        constexpr int ColorChannelCount = 3;
        constexpr int AlphaChannel = 3;
        constexpr uint32_t FootprintSize = 2;
        constexpr size_t ByteLevels = 256;
        constexpr float MaxByte = 255.0f;
        constexpr float ByteRounding = 0.5f;
        constexpr float FloatAverage = 0.25f;

        using Sum = std::array<float, ChannelCount>;

        uint8_t ToByte(float unit)
        {
            return static_cast<uint8_t>(std::clamp(unit * MaxByte + ByteRounding, 0.0f, MaxByte));
        }

        std::array<float, ByteLevels> BuildDecodeTable(bool srgb)
        {
            std::array<float, ByteLevels> table{};
            for (size_t i = 0; i < ByteLevels; ++i)
            {
                table[i] = srgb ? Math::SrgbToLinear(i / MaxByte) : i / MaxByte;
            }
            return table;
        }

        struct Rgba8Format
        {
            using Channel = uint8_t;

            bool srgb = false;
            bool normalMap = false;
            std::array<float, ByteLevels> decode{};

            void Accumulate(const uint8_t* texel, Sum& sum) const
            {
                for (int c = 0; c < ColorChannelCount; ++c)
                {
                    sum[c] += normalMap ? texel[c] / MaxByte * 2.0f - 1.0f : decode[texel[c]];
                }
                sum[AlphaChannel] += texel[AlphaChannel] / MaxByte;
            }

            void Store(const Sum& sum, int samples, uint8_t* out) const
            {
                if (normalMap)
                {
                    Math::Vector3 normal = Math::Vector3(sum[0], sum[1], sum[2]).Normalized();
                    out[0] = ToByte(normal.x * 0.5f + 0.5f);
                    out[1] = ToByte(normal.y * 0.5f + 0.5f);
                    out[2] = ToByte(normal.z * 0.5f + 0.5f);
                }
                else
                {
                    for (int c = 0; c < ColorChannelCount; ++c)
                    {
                        float value = sum[c] / static_cast<float>(samples);
                        out[c] = ToByte(srgb ? Math::LinearToSrgb(value) : value);
                    }
                }
                out[AlphaChannel] = ToByte(sum[AlphaChannel] / static_cast<float>(samples));
            }
        };

        struct FloatFormat
        {
            using Channel = float;

            void Accumulate(const float* texel, Sum& sum) const
            {
                for (int c = 0; c < ChannelCount; ++c)
                {
                    sum[c] += texel[c];
                }
            }

            void Store(const Sum& sum, int, float* out) const
            {
                for (int c = 0; c < ChannelCount; ++c)
                {
                    out[c] = sum[c] * FloatAverage;
                }
            }
        };

        template <typename Format>
        TextureMip Downsample(const TextureMip& previous, const Format& format)
        {
            using Channel = typename Format::Channel;
            constexpr size_t PixelSize = sizeof(Channel) * ChannelCount;
            TextureMip next;
            next.width = std::max<uint32_t>(previous.width / 2, 1);
            next.height = std::max<uint32_t>(previous.height / 2, 1);
            next.pixels.resize(static_cast<size_t>(next.width) * next.height * PixelSize);
            const auto* source = reinterpret_cast<const Channel*>(previous.pixels.data());
            auto* target = reinterpret_cast<Channel*>(next.pixels.data());
            for (uint32_t y = 0; y < next.height; ++y)
            {
                for (uint32_t x = 0; x < next.width; ++x)
                {
                    Sum sum{};
                    int samples = 0;
                    for (uint32_t dy = 0; dy < FootprintSize; ++dy)
                    {
                        for (uint32_t dx = 0; dx < FootprintSize; ++dx)
                        {
                            uint32_t sx = std::min(x * FootprintSize + dx, previous.width - 1);
                            uint32_t sy = std::min(y * FootprintSize + dy, previous.height - 1);
                            format.Accumulate(&source[(static_cast<size_t>(sy) * previous.width + sx) * ChannelCount], sum);
                            ++samples;
                        }
                    }
                    format.Store(sum, samples, &target[(static_cast<size_t>(y) * next.width + x) * ChannelCount]);
                }
            }
            return next;
        }

        template <typename Format>
        std::vector<TextureMip> BuildChain(const typename Format::Channel* pixels, uint32_t width, uint32_t height, bool generate, const Format& format)
        {
            constexpr size_t PixelSize = sizeof(typename Format::Channel) * ChannelCount;
            std::vector<TextureMip> mips;
            TextureMip base;
            base.width = width;
            base.height = height;
            base.pixels.resize(static_cast<size_t>(width) * height * PixelSize);
            std::memcpy(base.pixels.data(), pixels, base.pixels.size());
            mips.push_back(std::move(base));
            while (generate && (mips.back().width > 1 || mips.back().height > 1))
            {
                mips.push_back(Downsample(mips.back(), format));
            }
            return mips;
        }
    }

    std::vector<TextureMip> BuildMips8(const uint8_t* pixels, uint32_t width, uint32_t height, MipEncoding encoding, MipChain chain)
    {
        bool generate = chain == MipChain::Full;
        Rgba8Format format;
        format.srgb = encoding == MipEncoding::Srgb;
        format.normalMap = encoding == MipEncoding::NormalMap;
        if (generate)
        {
            format.decode = BuildDecodeTable(format.srgb);
        }
        return BuildChain(pixels, width, height, generate, format);
    }

    std::vector<TextureMip> BuildMipsFloat(const float* pixels, uint32_t width, uint32_t height, MipChain chain)
    {
        return BuildChain(pixels, width, height, chain == MipChain::Full, FloatFormat{});
    }
}
