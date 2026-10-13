#include "UI/Font.h"

#include "Assets/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Hash.h"
#include "Core/Log.h"
#include "Core/StringUtils.h"
#include "Core/Utf8.h"
#include "UI/UIAssets.h"

namespace BrightBox3D::UI
{
    RHI::Texture* Font::GetTexture(RHI::Device& device)
    {
        if (!m_texture && m_data.atlasWidth > 0)
        {
            RHI::TextureDesc desc;
            desc.width = m_data.atlasWidth;
            desc.height = m_data.atlasHeight;
            desc.format = RHI::Format::RGBA8Unorm;
            desc.debugName = "FontAtlas";
            m_texture = device.CreateTexture(desc);
            device.UpdateTexture(m_texture.get(), 0, 0, m_data.pixels.data(), m_data.atlasWidth * 4);
        }
        return m_texture.get();
    }

    float Font::MeasureWidth(const std::string& text, float size) const
    {
        float width = 0.0f;
        float lineWidth = 0.0f;
        uint32_t previous = 0;
        for (uint32_t codepoint : Core::Utf8::Decode(text))
        {
            if (codepoint == '\n')
            {
                width = std::max(width, lineWidth);
                lineWidth = 0.0f;
                previous = 0;
                continue;
            }
            const FontGlyph* glyph = m_data.FindGlyph(codepoint);
            if (!glyph)
            {
                glyph = m_data.FindGlyph('?');
            }
            if (glyph)
            {
                lineWidth += (glyph->advance + (previous ? m_data.GetKerning(previous, codepoint) : 0.0f)) * size;
            }
            previous = codepoint;
        }
        return std::max(width, lineWidth);
    }

    std::vector<TextLine> Font::LayoutLines(const std::string& text, float size, float maxWidth) const
    {
        std::vector<TextLine> lines;
        for (const std::string& paragraph : Core::Split(text, '\n'))
        {
            if (maxWidth <= 0.0f || MeasureWidth(paragraph, size) <= maxWidth)
            {
                lines.push_back({ paragraph, MeasureWidth(paragraph, size) });
                continue;
            }
            std::string current;
            for (const std::string& word : Core::Split(paragraph, ' '))
            {
                std::string candidate = current.empty() ? word : current + " " + word;
                if (!current.empty() && MeasureWidth(candidate, size) > maxWidth)
                {
                    lines.push_back({ current, MeasureWidth(current, size) });
                    current = word;
                }
                else
                {
                    current = candidate;
                }
            }
            lines.push_back({ current, MeasureWidth(current, size) });
        }
        return lines;
    }

    FontLibrary& FontLibrary::Get()
    {
        static FontLibrary* library = new FontLibrary();
        return *library;
    }

    Font* FontLibrary::GetDefault()
    {
        if (m_default || m_defaultFailed)
        {
            return m_default.get();
        }
        std::filesystem::path source = Core::FileSystem::EngineContentDirectory() / "Fonts" / "Roboto-Medium.ttf";
        auto bytes = Core::FileSystem::ReadBinary(source);
        if (!bytes)
        {
            Core::Log::Error("Default UI font is missing: " + source.generic_string());
            m_defaultFailed = true;
            return nullptr;
        }
        uint64_t hash = Core::HashBytes(bytes->data(), bytes->size());
        std::filesystem::path cache = std::filesystem::temp_directory_path() / "BrightBox3D" / ("DefaultFont_" + std::to_string(hash) + ".bin");
        FontData data;
        bool loaded = false;
        if (auto cached = Core::FileSystem::ReadBinary(cache))
        {
            loaded = data.Deserialize(cached->data(), cached->size());
        }
        if (!loaded)
        {
            std::string error;
            if (!GenerateFontData(source, data, error))
            {
                Core::Log::Error("Default UI font generation failed: " + error);
                m_defaultFailed = true;
                return nullptr;
            }
            std::vector<uint8_t> serialized = data.Serialize();
            Core::FileSystem::WriteBinary(cache, serialized.data(), serialized.size());
        }
        m_default = std::make_unique<Font>(std::move(data));
        return m_default.get();
    }

    Font* FontLibrary::Get(const std::string& family)
    {
        if (family.empty())
        {
            return GetDefault();
        }
        auto it = m_fonts.find(family);
        if (it != m_fonts.end())
        {
            return it->second ? it->second.get() : GetDefault();
        }
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        Core::Guid guid = Core::Guid::Parse(family);
        if (!guid.IsValid())
        {
            std::string path = family;
            if (path.rfind("Assets/", 0) == 0)
            {
                path = path.substr(7);
            }
            if (const Assets::AssetRecord* record = database.FindRecordByPath(path))
            {
                guid = record->guid;
            }
        }
        auto asset = database.Load<FontAsset>(guid);
        if (!asset)
        {
            Core::Log::Warning("UI font '" + family + "' was not found; using the default font");
            m_fonts[family] = nullptr;
            return GetDefault();
        }
        FontData copy = asset->GetData();
        copy.BuildIndex();
        m_fonts[family] = std::make_unique<Font>(std::move(copy));
        return m_fonts[family].get();
    }

    void FontLibrary::Clear()
    {
        m_fonts.clear();
    }
}
