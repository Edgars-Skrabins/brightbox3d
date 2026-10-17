#pragma once

#include "Core/Result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::UI
{
    struct FontGlyph
    {
        uint32_t codepoint = 0;
        float advance = 0.0f;
        float planeLeft = 0.0f;
        float planeBottom = 0.0f;
        float planeRight = 0.0f;
        float planeTop = 0.0f;
        float atlasLeft = 0.0f;
        float atlasBottom = 0.0f;
        float atlasRight = 0.0f;
        float atlasTop = 0.0f;
    };

    struct FontData
    {
        uint32_t atlasWidth = 0;
        uint32_t atlasHeight = 0;
        float pixelRange = 4.0f;
        float ascender = 0.8f;
        float descender = -0.2f;
        float lineHeight = 1.2f;
        std::vector<uint8_t> pixels;
        std::vector<FontGlyph> glyphs;
        std::unordered_map<uint64_t, float> kerning;

        std::vector<uint8_t> Serialize() const;
        bool Deserialize(const uint8_t* data, size_t size);
        const FontGlyph* FindGlyph(uint32_t codepoint) const;
        float GetKerning(uint32_t first, uint32_t second) const;
        void BuildIndex();

    private:
        std::unordered_map<uint32_t, size_t> m_index;
    };

    Core::Result<FontData> GenerateFontData(const std::filesystem::path& file);
}
