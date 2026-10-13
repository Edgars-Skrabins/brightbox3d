#pragma once

#include "RHI/Device.h"
#include "UI/FontData.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::UI
{
    struct TextLine
    {
        std::string text;
        float width = 0.0f;
    };

    class Font
    {
    public:
        explicit Font(FontData data) : m_data(std::move(data)) {}

        const FontData& GetData() const { return m_data; }
        RHI::Texture* GetTexture(RHI::Device& device);
        float MeasureWidth(const std::string& text, float size) const;
        std::vector<TextLine> LayoutLines(const std::string& text, float size, float maxWidth) const;
        float LineHeight(float size) const { return m_data.lineHeight * size; }

        static std::vector<uint32_t> Decode(const std::string& text);

    private:
        FontData m_data;
        std::unique_ptr<RHI::Texture> m_texture;
    };

    class FontLibrary
    {
    public:
        static FontLibrary& Get();

        Font* GetDefault();
        Font* Get(const std::string& family);
        void Clear();

    private:
        std::unique_ptr<Font> m_default;
        bool m_defaultFailed = false;
        std::unordered_map<std::string, std::unique_ptr<Font>> m_fonts;
    };
}
