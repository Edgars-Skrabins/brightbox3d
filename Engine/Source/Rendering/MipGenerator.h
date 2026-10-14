#pragma once

#include "Rendering/TextureAsset.h"

#include <cstdint>
#include <vector>

namespace BrightBox3D::Rendering::MipGenerator
{
    std::vector<TextureMip> BuildMips8(const uint8_t* pixels, uint32_t width, uint32_t height, bool srgb, bool normalMap, bool generate);
    std::vector<TextureMip> BuildMipsFloat(const float* pixels, uint32_t width, uint32_t height, bool generate);
}
