#include "UI/FontData.h"

#include "Core/EngineText.h"
#include "Serialization/BinaryStream.h"

#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <algorithm>
#include <cstring>
#include <thread>
#include <unordered_map>

namespace BrightBox3D::UI
{
    namespace
    {
        constexpr uint32_t FontFormatVersion = 1;
        constexpr uint32_t AsciiFirst = 0x20;
        constexpr uint32_t AsciiEnd = 0x7F;
        constexpr uint32_t LatinFirst = 0xA0;
        constexpr uint32_t LatinLast = 0x17F;
        constexpr uint32_t TypographicPunctuation[] = { 0x2018u, 0x2019u, 0x201Cu, 0x201Du, 0x2022u, 0x2026u, 0x20ACu, 0x2122u };
        constexpr double EdgeColoringAngle = 3.0;
        constexpr double MinimumGlyphScale = 40.0;
        constexpr double PixelRange = 6.0;
        constexpr double MiterLimit = 1.0;
        constexpr int GlyphSpacing = 1;
        constexpr size_t AtlasChannels = 4;

        struct FontSource
        {
            FontSource() : freetype(msdfgen::initializeFreetype()) {}

            ~FontSource()
            {
                if (font)
                {
                    msdfgen::destroyFont(font);
                }
                if (freetype)
                {
                    msdfgen::deinitializeFreetype(freetype);
                }
            }

            msdfgen::FreetypeHandle* freetype = nullptr;
            msdfgen::FontHandle* font = nullptr;
        };

        msdf_atlas::Charset BuildCharset()
        {
            msdf_atlas::Charset charset;
            for (uint32_t codepoint = AsciiFirst; codepoint < AsciiEnd; ++codepoint)
            {
                charset.add(codepoint);
            }
            for (uint32_t codepoint = LatinFirst; codepoint <= LatinLast; ++codepoint)
            {
                charset.add(codepoint);
            }
            for (uint32_t codepoint : TypographicPunctuation)
            {
                charset.add(codepoint);
            }
            return charset;
        }

        bool PackGlyphs(std::vector<msdf_atlas::GlyphGeometry>& glyphs, int& width, int& height)
        {
            for (msdf_atlas::GlyphGeometry& glyph : glyphs)
            {
                glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, EdgeColoringAngle, 0);
            }
            msdf_atlas::TightAtlasPacker packer;
            packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE);
            packer.setMinimumScale(MinimumGlyphScale);
            packer.setPixelRange(PixelRange);
            packer.setMiterLimit(MiterLimit);
            packer.setSpacing(GlyphSpacing);
            if (packer.pack(glyphs.data(), static_cast<int>(glyphs.size())) != 0)
            {
                return false;
            }
            packer.getDimensions(width, height);
            return true;
        }

        void CopyAtlas(std::vector<msdf_atlas::GlyphGeometry>& glyphs, int width, int height, FontData& data)
        {
            msdf_atlas::ImmediateAtlasGenerator<float, AtlasChannels, msdf_atlas::mtsdfGenerator, msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, AtlasChannels>> generator(width, height);
            msdf_atlas::GeneratorAttributes attributes;
            generator.setAttributes(attributes);
            generator.setThreadCount(std::max(1u, std::thread::hardware_concurrency()));
            generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));
            msdfgen::BitmapConstRef<msdf_atlas::byte, AtlasChannels> bitmap = generator.atlasStorage();
            data.atlasWidth = static_cast<uint32_t>(width);
            data.atlasHeight = static_cast<uint32_t>(height);
            data.pixelRange = static_cast<float>(PixelRange);
            size_t rowBytes = static_cast<size_t>(width) * AtlasChannels;
            data.pixels.resize(rowBytes * height);
            for (int y = 0; y < height; ++y)
            {
                std::memcpy(&data.pixels[static_cast<size_t>(y) * rowBytes], bitmap(0, height - 1 - y), rowBytes);
            }
        }

        void ConvertGlyphs(const std::vector<msdf_atlas::GlyphGeometry>& glyphs, double scale, int width, int height, FontData& data)
        {
            data.glyphs.clear();
            for (const msdf_atlas::GlyphGeometry& glyph : glyphs)
            {
                FontGlyph converted;
                converted.codepoint = glyph.getCodepoint();
                converted.advance = static_cast<float>(glyph.getAdvance() * scale);
                double l = 0, b = 0, r = 0, t = 0;
                glyph.getQuadPlaneBounds(l, b, r, t);
                converted.planeLeft = static_cast<float>(l * scale);
                converted.planeBottom = static_cast<float>(b * scale);
                converted.planeRight = static_cast<float>(r * scale);
                converted.planeTop = static_cast<float>(t * scale);
                glyph.getQuadAtlasBounds(l, b, r, t);
                converted.atlasLeft = static_cast<float>(l / width);
                converted.atlasRight = static_cast<float>(r / width);
                converted.atlasBottom = static_cast<float>(1.0 - b / height);
                converted.atlasTop = static_cast<float>(1.0 - t / height);
                data.glyphs.push_back(converted);
            }
        }

        void ConvertKerning(const std::vector<msdf_atlas::GlyphGeometry>& glyphs, const msdf_atlas::FontGeometry& geometry, double scale, FontData& data)
        {
            data.kerning.clear();
            std::unordered_map<int, uint32_t> codepoints;
            for (const msdf_atlas::GlyphGeometry& glyph : glyphs)
            {
                codepoints[glyph.getIndex()] = glyph.getCodepoint();
            }
            for (const auto& [pair, value] : geometry.getKerning())
            {
                auto first = codepoints.find(pair.first);
                auto second = codepoints.find(pair.second);
                if (first != codepoints.end() && second != codepoints.end())
                {
                    data.kerning[(static_cast<uint64_t>(first->second) << 32) | second->second] = static_cast<float>(value * scale);
                }
            }
        }
    }

    std::vector<uint8_t> FontData::Serialize() const
    {
        Serialization::BinaryWriter writer;
        writer.Write(FontFormatVersion);
        writer.Write(atlasWidth);
        writer.Write(atlasHeight);
        writer.Write(pixelRange);
        writer.Write(ascender);
        writer.Write(descender);
        writer.Write(lineHeight);
        writer.WriteVector(pixels);
        writer.WriteVector(glyphs);
        writer.WriteVarUInt(kerning.size());
        for (const auto& [key, value] : kerning)
        {
            writer.Write(key);
            writer.Write(value);
        }
        return std::move(writer.GetBuffer());
    }

    bool FontData::Deserialize(const uint8_t* data, size_t size)
    {
        Serialization::BinaryReader reader(data, size);
        uint32_t version = 0;
        if (!reader.Read(version) || version != FontFormatVersion)
        {
            return false;
        }
        reader.Read(atlasWidth);
        reader.Read(atlasHeight);
        reader.Read(pixelRange);
        reader.Read(ascender);
        reader.Read(descender);
        reader.Read(lineHeight);
        reader.ReadVector(pixels);
        reader.ReadVector(glyphs);
        uint64_t count = 0;
        reader.ReadVarUInt(count);
        kerning.clear();
        for (uint64_t i = 0; i < count && !reader.Failed(); ++i)
        {
            uint64_t key = 0;
            float value = 0.0f;
            reader.Read(key);
            reader.Read(value);
            kerning[key] = value;
        }
        BuildIndex();
        return !reader.Failed();
    }

    void FontData::BuildIndex()
    {
        m_index.clear();
        for (size_t i = 0; i < glyphs.size(); ++i)
        {
            m_index[glyphs[i].codepoint] = i;
        }
    }

    const FontGlyph* FontData::FindGlyph(uint32_t codepoint) const
    {
        auto it = m_index.find(codepoint);
        return it == m_index.end() ? nullptr : &glyphs[it->second];
    }

    float FontData::GetKerning(uint32_t first, uint32_t second) const
    {
        if (kerning.empty())
        {
            return 0.0f;
        }
        auto it = kerning.find((static_cast<uint64_t>(first) << 32) | second);
        return it == kerning.end() ? 0.0f : it->second;
    }

    Core::Result<FontData> GenerateFontData(const std::filesystem::path& file)
    {
        FontSource source;
        if (!source.freetype)
        {
            return Core::Error(Core::EngineText::FreeTypeInitFailed);
        }
        source.font = msdfgen::loadFont(source.freetype, file.string().c_str());
        if (!source.font)
        {
            return Core::Error(Core::EngineText::FontLoadFailed, file.generic_string());
        }
        std::vector<msdf_atlas::GlyphGeometry> glyphs;
        msdf_atlas::FontGeometry geometry(&glyphs);
        geometry.loadCharset(source.font, 1.0, BuildCharset());
        int width = 0;
        int height = 0;
        if (!PackGlyphs(glyphs, width, height))
        {
            return Core::Error(Core::EngineText::GlyphPackingFailed);
        }
        FontData data;
        CopyAtlas(glyphs, width, height, data);
        const msdfgen::FontMetrics& metrics = geometry.getMetrics();
        double scale = metrics.emSize > 0.0 ? 1.0 / metrics.emSize : 1.0;
        data.ascender = static_cast<float>(metrics.ascenderY * scale);
        data.descender = static_cast<float>(metrics.descenderY * scale);
        data.lineHeight = static_cast<float>(metrics.lineHeight * scale);
        ConvertGlyphs(glyphs, scale, width, height, data);
        ConvertKerning(glyphs, geometry, scale, data);
        data.BuildIndex();
        return data;
    }
}
