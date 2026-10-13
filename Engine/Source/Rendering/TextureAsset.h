#pragma once

#include "Assets/Asset.h"

#include <cstdint>
#include <vector>

namespace BrightBox3D::Rendering
{
    BrightEnum()
    enum class TextureFilterMode
    {
        Point,
        Bilinear,
        Trilinear,
        Anisotropic
    };

    BrightEnum()
    enum class TextureWrapMode
    {
        Repeat,
        Clamp,
        Mirror
    };

    enum class TexturePixelFormat : uint32_t
    {
        RGBA8,
        RGBA8Srgb,
        RGBA32Float
    };

    struct TextureMip
    {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> pixels;
    };

    BrightClass()
    class TextureAsset : public Assets::Asset
    {
        BrightBody()
    public:
        static constexpr Assets::AssetType Type = Assets::AssetType::Texture;

        uint32_t GetWidth() const { return m_mips.empty() ? 0 : m_mips[0].width; }
        uint32_t GetHeight() const { return m_mips.empty() ? 0 : m_mips[0].height; }
        TexturePixelFormat GetPixelFormat() const { return m_format; }
        TextureFilterMode GetFilter() const { return m_filter; }
        TextureWrapMode GetWrap() const { return m_wrap; }
        bool IsNormalMap() const { return m_isNormalMap; }
        const std::vector<TextureMip>& GetMips() const { return m_mips; }

        void SetData(TexturePixelFormat format, std::vector<TextureMip> mips, TextureFilterMode filter, TextureWrapMode wrap, bool isNormalMap)
        {
            m_format = format;
            m_mips = std::move(mips);
            m_filter = filter;
            m_wrap = wrap;
            m_isNormalMap = isNormalMap;
        }

        std::vector<uint8_t> Serialize() const;
        bool Deserialize(const uint8_t* data, size_t size);

    private:
        TexturePixelFormat m_format = TexturePixelFormat::RGBA8;
        std::vector<TextureMip> m_mips;
        TextureFilterMode m_filter = TextureFilterMode::Trilinear;
        TextureWrapMode m_wrap = TextureWrapMode::Repeat;
        bool m_isNormalMap = false;
    };
}
