#pragma once

#include "Rendering/TextureAsset.h"

#include <cstdint>
#include <vector>

namespace BrightBox3D::Rendering::MipGenerator
{
    enum class MipEncoding
    {
        Linear,
        Srgb,
        NormalMap
    };

    enum class MipChain
    {
        Full,
        BaseOnly
    };

    std::vector<TextureMip> BuildMips8(const uint8_t* pixels, uint32_t width, uint32_t height, MipEncoding encoding, MipChain chain);
    std::vector<TextureMip> BuildMipsFloat(const float* pixels, uint32_t width, uint32_t height, MipChain chain);
}
