#include "IconGlyphs.h"
#include "Serialization/Json.h"

#include <gtest/gtest.h>

#include <string>

using namespace BrightBox3D;

namespace
{
    uint32_t DecodeUtf8(const char* text)
    {
        const auto* bytes = reinterpret_cast<const unsigned char*>(text);
        return ((bytes[0] & 0x0Fu) << 12) | ((bytes[1] & 0x3Fu) << 6) | (bytes[2] & 0x3Fu);
    }

    uint32_t CodepointFromInfo(const Serialization::JsonDocument& info, const char* name)
    {
        std::string encoded = Serialization::GetString(info[name], "encodedCode");
        return static_cast<uint32_t>(std::stoul(encoded.substr(1), nullptr, 16));
    }
}

TEST(IconTests, GeneratedGlyphsMatchLucide)
{
    Serialization::JsonDocument info;
    ASSERT_TRUE(Serialization::LoadJsonFile(BRIGHT_LUCIDE_INFO, info));
    EXPECT_EQ(DecodeUtf8(Editor::IconGlyph(Core::Icon::Flag)), CodepointFromInfo(info, "flag"));
    EXPECT_EQ(DecodeUtf8(Editor::IconGlyph(Core::Icon::LayoutTemplate)), CodepointFromInfo(info, "layout-template"));
    EXPECT_EQ(DecodeUtf8(Editor::IconGlyph(Core::Icon::X)), CodepointFromInfo(info, "x"));
    EXPECT_STREQ(Editor::IconGlyph(Core::Icon::None), "");
    EXPECT_GE(DecodeUtf8(Editor::IconGlyph(Core::Icon::Flag)), Editor::IconRangeMin);
    EXPECT_LE(DecodeUtf8(Editor::IconGlyph(Core::Icon::Flag)), Editor::IconRangeMax);
}
