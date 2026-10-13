#include "Rendering/TextureCapture.h"

#include <stb_image_write.h>

#include <utility>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr int ChannelCount = 4;
        constexpr uint8_t OpaqueAlpha = 255;
    }

    bool ReadbackRGBA(RHI::Device& device, RHI::Texture& texture, std::vector<uint8_t>& pixels)
    {
        if (!device.ReadTexture(&texture, pixels))
        {
            return false;
        }
        bool bgra = texture.GetFormat() == RHI::Format::BGRA8Unorm;
        for (size_t i = 0; i + ChannelCount - 1 < pixels.size(); i += ChannelCount)
        {
            if (bgra)
            {
                std::swap(pixels[i], pixels[i + 2]);
            }
            pixels[i + 3] = OpaqueAlpha;
        }
        return true;
    }

    bool SavePng(RHI::Device& device, RHI::Texture& texture, const std::filesystem::path& path)
    {
        std::vector<uint8_t> pixels;
        if (!ReadbackRGBA(device, texture, pixels))
        {
            return false;
        }
        int width = static_cast<int>(texture.GetWidth());
        int height = static_cast<int>(texture.GetHeight());
        return stbi_write_png(path.string().c_str(), width, height, ChannelCount, pixels.data(), width * ChannelCount) != 0;
    }
}
