#include "Rendering/TextureAsset.h"

#include "Serialization/BinaryStream.h"

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t TextureFormatVersion = 1;
    }

    std::vector<uint8_t> TextureAsset::Serialize() const
    {
        Serialization::BinaryWriter writer;
        writer.Write(TextureFormatVersion);
        writer.Write(static_cast<uint32_t>(m_format));
        writer.Write(static_cast<uint32_t>(m_filter));
        writer.Write(static_cast<uint32_t>(m_wrap));
        writer.Write(static_cast<uint32_t>(m_isNormalMap ? 1 : 0));
        writer.WriteVarUInt(m_mips.size());
        for (const TextureMip& mip : m_mips)
        {
            writer.Write(mip.width);
            writer.Write(mip.height);
            writer.WriteVector(mip.pixels);
        }
        return std::move(writer.GetBuffer());
    }

    bool TextureAsset::Deserialize(const uint8_t* data, size_t size)
    {
        Serialization::BinaryReader reader(data, size);
        uint32_t version = 0;
        uint32_t format = 0;
        uint32_t filter = 0;
        uint32_t wrap = 0;
        uint32_t normal = 0;
        if (!reader.Read(version) || version != TextureFormatVersion)
        {
            return false;
        }
        reader.Read(format);
        reader.Read(filter);
        reader.Read(wrap);
        reader.Read(normal);
        m_format = static_cast<TexturePixelFormat>(format);
        m_filter = static_cast<TextureFilterMode>(filter);
        m_wrap = static_cast<TextureWrapMode>(wrap);
        m_isNormalMap = normal != 0;
        uint64_t count = 0;
        reader.ReadVarUInt(count);
        m_mips.resize(static_cast<size_t>(count));
        for (TextureMip& mip : m_mips)
        {
            reader.Read(mip.width);
            reader.Read(mip.height);
            reader.ReadVector(mip.pixels);
        }
        return !reader.Failed();
    }
}
