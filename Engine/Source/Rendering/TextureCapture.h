#pragma once

#include "RHI/Device.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace BrightBox3D::Rendering
{
    bool ReadbackRGBA(RHI::Device& device, RHI::Texture& texture, std::vector<uint8_t>& pixels);
    bool SavePng(RHI::Device& device, RHI::Texture& texture, const std::filesystem::path& path);
}
