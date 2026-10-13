#include "UI/FontData.h"

#include "Serialization/BinaryStream.h"

#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <cstring>
#include <thread>

namespace BrightBox3D::UI
{
    namespace
    {
        constexpr uint32_t FontFormatVersion = 1;
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

    bool GenerateFontData(const std::filesystem::path& file, FontData& data, std::string& error)
    {
        msdfgen::FreetypeHandle* freetype = msdfgen::initializeFreetype();
        if (!freetype)
        {
            error = "FreeType initialization failed";
            return false;
        }
        msdfgen::FontHandle* font = msdfgen::loadFont(freetype, file.string().c_str());
        if (!font)
        {
            msdfgen::deinitializeFreetype(freetype);
            error = "cannot load font '" + file.generic_string() + "'";
            return false;
        }

        msdf_atlas::Charset charset;
        for (uint32_t codepoint = 0x20; codepoint < 0x7F; ++codepoint)
        {
            charset.add(codepoint);
        }
        for (uint32_t codepoint = 0xA0; codepoint <= 0x17F; ++codepoint)
        {
            charset.add(codepoint);
        }
        for (uint32_t codepoint : { 0x2018u, 0x2019u, 0x201Cu, 0x201Du, 0x2022u, 0x2026u, 0x20ACu, 0x2122u })
        {
            charset.add(codepoint);
        }

        std::vector<msdf_atlas::GlyphGeometry> glyphs;
        msdf_atlas::FontGeometry geometry(&glyphs);
        geometry.loadCharset(font, 1.0, charset);
        for (msdf_atlas::GlyphGeometry& glyph : glyphs)
        {
            glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, 3.0, 0);
        }

        msdf_atlas::TightAtlasPacker packer;
        packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE);
        packer.setMinimumScale(40.0);
        packer.setPixelRange(6.0);
        packer.setMiterLimit(1.0);
        packer.setSpacing(1);
        if (packer.pack(glyphs.data(), static_cast<int>(glyphs.size())) != 0)
        {
            msdfgen::destroyFont(font);
            msdfgen::deinitializeFreetype(freetype);
            error = "glyph packing failed";
            return false;
        }
        int width = 0;
        int height = 0;
        packer.getDimensions(width, height);

        msdf_atlas::ImmediateAtlasGenerator<float, 4, msdf_atlas::mtsdfGenerator, msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>> generator(width, height);
        msdf_atlas::GeneratorAttributes attributes;
        generator.setAttributes(attributes);
        generator.setThreadCount(std::max(1u, std::thread::hardware_concurrency()));
        generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));
        msdfgen::BitmapConstRef<msdf_atlas::byte, 4> bitmap = generator.atlasStorage();

        data.atlasWidth = static_cast<uint32_t>(width);
        data.atlasHeight = static_cast<uint32_t>(height);
        data.pixelRange = 6.0f;
        data.pixels.resize(static_cast<size_t>(width) * height * 4);
        for (int y = 0; y < height; ++y)
        {
            const msdf_atlas::byte* source = bitmap(0, height - 1 - y);
            std::memcpy(&data.pixels[static_cast<size_t>(y) * width * 4], source, static_cast<size_t>(width) * 4);
        }

        const msdfgen::FontMetrics& metrics = geometry.getMetrics();
        double scale = metrics.emSize > 0.0 ? 1.0 / metrics.emSize : 1.0;
        data.ascender = static_cast<float>(metrics.ascenderY * scale);
        data.descender = static_cast<float>(metrics.descenderY * scale);
        data.lineHeight = static_cast<float>(metrics.lineHeight * scale);
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
        data.BuildIndex();
        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(freetype);
        return true;
    }
}
